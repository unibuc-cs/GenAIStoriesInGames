#include "ContractModel.h"

#include "ContractSerialization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"

FContractModel::FContractModel()
{
	SessionStartSeconds = FPlatformTime::Seconds();
}

// ---------------------------------------------------------------------------
// Graph access
// ---------------------------------------------------------------------------

FStoryNode* FContractModel::FindNode(const FString& NodeId)
{
	for (FStoryNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}
	return nullptr;
}

const FStoryNode* FContractModel::FindNode(const FString& NodeId) const
{
	for (const FStoryNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}
	return nullptr;
}

FStoryChoice* FContractModel::FindChoice(const FString& NodeId, const FString& ChoiceId)
{
	if (FStoryNode* Node = FindNode(NodeId))
	{
		for (FStoryChoice& Choice : Node->Choices)
		{
			if (Choice.ChoiceId == ChoiceId)
			{
				return &Choice;
			}
		}
	}
	return nullptr;
}

TSet<FString> FContractModel::ComputeBranchStateAfterNode(const FString& NodeId) const
{
	// Walk the primary-parent chain up to the root, then replay effects
	// downwards: for each hop apply choice effects (delta_C) then node
	// effects (delta_V).
	TArray<const FStoryNode*> Chain;
	const FStoryNode* Cursor = FindNode(NodeId);
	while (Cursor)
	{
		Chain.Insert(Cursor, 0);
		Cursor = Cursor->PrimaryParentId.IsEmpty() ? nullptr : FindNode(Cursor->PrimaryParentId);
	}

	TSet<FString> State;
	for (int32 i = 0; i < Chain.Num(); ++i)
	{
		const FStoryNode* Node = Chain[i];

		// Apply the incoming choice's effects (recorded on the parent).
		if (i > 0 && !Node->PrimaryIncomingChoiceId.IsEmpty())
		{
			const FStoryNode* Parent = Chain[i - 1];
			for (const FStoryChoice& Choice : Parent->Choices)
			{
				if (Choice.ChoiceId == Node->PrimaryIncomingChoiceId)
				{
					for (const FString& Del : Choice.DelEffects) { State.Remove(Del); }
					for (const FString& Add : Choice.AddEffects) { State.Add(Add); }
					break;
				}
			}
		}

		// Node effects delta_V(S, v).
		for (const FString& Del : Node->DelEffects) { State.Remove(Del); }
		for (const FString& Add : Node->AddEffects) { State.Add(Add); }
	}
	return State;
}

TArray<FString> FContractModel::ChildNodeIds(const FString& NodeId) const
{
	TArray<FString> Result;
	if (const FStoryNode* Node = FindNode(NodeId))
	{
		for (const FStoryChoice& Choice : Node->Choices)
		{
			if (!Choice.TargetNodeId.IsEmpty())
			{
				const FStoryNode* Target = FindNode(Choice.TargetNodeId);
				if (Target && Target->Status != ENodeStatus::Removed)
				{
					Result.AddUnique(Choice.TargetNodeId);
				}
			}
		}
	}
	return Result;
}

TArray<FString> FContractModel::ReachableFrom(const FString& NodeId) const
{
	TArray<FString> Result;
	TArray<FString> Frontier;
	Frontier.Add(NodeId);
	while (Frontier.Num() > 0)
	{
		const FString Current = Frontier.Pop();
		if (Result.Contains(Current))
		{
			continue;
		}
		Result.Add(Current);
		for (const FString& Child : ChildNodeIds(Current))
		{
			Frontier.Add(Child);
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Validation (Eq. 6)
// ---------------------------------------------------------------------------

static bool SetContains(const TSet<FString>& State, const TArray<FString>& Required, FString* OutMissing)
{
	for (const FString& Pred : Required)
	{
		if (!State.Contains(Pred))
		{
			if (OutMissing)
			{
				*OutMissing = Pred;
			}
			return false;
		}
	}
	return true;
}

void FContractModel::Revalidate(const TArray<FString>& ScopeNodeIds)
{
	const bool bWholeGraph = ScopeNodeIds.Num() == 0;

	for (FStoryNode& Node : Nodes)
	{
		if (Node.Status == ENodeStatus::Removed || Node.Status == ENodeStatus::Proposed)
		{
			continue;
		}
		if (!bWholeGraph && !ScopeNodeIds.Contains(Node.NodeId))
		{
			continue; // records outside the closure retain their earlier result
		}

		// State after this node along its incoming path.
		const TSet<FString> StateAfterNode = ComputeBranchStateAfterNode(Node.NodeId);

		// Node-level: grounding.
		if (Node.HasCapabilities())
		{
			if (Node.Grounding == EGroundingStatus::Unresolved)
			{
				Node.Status = ENodeStatus::Unsupported;
			}
			else
			{
				Node.Status = ENodeStatus::Valid;
			}
		}
		else
		{
			Node.Grounding = EGroundingStatus::NotRequired;
			Node.Status = ENodeStatus::Valid;
		}

		// Edge-level: guard after node effects, successor preconditions after
		// choice effects.
		for (FStoryChoice& Choice : Node.Choices)
		{
			Choice.bBlocked = false;
			Choice.BlockDiagnostic.Empty();

			FString Missing;
			if (!SetContains(StateAfterNode, Choice.Guard, &Missing))
			{
				Choice.bBlocked = true;
				Choice.BlockDiagnostic = FString::Printf(TEXT("guard requires %s, absent on this branch"), *Missing);
			}

			if (!Choice.bBlocked && !Choice.TargetNodeId.IsEmpty())
			{
				if (const FStoryNode* Target = FindNode(Choice.TargetNodeId))
				{
					if (Target->Status != ENodeStatus::Removed && Target->Status != ENodeStatus::Proposed)
					{
						TSet<FString> StateAfterChoice = StateAfterNode;
						for (const FString& Del : Choice.DelEffects) { StateAfterChoice.Remove(Del); }
						for (const FString& Add : Choice.AddEffects) { StateAfterChoice.Add(Add); }

						if (!SetContains(StateAfterChoice, Target->Preconditions, &Missing))
						{
							Choice.bBlocked = true;
							Choice.BlockDiagnostic = FString::Printf(
								TEXT("%s requires %s, false after %s; state is branch-local"),
								*Choice.TargetNodeId, *Missing, *Node.NodeId);
						}
					}
				}
			}
		}
	}

	// Second pass: a node whose primary incoming edge is blocked cannot be
	// licensed along its own path — surface it for review.
	for (FStoryNode& Node : Nodes)
	{
		if (Node.Status == ENodeStatus::Removed || Node.Status == ENodeStatus::Proposed
			|| Node.Status == ENodeStatus::Unsupported)
		{
			continue;
		}
		if (Node.PrimaryParentId.IsEmpty())
		{
			continue;
		}
		if (const FStoryNode* Parent = FindNode(Node.PrimaryParentId))
		{
			for (const FStoryChoice& Choice : Parent->Choices)
			{
				if (Choice.ChoiceId == Node.PrimaryIncomingChoiceId && Choice.bBlocked)
				{
					Node.Status = ENodeStatus::NeedsReview;
					break;
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Curves
// ---------------------------------------------------------------------------

void FContractModel::SetApprovedCurveValue(ENarrativeAxis Axis, int32 ControlIndex, float NewValue, const FString& Actor)
{
	const int32 AxisIndex = static_cast<int32>(Axis);
	if (AxisIndex < 0 || AxisIndex >= NumAxes || ControlIndex < 0 || ControlIndex >= NumCurveControlPoints)
	{
		return;
	}

	NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	FTargetCurve& Curve = Curves[AxisIndex];
	if (FMath::IsNearlyEqual(Curve.Approved[ControlIndex], NewValue, 0.0005f))
	{
		return;
	}

	FCurveEditRecord Edit;
	Edit.Axis = Axis;
	Edit.ControlIndex = ControlIndex;
	Edit.OldValue = Curve.Approved[ControlIndex];
	Edit.NewValue = NewValue;
	Edit.Actor = Actor;
	Edit.Timestamp = NowTimestamp();
	CurveEdits.Add(Edit);

	Curve.Approved[ControlIndex] = NewValue;
	Versions.StoryBible++;
	Notify();
}

float FContractModel::ComputePathAdherence(const FString& LeafNodeId) const
{
	// Adh(P) = 1 - (1 / (L * |A|)) * sum_i sum_a |r_a(v_i) - gamma*_a(i/(L-1))|
	TArray<const FStoryNode*> Chain;
	const FStoryNode* Cursor = FindNode(LeafNodeId);
	while (Cursor)
	{
		Chain.Insert(Cursor, 0);
		Cursor = Cursor->PrimaryParentId.IsEmpty() ? nullptr : FindNode(Cursor->PrimaryParentId);
	}

	const int32 L = Chain.Num();
	if (L < 2)
	{
		return 1.f;
	}

	float Total = 0.f;
	for (int32 i = 0; i < L; ++i)
	{
		const float X = static_cast<float>(i) / static_cast<float>(L - 1);
		for (int32 a = 0; a < NumAxes; ++a)
		{
			Total += FMath::Abs(Chain[i]->AxisEstimates[a] - Curves[a].EvalApproved(X));
		}
	}
	return 1.f - Total / (static_cast<float>(L) * static_cast<float>(NumAxes));
}

// ---------------------------------------------------------------------------
// Authorization
// ---------------------------------------------------------------------------

FAuthorizationEpisode* FContractModel::FindEpisode(const FString& EpisodeId)
{
	for (FAuthorizationEpisode& Episode : Episodes)
	{
		if (Episode.EpisodeId == EpisodeId)
		{
			return &Episode;
		}
	}
	return nullptr;
}

EPolicyRoute FContractModel::RouteEpisode(const FAuthorizationEpisode& Episode) const
{
	if (Episode.ResolvedAction != EProposalAction::None)
	{
		return EPolicyRoute::Applied;
	}

	// All policies require review for a new label, placeholder, or
	// implementation commitment, and for gate-invalid proposals.
	const bool bAlwaysReview =
		!Episode.bGateValid
		|| Episode.Profile.bIntroducesNewLabel
		|| Episode.Profile.ImplConsequence == EImplConsequence::Placeholder
		|| Episode.Profile.ImplConsequence == EImplConsequence::NewRequirement;

	if (bAlwaysReview)
	{
		return EPolicyRoute::Review;
	}

	if (Episode.Profile.IsRegisteredLocalNormalization())
	{
		// Automatic and Assisted apply logged registered normalizations;
		// Strict requires review for every consequential change.
		return (Policy == EAuthorizationPolicy::Strict) ? EPolicyRoute::Review : EPolicyRoute::Auto;
	}

	// Meaningful, persistent, reachability-changing, or irreversible:
	// Automatic applies other gate-valid changes; Assisted and Strict pause.
	return (Policy == EAuthorizationPolicy::Automatic) ? EPolicyRoute::Auto : EPolicyRoute::Review;
}

void FContractModel::SetPolicy(EAuthorizationPolicy NewPolicy, const FString& Actor)
{
	if (Policy == NewPolicy)
	{
		return;
	}
	Policy = NewPolicy;
	LogEvent(TEXT("policy_changed"), PolicyDisplayName(NewPolicy));
	RouteAndAutoApply(FString::Printf(TEXT("policy:%s"), PolicyDisplayName(NewPolicy)));
	Notify();
}

void FContractModel::RouteAndAutoApply(const FString& Actor)
{
	// Route everything first.
	for (FAuthorizationEpisode& Episode : Episodes)
	{
		if (Episode.ResolvedAction == EProposalAction::None && Episode.Route != EPolicyRoute::Deferred)
		{
			Episode.Route = RouteEpisode(Episode);
		}
	}

	// Apply the auto-routed ones through the same recorded pathway a person
	// would use, selecting the recommended (or first) option.
	TArray<FString> AutoIds;
	for (const FAuthorizationEpisode& Episode : Episodes)
	{
		if (Episode.Route == EPolicyRoute::Auto && Episode.ResolvedAction == EProposalAction::None)
		{
			AutoIds.Add(Episode.EpisodeId);
		}
	}
	for (const FString& Id : AutoIds)
	{
		FAuthorizationEpisode* Episode = FindEpisode(Id);
		if (!Episode)
		{
			continue;
		}
		FString OptionId;
		for (const FEpisodeOption& Option : Episode->Options)
		{
			if (Option.ActionKind == EProposalAction::Approve)
			{
				OptionId = Option.OptionId;
				if (Option.bRecommended)
				{
					break;
				}
			}
		}
		if (!OptionId.IsEmpty())
		{
			ResolveEpisode(Id, OptionId, EProposalAction::AutoApplied, Actor);
		}
	}
}

void FContractModel::ResolveEpisode(const FString& EpisodeId, const FString& OptionId, EProposalAction Action, const FString& Actor)
{
	FAuthorizationEpisode* Episode = FindEpisode(EpisodeId);
	if (!Episode || Episode->ResolvedAction != EProposalAction::None)
	{
		return;
	}

	// Every applied decision is undoable: capture the state as it stands.
	PushUndoSnapshot(Episode->EpisodeId,
		FString::Printf(TEXT("%s (%s)"), *Episode->Title, ActionDisplayName(Action)));

	FDecisionRecord Record;
	Record.EpisodeId = Episode->EpisodeId;
	Record.EpisodeTitle = Episode->Title;
	Record.Actor = Actor;
	Record.PolicyAtDecision = Policy;
	Record.GraphVersionBefore = Versions.GraphVersion;
	Record.Timestamp = NowTimestamp();

	if (Action == EProposalAction::Defer)
	{
		Episode->Route = EPolicyRoute::Deferred;
		Record.Action = EProposalAction::Defer;
		Record.GraphVersionAfter = Versions.GraphVersion;
		AppendDecision(Record);
		Notify();
		return;
	}

	const FEpisodeOption* Chosen = nullptr;
	for (const FEpisodeOption& Option : Episode->Options)
	{
		if (Option.OptionId == OptionId)
		{
			Chosen = &Option;
			break;
		}
	}

	Record.Action = Action;
	if (Chosen)
	{
		Record.OptionLabel = Chosen->Label;
	}

	if (Action == EProposalAction::Reject || (Chosen && Chosen->ActionKind == EProposalAction::Reject))
	{
		Episode->ResolvedAction = EProposalAction::Reject;
		Episode->ResolvedBy = Actor;
		Episode->Route = EPolicyRoute::Applied;
		Episode->GraphVersionAfter = Versions.GraphVersion;
		Record.GraphVersionAfter = Versions.GraphVersion;
		AppendDecision(Record);
		Notify();
		return;
	}

	if (!Chosen)
	{
		return;
	}

	// Accepted edits create a new contract version and mark dependent
	// validation records for rechecking.
	ApplyMutations(Chosen->Mutations, Record);

	Versions.GraphVersion++;
	Record.GraphVersionAfter = Versions.GraphVersion;

	Episode->ResolvedAction = (Action == EProposalAction::AutoApplied) ? EProposalAction::AutoApplied : Chosen->ActionKind;
	Episode->ResolvedOptionId = Chosen->OptionId;
	Episode->ResolvedBy = Actor;
	Episode->Route = EPolicyRoute::Applied;
	Episode->GraphVersionAfter = Versions.GraphVersion;

	// Scoped revalidation: recompute only the dependency closure.
	LastRevalidationBoundary = Record.RevalidatedNodes;
	Revalidate(Record.RevalidatedNodes);

	AppendDecision(Record);
	Notify();
}

int32 FContractModel::NumPendingEpisodes() const
{
	int32 Count = 0;
	for (const FAuthorizationEpisode& Episode : Episodes)
	{
		if (Episode.ResolvedAction == EProposalAction::None)
		{
			Count++;
		}
	}
	return Count;
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

void FContractModel::ApplyMutations(const TArray<FGraphMutation>& Mutations, FDecisionRecord& Record)
{
	TSet<FString> Touched;

	for (const FGraphMutation& Mutation : Mutations)
	{
		switch (Mutation.Type)
		{
		case EGraphMutationType::InsertNodeBetween:
		{
			FStoryNode NewNode = Mutation.NewNode;
			const FString TargetId = Mutation.TargetNodeId;
			FStoryNode* Target = FindNode(TargetId);
			if (!Target)
			{
				break;
			}
			const FString ParentId = Target->PrimaryParentId;
			FStoryNode* Parent = FindNode(ParentId);

			// Rewire parent's choice to point at the new node.
			if (Parent)
			{
				for (FStoryChoice& Choice : Parent->Choices)
				{
					if (Choice.TargetNodeId == TargetId)
					{
						Choice.TargetNodeId = NewNode.NodeId;
						NewNode.PrimaryIncomingChoiceId = Choice.ChoiceId;
						break;
					}
				}
			}
			NewNode.PrimaryParentId = ParentId;
			NewNode.Status = ENodeStatus::NeedsReview;
			NewNode.Depth = Target->Depth;
			NewNode.Lane = Target->Lane;

			// The new node continues into the original target.
			FStoryChoice Continue;
			Continue.ChoiceId = NewNode.NodeId + TEXT("_continue");
			Continue.Label = FString::Printf(TEXT("Continue to %s"), *TargetId);
			Continue.TargetNodeId = TargetId;
			NewNode.Choices.Add(Continue);

			Target->PrimaryParentId = NewNode.NodeId;
			Target->PrimaryIncomingChoiceId = Continue.ChoiceId;

			// Shift depths of the inserted node's subtree for layout.
			for (const FString& Id : ReachableFrom(TargetId))
			{
				if (FStoryNode* N = FindNode(Id))
				{
					N->Depth++;
				}
			}

			Nodes.Add(NewNode);
			Touched.Add(NewNode.NodeId);
			Touched.Add(TargetId);
			if (!ParentId.IsEmpty())
			{
				// Incident edge: the parent's rewired choice must be rechecked.
				Touched.Add(ParentId);
			}
			break;
		}
		case EGraphMutationType::AppendNodeUnder:
		{
			FStoryNode NewNode = Mutation.NewNode;
			FStoryNode* Parent = FindNode(Mutation.TargetNodeId);
			if (!Parent)
			{
				break;
			}

			FStoryChoice Choice;
			Choice.ChoiceId = FString::Printf(TEXT("c_%s_%s"), *Parent->NodeId, *NewNode.NodeId);
			Choice.Label = Mutation.StringParamA.IsEmpty()
				? FString::Printf(TEXT("Continue to %s"), *NewNode.Title) : Mutation.StringParamA;
			Choice.TargetNodeId = NewNode.NodeId;

			NewNode.PrimaryParentId = Parent->NodeId;
			NewNode.PrimaryIncomingChoiceId = Choice.ChoiceId;
			NewNode.Status = ENodeStatus::NeedsReview;
			NewNode.Depth = Parent->Depth + 1;
			NewNode.Lane = FMath::Clamp(Parent->Lane + Parent->Choices.Num(), 0, 7);

			const FString ParentId = Parent->NodeId;
			Parent->Choices.Add(Choice);
			Nodes.Add(NewNode); // may reallocate: Parent is dead past here

			Touched.Add(ParentId);
			Touched.Add(NewNode.NodeId);
			break;
		}
		case EGraphMutationType::RemovePrecondition:
		{
			if (FStoryNode* Target = FindNode(Mutation.TargetNodeId))
			{
				Target->Preconditions.Remove(Mutation.StringParamA);
				Target->Provenance.Add(FString::Printf(TEXT("precondition weakened: removed %s"), *Mutation.StringParamA));
				Touched.Add(Target->NodeId);
				if (!Target->PrimaryParentId.IsEmpty())
				{
					Touched.Add(Target->PrimaryParentId);
				}
			}
			break;
		}
		case EGraphMutationType::SetChoiceLabel:
		{
			if (FStoryChoice* Choice = FindChoice(Mutation.TargetNodeId, Mutation.StringParamA))
			{
				Choice->Label = Mutation.StringParamB;
				Touched.Add(Mutation.TargetNodeId);
			}
			break;
		}
		case EGraphMutationType::SetGrounding:
		{
			if (FStoryNode* Target = FindNode(Mutation.TargetNodeId))
			{
				Target->Grounding = Mutation.Grounding;
				Target->Provenance.Add(FString::Printf(TEXT("grounding set to %s"), GroundingDisplayName(Mutation.Grounding)));
				Touched.Add(Target->NodeId);
			}
			break;
		}
		case EGraphMutationType::AddCapabilityNeed:
		{
			ImplementationNeeds.Add(Mutation.StringParamA);
			Versions.EngineCapabilityManifest++;
			break;
		}
		case EGraphMutationType::RemoveNode:
		{
			if (FStoryNode* Target = FindNode(Mutation.TargetNodeId))
			{
				Target->Status = ENodeStatus::Removed;
				if (FStoryNode* Parent = FindNode(Target->PrimaryParentId))
				{
					Touched.Add(Parent->NodeId);
				}
			}
			break;
		}
		case EGraphMutationType::AddProvenance:
		{
			if (FStoryNode* Target = FindNode(Mutation.TargetNodeId))
			{
				Target->Provenance.Add(Mutation.StringParamA);
			}
			break;
		}
		default:
			break;
		}
	}

	// Revalidation boundary: Reach_D(Touched) — the touched nodes and every
	// obligation downstream of them.
	TSet<FString> Boundary;
	for (const FString& Id : Touched)
	{
		for (const FString& Reached : ReachableFrom(Id))
		{
			Boundary.Add(Reached);
		}
	}
	Record.RevalidatedNodes = Boundary.Array();
}

// ---------------------------------------------------------------------------
// Live proposal support (v3)
// ---------------------------------------------------------------------------

void FContractModel::AddEpisode(FAuthorizationEpisode Episode)
{
	Episodes.Add(MoveTemp(Episode));
	RouteAndAutoApply(FString::Printf(TEXT("policy:%s"), PolicyDisplayName(Policy)));
	Notify();
}

TSet<FString> FContractModel::CollectLicensedPredicates() const
{
	TSet<FString> Result;
	for (const FStoryNode& Node : Nodes)
	{
		if (Node.Status == ENodeStatus::Removed)
		{
			continue;
		}
		for (const FString& P : Node.Preconditions) { Result.Add(P); }
		for (const FString& P : Node.AddEffects)    { Result.Add(P); }
		for (const FString& P : Node.DelEffects)    { Result.Add(P); }
		for (const FStoryChoice& Choice : Node.Choices)
		{
			for (const FString& P : Choice.Guard)      { Result.Add(P); }
			for (const FString& P : Choice.AddEffects) { Result.Add(P); }
			for (const FString& P : Choice.DelEffects) { Result.Add(P); }
		}
	}
	return Result;
}

void FContractModel::RequestLiveProposal()
{
	if (ProposalMode != EProposalSourceMode::Live)
	{
		return;
	}
	const FString Target = SelectedNodeId.IsEmpty() ? RootNodeId() : SelectedNodeId;
	LogEvent(TEXT("generate_requested"), Target);
	if (LiveProposalRequestHandler)
	{
		LiveProposalRequestHandler(Target);
	}
	else
	{
		LiveStatus = TEXT("No LLM handler bound (missing LLMConfig.ini?)");
		Notify();
	}
}

void FContractModel::RequestFrontierExpansion()
{
	if (ProposalMode != EProposalSourceMode::Live)
	{
		return;
	}
	LogEvent(TEXT("generate_requested"), TEXT("frontier"));
	if (FrontierExpansionHandler)
	{
		FrontierExpansionHandler();
	}
	else
	{
		LiveStatus = TEXT("No LLM handler bound (missing LLMConfig.ini?)");
		Notify();
	}
}

TArray<FString> FContractModel::FrontierNodeIds(int32 MaxChoicesPerNode) const
{
	struct FEntry { FString Id; int32 Depth; };
	TArray<FEntry> Entries;
	for (const FStoryNode& Node : Nodes)
	{
		if (Node.Status == ENodeStatus::Removed || Node.Status == ENodeStatus::Proposed
			|| Node.bEnding || Node.Choices.Num() >= MaxChoicesPerNode)
		{
			continue;
		}
		Entries.Add({Node.NodeId, Node.Depth});
	}
	// Deterministic ordering: shallowest first, node id as the tie-break.
	// (TArray::Sort is unstable, so equal depths would otherwise come out
	// in arbitrary order -- caught by the full-loop scenario.)
	Entries.Sort([](const FEntry& A, const FEntry& B)
	{
		if (A.Depth != B.Depth)
		{
			return A.Depth < B.Depth;
		}
		return A.Id < B.Id;
	});

	TArray<FString> Result;
	for (const FEntry& Entry : Entries)
	{
		Result.Add(Entry.Id);
	}
	return Result;
}

int32 FContractModel::NumLiveNodes() const
{
	int32 Count = 0;
	for (const FStoryNode& Node : Nodes)
	{
		if (Node.Status != ENodeStatus::Removed && Node.Status != ENodeStatus::Proposed)
		{
			Count++;
		}
	}
	return Count;
}

// ---------------------------------------------------------------------------
// Undo (v6)
// ---------------------------------------------------------------------------

void FContractModel::PushUndoSnapshot(const FString& EpisodeId, const FString& Label)
{
	FUndoEntry Entry;
	Entry.EpisodeId = EpisodeId;
	Entry.Label = Label;
	Entry.SnapshotJson = ContractSerialization::SaveToJsonString(*this);
	Entry.Timestamp = NowTimestamp();
	UndoStack.Add(MoveTemp(Entry));
	while (UndoStack.Num() > MaxUndoDepth)
	{
		UndoStack.RemoveAt(0);
	}
}

void FContractModel::UndoLastDecision(const FString& Actor)
{
	if (UndoStack.Num() == 0)
	{
		return;
	}
	const FUndoEntry Entry = UndoStack.Pop();
	const int32 VersionBeforeRollback = Versions.GraphVersion;

	// History is append-only: the log survives the restore and gains a
	// Rollback record; the graph version moves FORWARD.
	TArray<FDecisionRecord> PreservedLog = DecisionLog;
	TArray<FUndoEntry> PreservedStack = UndoStack;
	TArray<FTelemetryEvent> PreservedTelemetry = Telemetry;

	FString Error;
	if (!ContractSerialization::LoadFromJsonString(*this, Entry.SnapshotJson, Error))
	{
		LiveStatus = FString::Printf(TEXT("Undo failed: %s."), *Error);
		Notify();
		return;
	}

	UndoStack = MoveTemp(PreservedStack);
	Telemetry = MoveTemp(PreservedTelemetry);
	DecisionLog = MoveTemp(PreservedLog);
	Versions.GraphVersion = FMath::Max(VersionBeforeRollback, Versions.GraphVersion) + 1;
	LastRevalidationBoundary.Empty();

	FDecisionRecord Record;
	Record.EpisodeId = Entry.EpisodeId;
	Record.EpisodeTitle = FString::Printf(TEXT("Rollback: %s"), *Entry.Label);
	Record.Action = EProposalAction::Rollback;
	Record.Actor = Actor;
	Record.PolicyAtDecision = Policy;
	Record.GraphVersionBefore = VersionBeforeRollback;
	Record.GraphVersionAfter = Versions.GraphVersion;
	Record.Timestamp = NowTimestamp();
	AppendDecision(Record);

	LogEvent(TEXT("undo"), Entry.EpisodeId, Entry.Label);
	LiveStatus = FString::Printf(TEXT("Rolled back '%s' -> graph v%d."), *Entry.Label, Versions.GraphVersion);
	Notify();
}

// ---------------------------------------------------------------------------
// Telemetry (v6)
// ---------------------------------------------------------------------------

void FContractModel::LogEvent(const FString& Type, const FString& Target, const FString& Detail)
{
	FTelemetryEvent Event;
	Event.Type = Type;
	Event.Target = Target;
	Event.Detail = Detail;
	Event.SessionSeconds = FPlatformTime::Seconds() - SessionStartSeconds;
	Event.Timestamp = NowTimestamp();
	Telemetry.Add(MoveTemp(Event));
	ExportTelemetryJson();
}

void FContractModel::ExportTelemetryJson() const
{
	FString Out = TEXT("[\n");
	for (int32 i = 0; i < Telemetry.Num(); ++i)
	{
		const FTelemetryEvent& E = Telemetry[i];
		Out += FString::Printf(
			TEXT("  {\"t\":%.3f,\"type\":\"%s\",\"target\":\"%s\",\"detail\":\"%s\",\"time\":\"%s\"}%s\n"),
			E.SessionSeconds, *E.Type, *E.Target, *E.Detail.Replace(TEXT("\""), TEXT("'")), *E.Timestamp,
			(i + 1 < Telemetry.Num()) ? TEXT(",") : TEXT(""));
	}
	Out += TEXT("]\n");
	FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("SessionTelemetry.json")));
}

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------

void FContractModel::AppendDecision(FDecisionRecord Record)
{
	LogEvent(TEXT("decision"), Record.EpisodeId,
		FString::Printf(TEXT("%s by %s"), ActionDisplayName(Record.Action), *Record.Actor));
	DecisionLog.Add(MoveTemp(Record));
	ExportDecisionLogJson();
}

FString FContractModel::NowTimestamp()
{
	return FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
}

void FContractModel::ExportDecisionLogJson() const
{
	FString Out = TEXT("[\n");
	for (int32 i = 0; i < DecisionLog.Num(); ++i)
	{
		const FDecisionRecord& R = DecisionLog[i];
		FString Nodes_ = FString::JoinBy(R.RevalidatedNodes, TEXT(","), [](const FString& S) { return FString::Printf(TEXT("\"%s\""), *S); });
		Out += FString::Printf(
			TEXT("  {\"episode\":\"%s\",\"title\":\"%s\",\"action\":\"%s\",\"option\":\"%s\",\"actor\":\"%s\",\"policy\":\"%s\",\"graph_before\":%d,\"graph_after\":%d,\"revalidated\":[%s],\"time\":\"%s\"}%s\n"),
			*R.EpisodeId, *R.EpisodeTitle, ActionDisplayName(R.Action), *R.OptionLabel, *R.Actor,
			PolicyDisplayName(R.PolicyAtDecision), R.GraphVersionBefore, R.GraphVersionAfter,
			*Nodes_, *R.Timestamp,
			(i + 1 < DecisionLog.Num()) ? TEXT(",") : TEXT(""));
	}
	Out += TEXT("]\n");

	const FString Path = FPaths::ProjectSavedDir() / TEXT("DecisionLog.json");
	FFileHelper::SaveStringToFile(Out, *Path);
}
