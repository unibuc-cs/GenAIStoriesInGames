#include "LlmProposals.h"

#include "ContractModel.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString JoinSet(const TSet<FString>& Set)
	{
		TArray<FString> Arr = Set.Array();
		Arr.Sort();
		return FString::Join(Arr, TEXT(", "));
	}

	void ReadStringArray(const TSharedPtr<FJsonObject>& Obj, const FString& Field, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj->TryGetArrayField(Field, Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S))
				{
					S.TrimStartAndEndInline();
					if (!S.IsEmpty())
					{
						Out.Add(S);
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Prompts
// ---------------------------------------------------------------------------

FString LlmProposals::BuildSystemPrompt(const FContractModel& Model)
{
	TArray<FString> RegisteredCaps;
	for (const FCapabilityRecord& Rec : Model.Manifest)
	{
		if (Rec.bRegistered)
		{
			RegisteredCaps.Add(Rec.CapabilityId);
		}
	}

	FString Out;
	Out += TEXT("You are the bounded story-event generator inside a Narrative Capability Contract authoring tool. ");
	Out += TEXT("You propose exactly ONE successor event node for a branching mystery story. ");
	Out += TEXT("You must respect the contract:\n");
	Out += FString::Printf(TEXT("- Story: %s (genre: %s).\n"), *Model.StoryTitle, *Model.GenreLabel);
	Out += FString::Printf(TEXT("- Licensed predicates (STRONGLY prefer these; each NEW predicate is a reviewed commitment): %s.\n"),
		*JoinSet(Model.CollectLicensedPredicates()));
	Out += FString::Printf(TEXT("- Registered engine capabilities (anything else becomes a placeholder decision): %s.\n"),
		*FString::Join(RegisteredCaps, TEXT(", ")));
	Out += TEXT("- Preconditions must be satisfiable in the given branch state; effects use predicate(argument) form.\n");
	Out += TEXT("- Axis estimates are floats in [0,1] for valence, tension, agency, information, stakes.\n\n");
	Out += TEXT("Reply with STRICT JSON only (no prose, no markdown), of the shape {\"candidates\": [c1, c2, c3]} ");
	Out += TEXT("with exactly THREE distinct candidate objects. Each candidate has fields: ");
	Out += TEXT("title (string, <=5 words), description (string, 1-2 sentences), choice_label (string, the player choice leading here), ");
	Out += TEXT("preconditions (string[]), add_effects (string[]), del_effects (string[]), required_capabilities (string[]), ");
	Out += TEXT("axis_estimates (object with valence, tension, agency, information, stakes), is_ending (bool), rationale (string).");
	return Out;
}

FString LlmProposals::BuildUserPrompt(const FContractModel& Model, const FString& ExpansionNodeId)
{
	const FStoryNode* Node = Model.FindNode(ExpansionNodeId);
	if (!Node)
	{
		return FString();
	}

	TArray<FString> State = Model.ComputeBranchStateAfterNode(ExpansionNodeId).Array();
	State.Sort();

	const int32 GraphDepthMax = 5;
	const float X = FMath::Clamp(static_cast<float>(Node->Depth + 1) / static_cast<float>(GraphDepthMax), 0.f, 1.f);
	FString Targets;
	for (int32 a = 0; a < NumAxes; ++a)
	{
		Targets += FString::Printf(TEXT("%s=%.2f "), AxisDisplayName(static_cast<ENarrativeAxis>(a)),
			Model.Curves[a].EvalApproved(X));
	}

	FString Out;
	Out += FString::Printf(TEXT("Expansion node %s '%s': %s\n"), *Node->NodeId, *Node->Title, *Node->Description);
	Out += FString::Printf(TEXT("Branch state after this node: %s\n"), *FString::Join(State, TEXT(", ")));
	Out += FString::Printf(TEXT("Approved target-curve values at the successor position (x=%.2f): %s\n"), X, *Targets);
	Out += TEXT("Existing outgoing choices: ");
	for (const FStoryChoice& Choice : Node->Choices)
	{
		Out += FString::Printf(TEXT("[%s -> %s] "), *Choice.Label, *Choice.TargetNodeId);
	}
	Out += TEXT("\nPropose THREE distinct new successor events that fit the target curves, take different narrative directions, and do not duplicate existing choices.");
	return Out;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

namespace
{
	// Tolerate markdown fences and leading prose: cut to the outermost
	// JSON object and deserialize it.
	TSharedPtr<FJsonObject> ExtractJsonObject(const FString& Content, FString& OutError)
	{
		FString Json = Content;
		const int32 First = Json.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		const int32 Last = Json.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (First == INDEX_NONE || Last == INDEX_NONE || Last <= First)
		{
			OutError = TEXT("reply contains no JSON object");
			return nullptr;
		}
		Json = Json.Mid(First, Last - First + 1);

		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
		{
			OutError = TEXT("reply is not valid JSON");
			return nullptr;
		}
		return Obj;
	}

	bool ProposalFromJsonObject(const TSharedPtr<FJsonObject>& Obj, FLlmProposal& OutProposal, FString& OutError);
}

bool LlmProposals::ParseProposalJson(const FString& Content, FLlmProposal& OutProposal, FString& OutError)
{
	const TSharedPtr<FJsonObject> Obj = ExtractJsonObject(Content, OutError);
	if (!Obj.IsValid())
	{
		return false;
	}
	return ProposalFromJsonObject(Obj, OutProposal, OutError);
}

bool LlmProposals::ParseCandidatesJson(const FString& Content, TArray<FLlmProposal>& OutProposals, FString& OutError)
{
	const TSharedPtr<FJsonObject> Obj = ExtractJsonObject(Content, OutError);
	if (!Obj.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
	if (Obj->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates)
	{
		for (const TSharedPtr<FJsonValue>& V : *Candidates)
		{
			const TSharedPtr<FJsonObject>* CandidateObj = nullptr;
			if (V.IsValid() && V->TryGetObject(CandidateObj) && CandidateObj && CandidateObj->IsValid())
			{
				FLlmProposal Proposal;
				FString CandidateError;
				if (ProposalFromJsonObject(*CandidateObj, Proposal, CandidateError))
				{
					OutProposals.Add(MoveTemp(Proposal));
				}
			}
		}
		if (OutProposals.Num() == 0)
		{
			OutError = TEXT("candidates array holds no valid candidate");
			return false;
		}
		return true;
	}

	// Single-object fallback (a model that ignored the candidates shape).
	FLlmProposal Proposal;
	if (ProposalFromJsonObject(Obj, Proposal, OutError))
	{
		OutProposals.Add(MoveTemp(Proposal));
		return true;
	}
	return false;
}

float LlmProposals::ScoreProposal(const FContractModel& Model, const FString& ExpansionNodeId, const FLlmProposal& Proposal)
{
	const FStoryNode* Node = Model.FindNode(ExpansionNodeId);
	const int32 GraphDepthMax = 5;
	const float X = Node
		? FMath::Clamp(static_cast<float>(Node->Depth + 1) / static_cast<float>(GraphDepthMax), 0.f, 1.f)
		: 0.5f;

	// Target fit: 1 - mean absolute distance from the approved curves.
	float Distance = 0.f;
	for (int32 a = 0; a < NumAxes; ++a)
	{
		Distance += FMath::Abs(Proposal.AxisEstimates[a] - Model.Curves[a].EvalApproved(X));
	}
	const float Fit = 1.f - Distance / static_cast<float>(NumAxes);

	// Implementability: every requested capability registered.
	float Impl = 1.f;
	for (const FString& Cap : Proposal.RequiredCapabilities)
	{
		bool bRegistered = false;
		for (const FCapabilityRecord& Rec : Model.Manifest)
		{
			if (Rec.CapabilityId == Cap && Rec.bRegistered)
			{
				bRegistered = true;
				break;
			}
		}
		if (!bRegistered)
		{
			Impl = 0.f;
			break;
		}
	}

	// Licensing: predicates inside the licensed vocabulary.
	const TSet<FString> Licensed = Model.CollectLicensedPredicates();
	float License = 1.f;
	for (const FString& P : Proposal.AddEffects)
	{
		if (!Licensed.Contains(P))
		{
			License = 0.5f;
			break;
		}
	}

	return 0.5f * Fit + 0.3f * Impl + 0.2f * License;
}

namespace
{
bool ProposalFromJsonObject(const TSharedPtr<FJsonObject>& Obj, FLlmProposal& OutProposal, FString& OutError)
{
	if (!Obj->TryGetStringField(TEXT("title"), OutProposal.Title) || OutProposal.Title.IsEmpty())
	{
		OutError = TEXT("missing required field: title");
		return false;
	}
	Obj->TryGetStringField(TEXT("description"), OutProposal.Description);
	Obj->TryGetStringField(TEXT("choice_label"), OutProposal.ChoiceLabel);
	Obj->TryGetStringField(TEXT("rationale"), OutProposal.Rationale);
	Obj->TryGetBoolField(TEXT("is_ending"), OutProposal.bEnding);

	ReadStringArray(Obj, TEXT("preconditions"), OutProposal.Preconditions);
	ReadStringArray(Obj, TEXT("add_effects"), OutProposal.AddEffects);
	ReadStringArray(Obj, TEXT("del_effects"), OutProposal.DelEffects);
	ReadStringArray(Obj, TEXT("required_capabilities"), OutProposal.RequiredCapabilities);

	const TSharedPtr<FJsonObject>* Axes = nullptr;
	if (Obj->TryGetObjectField(TEXT("axis_estimates"), Axes) && Axes && Axes->IsValid())
	{
		const TCHAR* Keys[NumAxes] = {TEXT("valence"), TEXT("tension"), TEXT("agency"), TEXT("information"), TEXT("stakes")};
		for (int32 a = 0; a < NumAxes; ++a)
		{
			double V = 0.5;
			if ((*Axes)->TryGetNumberField(Keys[a], V))
			{
				OutProposal.AxisEstimates[a] = FMath::Clamp(static_cast<float>(V), 0.f, 1.f);
			}
		}
	}

	if (OutProposal.ChoiceLabel.IsEmpty())
	{
		OutProposal.ChoiceLabel = FString::Printf(TEXT("Continue: %s"), *OutProposal.Title);
	}
	return true;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
// Extractor + validators -> authorization episode
// ---------------------------------------------------------------------------

FAuthorizationEpisode LlmProposals::BuildEpisode(FContractModel& Model, const FString& ExpansionNodeId, const FLlmProposal& Proposal,
	const TArray<FString>& RankedAlternatives)
{
	Model.LiveEpisodeCounter++;
	const FString NewNodeId = FString::Printf(TEXT("L%d"), Model.LiveEpisodeCounter);

	FAuthorizationEpisode E;
	E.EpisodeId = FString::Printf(TEXT("EL%d"), Model.LiveEpisodeCounter);
	E.Title = FString::Printf(TEXT("Live proposal %s '%s' under %s"), *NewNodeId, *Proposal.Title, *ExpansionNodeId);
	E.ProposalClass = TEXT("Generated");
	E.ProposalText = FString::Printf(TEXT("%s -- %s"), *Proposal.Title, *Proposal.Description);
	E.TargetNodeId = ExpansionNodeId;

	// --- Domain validator: new predicates are new-label commitments ------
	const TSet<FString> Licensed = Model.CollectLicensedPredicates();
	TArray<FString> NewPredicates;
	for (const FString& P : Proposal.AddEffects)
	{
		if (!Licensed.Contains(P))
		{
			NewPredicates.AddUnique(P);
		}
	}
	for (const FString& P : Proposal.Preconditions)
	{
		if (!Licensed.Contains(P))
		{
			NewPredicates.AddUnique(P);
		}
	}

	// --- Manifest validator: capability grounding ------------------------
	TArray<FString> UnregisteredCaps;
	TMap<FString, FString> Mapping;
	for (const FString& Cap : Proposal.RequiredCapabilities)
	{
		const FCapabilityRecord* Found = nullptr;
		for (const FCapabilityRecord& Rec : Model.Manifest)
		{
			if (Rec.CapabilityId == Cap && Rec.bRegistered)
			{
				Found = &Rec;
				break;
			}
		}
		if (Found)
		{
			Mapping.Add(Cap, Found->ImplementationClass);
		}
		else
		{
			UnregisteredCaps.AddUnique(Cap);
		}
	}

	// --- State validator: preconditions against the branch state ---------
	const TSet<FString> BranchState = Model.ComputeBranchStateAfterNode(ExpansionNodeId);
	TArray<FString> MissingPreconditions;
	for (const FString& P : Proposal.Preconditions)
	{
		if (!BranchState.Contains(P))
		{
			MissingPreconditions.Add(P);
		}
	}

	E.bGateValid = MissingPreconditions.Num() == 0 && UnregisteredCaps.Num() == 0;
	if (MissingPreconditions.Num() > 0)
	{
		E.Diagnostic = FString::Printf(TEXT("precondition %s is false after %s; state is branch-local"),
			*MissingPreconditions[0], *ExpansionNodeId);
	}
	else if (UnregisteredCaps.Num() > 0)
	{
		E.Diagnostic = FString::Printf(TEXT("%s has no registered implementation in the manifest"),
			*UnregisteredCaps[0]);
	}
	else if (NewPredicates.Num() > 0)
	{
		E.Diagnostic = FString::Printf(TEXT("introduces %d unlicensed predicate(s): %s"),
			NewPredicates.Num(), *FString::Join(NewPredicates, TEXT(", ")));
	}

	// --- Commitment profile ----------------------------------------------
	E.Profile.Reversibility = EReversibility::Reversible; // an un-accepted insert is undoable by rejection
	E.Profile.Scope = EDependencyScope::Branch;
	E.Profile.bChangesNarrativeMeaning = true;
	E.Profile.bChangesReachability = true;
	E.Profile.bChangesPersistentState = Proposal.AddEffects.Num() > 0 || Proposal.DelEffects.Num() > 0;
	E.Profile.bIntroducesNewLabel = NewPredicates.Num() > 0;
	E.Profile.ImplConsequence =
		UnregisteredCaps.Num() > 0 ? EImplConsequence::Placeholder :
		Mapping.Num() > 0 ? EImplConsequence::RegisteredMapping : EImplConsequence::None;

	// --- Evidence package -------------------------------------------------
	TArray<FString> StateLines = BranchState.Array();
	StateLines.Sort();
	E.Evidence.BranchState = StateLines;
	E.Evidence.AffectedRegion = {ExpansionNodeId, NewNodeId};
	for (const auto& Pair : Mapping)
	{
		E.Evidence.MappingLines.Add(FString::Printf(TEXT("%s -> %s"), *Pair.Key, *Pair.Value));
	}
	for (const FString& Cap : UnregisteredCaps)
	{
		E.Evidence.MappingLines.Add(FString::Printf(TEXT("%s -> no registered implementation"), *Cap));
	}
	E.Evidence.ProvenanceLines = {
		FString::Printf(TEXT("live model proposal under contract C(L%d, D%d, M%d, B%d)"),
			Model.Versions.CoreNarrativeLibrary, Model.Versions.DomainNarrativeProfile,
			Model.Versions.EngineCapabilityManifest, Model.Versions.StoryBible)};
	if (!Proposal.Rationale.IsEmpty())
	{
		E.Evidence.ProvenanceLines.Add(FString::Printf(TEXT("model rationale: %s"), *Proposal.Rationale));
	}
	for (const FString& P : NewPredicates)
	{
		E.Evidence.ProvenanceLines.Add(FString::Printf(TEXT("unlicensed predicate: %s"), *P));
	}
	E.Evidence.RevalidationChecks = {
		FString::Printf(TEXT("recheck %s and the new edge"), *ExpansionNodeId),
		TEXT("re-run path-fit measures on the affected spine")};

	// --- The proposed node ------------------------------------------------
	FStoryNode NewNode;
	NewNode.NodeId = NewNodeId;
	NewNode.Title = Proposal.Title;
	NewNode.Description = Proposal.Description;
	NewNode.Preconditions = Proposal.Preconditions;
	NewNode.AddEffects = Proposal.AddEffects;
	NewNode.DelEffects = Proposal.DelEffects;
	NewNode.RequiredCapabilities = Proposal.RequiredCapabilities;
	NewNode.SelectedMapping = Mapping;
	NewNode.bEnding = Proposal.bEnding;
	for (int32 a = 0; a < NumAxes; ++a)
	{
		NewNode.AxisEstimates[a] = Proposal.AxisEstimates[a];
	}
	NewNode.Grounding =
		Proposal.RequiredCapabilities.Num() == 0 ? EGroundingStatus::NotRequired :
		UnregisteredCaps.Num() == 0 ? EGroundingStatus::Implemented : EGroundingStatus::Unresolved;
	NewNode.Provenance = {FString::Printf(TEXT("generated live at graph v%d"), Model.Versions.GraphVersion)};
	if (!Proposal.Rationale.IsEmpty())
	{
		NewNode.Provenance.Add(FString::Printf(TEXT("rationale: %s"), *Proposal.Rationale));
	}

	// --- Bounded options --------------------------------------------------
	if (E.bGateValid || UnregisteredCaps.Num() > 0)
	{
		FEpisodeOption Accept;
		Accept.OptionId = E.EpisodeId + TEXT("_accept");
		Accept.ActionKind = EProposalAction::Approve;
		Accept.bRecommended = E.bGateValid && NewPredicates.Num() == 0;
		if (UnregisteredCaps.Num() > 0)
		{
			Accept.Label = FString::Printf(TEXT("Approve with visible placeholder (%s)"), *FString::Join(UnregisteredCaps, TEXT(", ")));
			Accept.Description = TEXT("Admits the node; unregistered capabilities become an approved, explicitly marked placeholder.");
			FStoryNode PlaceholderNode = NewNode;
			PlaceholderNode.Grounding = EGroundingStatus::ApprovedPlaceholder;
			FGraphMutation M;
			M.Type = EGraphMutationType::AppendNodeUnder;
			M.TargetNodeId = ExpansionNodeId;
			M.StringParamA = Proposal.ChoiceLabel;
			M.NewNode = PlaceholderNode;
			Accept.Mutations.Add(M);
			for (const FString& Cap : UnregisteredCaps)
			{
				FGraphMutation Need;
				Need.Type = EGraphMutationType::AddCapabilityNeed;
				Need.StringParamA = FString::Printf(TEXT("%s for %s (live proposal)"), *Cap, *NewNodeId);
				Accept.Mutations.Add(Need);
			}
		}
		else
		{
			Accept.Label = FString::Printf(TEXT("Approve: append %s '%s'"), *NewNodeId, *Proposal.Title);
			Accept.Description = FString::Printf(TEXT("Adds the node under %s via choice '%s'."), *ExpansionNodeId, *Proposal.ChoiceLabel);
			FGraphMutation M;
			M.Type = EGraphMutationType::AppendNodeUnder;
			M.TargetNodeId = ExpansionNodeId;
			M.StringParamA = Proposal.ChoiceLabel;
			M.NewNode = NewNode;
			Accept.Mutations.Add(M);
		}
		E.Options.Add(Accept);
	}

	FEpisodeOption Reject;
	Reject.OptionId = E.EpisodeId + TEXT("_reject");
	Reject.Label = TEXT("Reject the proposal");
	Reject.Description = TEXT("No graph change; the candidate and this decision remain in the log.");
	Reject.ActionKind = EProposalAction::Reject;
	Reject.bRecommended = !E.bGateValid && UnregisteredCaps.Num() == 0;
	E.Options.Add(Reject);

	E.Evidence.AlternativeLines.Empty();
	for (const FEpisodeOption& Option : E.Options)
	{
		E.Evidence.AlternativeLines.Add(Option.Label);
	}
	// Scored sibling candidates from the same generation round (bounded
	// causal review of the top-k, Algorithm 1).
	for (const FString& Line : RankedAlternatives)
	{
		E.Evidence.AlternativeLines.Add(Line);
	}

	return E;
}

FAuthorizationEpisode LlmProposals::RankAndBuildEpisode(FContractModel& Model, const FString& ExpansionNodeId,
	const TArray<FLlmProposal>& Candidates, int32& OutBestIndex, float& OutBestScore)
{
	check(Candidates.Num() > 0);

	TArray<float> Scores;
	Scores.SetNum(Candidates.Num());
	TArray<int32> Order;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Scores[i] = ScoreProposal(Model, ExpansionNodeId, Candidates[i]);
		Order.Add(i);
	}
	Order.Sort([&Scores](int32 A, int32 B) { return Scores[A] > Scores[B]; });

	TArray<FString> RankedLines;
	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		const int32 Index = Order[Rank];
		RankedLines.Add(FString::Printf(TEXT("rank %d: '%s' score %.2f%s"),
			Rank + 1, *Candidates[Index].Title, Scores[Index],
			Rank == 0 ? TEXT(" (this proposal)") : TEXT(" (not queued)")));
	}

	OutBestIndex = Order[0];
	OutBestScore = Scores[Order[0]];
	return BuildEpisode(Model, ExpansionNodeId, Candidates[Order[0]], RankedLines);
}
