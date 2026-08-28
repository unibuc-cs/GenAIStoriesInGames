#include "ContractSerialization.h"

#include "ContractModel.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

namespace
{
	// ------------------------------------------------------------------ util
	TArray<TSharedPtr<FJsonValue>> ToJsonArray(const TArray<FString>& Arr)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FString& S : Arr)
		{
			Out.Add(MakeShared<FJsonValueString>(S));
		}
		return Out;
	}

	TArray<FString> FromJsonArray(const TSharedPtr<FJsonObject>& Obj, const FString& Field)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj->TryGetArrayField(Field, Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S))
				{
					Out.Add(S);
				}
			}
		}
		return Out;
	}

	TArray<TSharedPtr<FJsonValue>> FloatsToJson(const float* Values, int32 Count)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (int32 i = 0; i < Count; ++i)
		{
			Out.Add(MakeShared<FJsonValueNumber>(Values[i]));
		}
		return Out;
	}

	void JsonToFloats(const TSharedPtr<FJsonObject>& Obj, const FString& Field, float* Values, int32 Count)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj->TryGetArrayField(Field, Arr) && Arr)
		{
			for (int32 i = 0; i < Count && i < Arr->Num(); ++i)
			{
				double D = 0.0;
				if ((*Arr)[i].IsValid() && (*Arr)[i]->TryGetNumber(D))
				{
					Values[i] = static_cast<float>(D);
				}
			}
		}
	}

	int32 GetInt(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 Default = 0)
	{
		double D = 0.0;
		return Obj->TryGetNumberField(Field, D) ? static_cast<int32>(D) : Default;
	}

	bool GetBool(const TSharedPtr<FJsonObject>& Obj, const FString& Field, bool bDefault = false)
	{
		bool B = bDefault;
		Obj->TryGetBoolField(Field, B);
		return B;
	}

	FString GetStr(const TSharedPtr<FJsonObject>& Obj, const FString& Field)
	{
		FString S;
		Obj->TryGetStringField(Field, S);
		return S;
	}

	// ------------------------------------------------------------------ node
	TSharedRef<FJsonObject> ChoiceToJson(const FStoryChoice& Choice)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Choice.ChoiceId);
		Obj->SetStringField(TEXT("label"), Choice.Label);
		Obj->SetArrayField(TEXT("guard"), ToJsonArray(Choice.Guard));
		Obj->SetArrayField(TEXT("add"), ToJsonArray(Choice.AddEffects));
		Obj->SetArrayField(TEXT("del"), ToJsonArray(Choice.DelEffects));
		Obj->SetStringField(TEXT("target"), Choice.TargetNodeId);
		return Obj;
	}

	FStoryChoice ChoiceFromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FStoryChoice Choice;
		Choice.ChoiceId = GetStr(Obj, TEXT("id"));
		Choice.Label = GetStr(Obj, TEXT("label"));
		Choice.Guard = FromJsonArray(Obj, TEXT("guard"));
		Choice.AddEffects = FromJsonArray(Obj, TEXT("add"));
		Choice.DelEffects = FromJsonArray(Obj, TEXT("del"));
		Choice.TargetNodeId = GetStr(Obj, TEXT("target"));
		return Choice;
	}

	TSharedRef<FJsonObject> NodeToJson(const FStoryNode& Node)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Node.NodeId);
		Obj->SetStringField(TEXT("title"), Node.Title);
		Obj->SetStringField(TEXT("description"), Node.Description);
		Obj->SetArrayField(TEXT("entities"), ToJsonArray(Node.Entities));
		Obj->SetArrayField(TEXT("preconditions"), ToJsonArray(Node.Preconditions));
		Obj->SetArrayField(TEXT("add"), ToJsonArray(Node.AddEffects));
		Obj->SetArrayField(TEXT("del"), ToJsonArray(Node.DelEffects));
		TArray<TSharedPtr<FJsonValue>> Choices;
		for (const FStoryChoice& Choice : Node.Choices)
		{
			Choices.Add(MakeShared<FJsonValueObject>(ChoiceToJson(Choice)));
		}
		Obj->SetArrayField(TEXT("choices"), Choices);
		Obj->SetArrayField(TEXT("axes"), FloatsToJson(Node.AxisEstimates, NumAxes));
		Obj->SetArrayField(TEXT("bindings"), ToJsonArray(Node.GameplayBindings));
		Obj->SetArrayField(TEXT("capabilities"), ToJsonArray(Node.RequiredCapabilities));
		// Stored as an array of pairs rather than a JSON object: iterating
		// FJsonObject::Values relies on engine-private key storage that
		// changed in UE 5.8, while arrays go through stable public APIs.
		TArray<TSharedPtr<FJsonValue>> Mapping;
		for (const auto& Pair : Node.SelectedMapping)
		{
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("capability"), Pair.Key);
			Entry->SetStringField(TEXT("implementation"), Pair.Value);
			Mapping.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Obj->SetArrayField(TEXT("mapping"), Mapping);
		Obj->SetArrayField(TEXT("provenance"), ToJsonArray(Node.Provenance));
		Obj->SetNumberField(TEXT("grounding"), static_cast<int32>(Node.Grounding));
		Obj->SetNumberField(TEXT("status"), static_cast<int32>(Node.Status));
		Obj->SetBoolField(TEXT("ending"), Node.bEnding);
		Obj->SetStringField(TEXT("primary_parent"), Node.PrimaryParentId);
		Obj->SetStringField(TEXT("primary_choice"), Node.PrimaryIncomingChoiceId);
		Obj->SetNumberField(TEXT("depth"), Node.Depth);
		Obj->SetNumberField(TEXT("lane"), Node.Lane);
		return Obj;
	}

	FStoryNode NodeFromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FStoryNode Node;
		Node.NodeId = GetStr(Obj, TEXT("id"));
		Node.Title = GetStr(Obj, TEXT("title"));
		Node.Description = GetStr(Obj, TEXT("description"));
		Node.Entities = FromJsonArray(Obj, TEXT("entities"));
		Node.Preconditions = FromJsonArray(Obj, TEXT("preconditions"));
		Node.AddEffects = FromJsonArray(Obj, TEXT("add"));
		Node.DelEffects = FromJsonArray(Obj, TEXT("del"));
		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (Obj->TryGetArrayField(TEXT("choices"), Choices) && Choices)
		{
			for (const TSharedPtr<FJsonValue>& V : *Choices)
			{
				const TSharedPtr<FJsonObject>* ChoiceObj = nullptr;
				if (V->TryGetObject(ChoiceObj) && ChoiceObj && ChoiceObj->IsValid())
				{
					Node.Choices.Add(ChoiceFromJson(*ChoiceObj));
				}
			}
		}
		JsonToFloats(Obj, TEXT("axes"), Node.AxisEstimates, NumAxes);
		Node.GameplayBindings = FromJsonArray(Obj, TEXT("bindings"));
		Node.RequiredCapabilities = FromJsonArray(Obj, TEXT("capabilities"));
		const TArray<TSharedPtr<FJsonValue>>* Mapping = nullptr;
		if (Obj->TryGetArrayField(TEXT("mapping"), Mapping) && Mapping)
		{
			for (const TSharedPtr<FJsonValue>& V : *Mapping)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				if (V.IsValid() && V->TryGetObject(Entry) && Entry && Entry->IsValid())
				{
					const FString Capability = GetStr(*Entry, TEXT("capability"));
					const FString Implementation = GetStr(*Entry, TEXT("implementation"));
					if (!Capability.IsEmpty())
					{
						Node.SelectedMapping.Add(Capability, Implementation);
					}
				}
			}
		}
		Node.Provenance = FromJsonArray(Obj, TEXT("provenance"));
		Node.Grounding = static_cast<EGroundingStatus>(GetInt(Obj, TEXT("grounding")));
		Node.Status = static_cast<ENodeStatus>(GetInt(Obj, TEXT("status")));
		Node.bEnding = GetBool(Obj, TEXT("ending"));
		Node.PrimaryParentId = GetStr(Obj, TEXT("primary_parent"));
		Node.PrimaryIncomingChoiceId = GetStr(Obj, TEXT("primary_choice"));
		Node.Depth = GetInt(Obj, TEXT("depth"));
		Node.Lane = GetInt(Obj, TEXT("lane"));
		return Node;
	}

	// -------------------------------------------------------------- episodes
	TSharedRef<FJsonObject> MutationToJson(const FGraphMutation& Mutation)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("type"), static_cast<int32>(Mutation.Type));
		Obj->SetStringField(TEXT("target"), Mutation.TargetNodeId);
		Obj->SetStringField(TEXT("a"), Mutation.StringParamA);
		Obj->SetStringField(TEXT("b"), Mutation.StringParamB);
		Obj->SetNumberField(TEXT("grounding"), static_cast<int32>(Mutation.Grounding));
		if (!Mutation.NewNode.NodeId.IsEmpty())
		{
			Obj->SetObjectField(TEXT("node"), NodeToJson(Mutation.NewNode));
		}
		return Obj;
	}

	FGraphMutation MutationFromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FGraphMutation Mutation;
		Mutation.Type = static_cast<EGraphMutationType>(GetInt(Obj, TEXT("type")));
		Mutation.TargetNodeId = GetStr(Obj, TEXT("target"));
		Mutation.StringParamA = GetStr(Obj, TEXT("a"));
		Mutation.StringParamB = GetStr(Obj, TEXT("b"));
		Mutation.Grounding = static_cast<EGroundingStatus>(GetInt(Obj, TEXT("grounding")));
		const TSharedPtr<FJsonObject>* NodeObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("node"), NodeObj) && NodeObj && NodeObj->IsValid())
		{
			Mutation.NewNode = NodeFromJson(*NodeObj);
		}
		return Mutation;
	}

	TSharedRef<FJsonObject> EpisodeToJson(const FAuthorizationEpisode& E)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), E.EpisodeId);
		Obj->SetStringField(TEXT("title"), E.Title);
		Obj->SetStringField(TEXT("class"), E.ProposalClass);
		Obj->SetStringField(TEXT("text"), E.ProposalText);
		Obj->SetStringField(TEXT("diagnostic"), E.Diagnostic);
		Obj->SetStringField(TEXT("target"), E.TargetNodeId);
		Obj->SetBoolField(TEXT("gate_valid"), E.bGateValid);

		TSharedRef<FJsonObject> Profile = MakeShared<FJsonObject>();
		Profile->SetNumberField(TEXT("reversibility"), static_cast<int32>(E.Profile.Reversibility));
		Profile->SetNumberField(TEXT("scope"), static_cast<int32>(E.Profile.Scope));
		Profile->SetBoolField(TEXT("meaning"), E.Profile.bChangesNarrativeMeaning);
		Profile->SetBoolField(TEXT("reachability"), E.Profile.bChangesReachability);
		Profile->SetBoolField(TEXT("persistent"), E.Profile.bChangesPersistentState);
		Profile->SetBoolField(TEXT("new_label"), E.Profile.bIntroducesNewLabel);
		Profile->SetNumberField(TEXT("impl"), static_cast<int32>(E.Profile.ImplConsequence));
		Obj->SetObjectField(TEXT("profile"), Profile);

		TSharedRef<FJsonObject> Evidence = MakeShared<FJsonObject>();
		Evidence->SetArrayField(TEXT("branch_state"), ToJsonArray(E.Evidence.BranchState));
		Evidence->SetArrayField(TEXT("affected"), ToJsonArray(E.Evidence.AffectedRegion));
		Evidence->SetArrayField(TEXT("mapping"), ToJsonArray(E.Evidence.MappingLines));
		Evidence->SetArrayField(TEXT("provenance"), ToJsonArray(E.Evidence.ProvenanceLines));
		Evidence->SetArrayField(TEXT("alternatives"), ToJsonArray(E.Evidence.AlternativeLines));
		Evidence->SetArrayField(TEXT("revalidation"), ToJsonArray(E.Evidence.RevalidationChecks));
		Obj->SetObjectField(TEXT("evidence"), Evidence);

		TArray<TSharedPtr<FJsonValue>> Options;
		for (const FEpisodeOption& Option : E.Options)
		{
			TSharedRef<FJsonObject> OptionObj = MakeShared<FJsonObject>();
			OptionObj->SetStringField(TEXT("id"), Option.OptionId);
			OptionObj->SetStringField(TEXT("label"), Option.Label);
			OptionObj->SetStringField(TEXT("description"), Option.Description);
			OptionObj->SetNumberField(TEXT("action"), static_cast<int32>(Option.ActionKind));
			OptionObj->SetBoolField(TEXT("recommended"), Option.bRecommended);
			TArray<TSharedPtr<FJsonValue>> Mutations;
			for (const FGraphMutation& Mutation : Option.Mutations)
			{
				Mutations.Add(MakeShared<FJsonValueObject>(MutationToJson(Mutation)));
			}
			OptionObj->SetArrayField(TEXT("mutations"), Mutations);
			Options.Add(MakeShared<FJsonValueObject>(OptionObj));
		}
		Obj->SetArrayField(TEXT("options"), Options);

		Obj->SetNumberField(TEXT("route"), static_cast<int32>(E.Route));
		Obj->SetNumberField(TEXT("resolved_action"), static_cast<int32>(E.ResolvedAction));
		Obj->SetStringField(TEXT("resolved_option"), E.ResolvedOptionId);
		Obj->SetStringField(TEXT("resolved_by"), E.ResolvedBy);
		Obj->SetNumberField(TEXT("version_after"), E.GraphVersionAfter);
		return Obj;
	}

	FAuthorizationEpisode EpisodeFromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FAuthorizationEpisode E;
		E.EpisodeId = GetStr(Obj, TEXT("id"));
		E.Title = GetStr(Obj, TEXT("title"));
		E.ProposalClass = GetStr(Obj, TEXT("class"));
		E.ProposalText = GetStr(Obj, TEXT("text"));
		E.Diagnostic = GetStr(Obj, TEXT("diagnostic"));
		E.TargetNodeId = GetStr(Obj, TEXT("target"));
		E.bGateValid = GetBool(Obj, TEXT("gate_valid"), true);

		const TSharedPtr<FJsonObject>* Profile = nullptr;
		if (Obj->TryGetObjectField(TEXT("profile"), Profile) && Profile && Profile->IsValid())
		{
			E.Profile.Reversibility = static_cast<EReversibility>(GetInt(*Profile, TEXT("reversibility")));
			E.Profile.Scope = static_cast<EDependencyScope>(GetInt(*Profile, TEXT("scope")));
			E.Profile.bChangesNarrativeMeaning = GetBool(*Profile, TEXT("meaning"));
			E.Profile.bChangesReachability = GetBool(*Profile, TEXT("reachability"));
			E.Profile.bChangesPersistentState = GetBool(*Profile, TEXT("persistent"));
			E.Profile.bIntroducesNewLabel = GetBool(*Profile, TEXT("new_label"));
			E.Profile.ImplConsequence = static_cast<EImplConsequence>(GetInt(*Profile, TEXT("impl")));
		}

		const TSharedPtr<FJsonObject>* Evidence = nullptr;
		if (Obj->TryGetObjectField(TEXT("evidence"), Evidence) && Evidence && Evidence->IsValid())
		{
			E.Evidence.BranchState = FromJsonArray(*Evidence, TEXT("branch_state"));
			E.Evidence.AffectedRegion = FromJsonArray(*Evidence, TEXT("affected"));
			E.Evidence.MappingLines = FromJsonArray(*Evidence, TEXT("mapping"));
			E.Evidence.ProvenanceLines = FromJsonArray(*Evidence, TEXT("provenance"));
			E.Evidence.AlternativeLines = FromJsonArray(*Evidence, TEXT("alternatives"));
			E.Evidence.RevalidationChecks = FromJsonArray(*Evidence, TEXT("revalidation"));
		}

		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if (Obj->TryGetArrayField(TEXT("options"), Options) && Options)
		{
			for (const TSharedPtr<FJsonValue>& V : *Options)
			{
				const TSharedPtr<FJsonObject>* OptionObj = nullptr;
				if (V->TryGetObject(OptionObj) && OptionObj && OptionObj->IsValid())
				{
					FEpisodeOption Option;
					Option.OptionId = GetStr(*OptionObj, TEXT("id"));
					Option.Label = GetStr(*OptionObj, TEXT("label"));
					Option.Description = GetStr(*OptionObj, TEXT("description"));
					Option.ActionKind = static_cast<EProposalAction>(GetInt(*OptionObj, TEXT("action")));
					Option.bRecommended = GetBool(*OptionObj, TEXT("recommended"));
					const TArray<TSharedPtr<FJsonValue>>* Mutations = nullptr;
					if ((*OptionObj)->TryGetArrayField(TEXT("mutations"), Mutations) && Mutations)
					{
						for (const TSharedPtr<FJsonValue>& MV : *Mutations)
						{
							const TSharedPtr<FJsonObject>* MutationObj = nullptr;
							if (MV->TryGetObject(MutationObj) && MutationObj && MutationObj->IsValid())
							{
								Option.Mutations.Add(MutationFromJson(*MutationObj));
							}
						}
					}
					E.Options.Add(Option);
				}
			}
		}

		E.Route = static_cast<EPolicyRoute>(GetInt(Obj, TEXT("route")));
		E.ResolvedAction = static_cast<EProposalAction>(GetInt(Obj, TEXT("resolved_action")));
		E.ResolvedOptionId = GetStr(Obj, TEXT("resolved_option"));
		E.ResolvedBy = GetStr(Obj, TEXT("resolved_by"));
		E.GraphVersionAfter = GetInt(Obj, TEXT("version_after"), -1);
		return E;
	}
}

// ===========================================================================
// Public API
// ===========================================================================

FString ContractSerialization::SaveToJsonString(const FContractModel& Model)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("format_version"), 1);
	Root->SetStringField(TEXT("story_title"), Model.StoryTitle);
	Root->SetStringField(TEXT("genre"), Model.GenreLabel);
	Root->SetNumberField(TEXT("policy"), static_cast<int32>(Model.Policy));
	Root->SetNumberField(TEXT("proposal_mode"), static_cast<int32>(Model.ProposalMode));
	Root->SetNumberField(TEXT("live_counter"), Model.LiveEpisodeCounter);
	Root->SetStringField(TEXT("selected_node"), Model.SelectedNodeId);

	TSharedRef<FJsonObject> Versions = MakeShared<FJsonObject>();
	Versions->SetNumberField(TEXT("library"), Model.Versions.CoreNarrativeLibrary);
	Versions->SetNumberField(TEXT("domain"), Model.Versions.DomainNarrativeProfile);
	Versions->SetNumberField(TEXT("manifest"), Model.Versions.EngineCapabilityManifest);
	Versions->SetNumberField(TEXT("bible"), Model.Versions.StoryBible);
	Versions->SetNumberField(TEXT("graph"), Model.Versions.GraphVersion);
	Root->SetObjectField(TEXT("versions"), Versions);

	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (const FStoryNode& Node : Model.Nodes)
	{
		Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
	}
	Root->SetArrayField(TEXT("nodes"), Nodes);

	TArray<TSharedPtr<FJsonValue>> Curves;
	for (int32 a = 0; a < NumAxes; ++a)
	{
		const FTargetCurve& Curve = Model.Curves[a];
		TSharedRef<FJsonObject> CurveObj = MakeShared<FJsonObject>();
		CurveObj->SetNumberField(TEXT("axis"), a);
		CurveObj->SetArrayField(TEXT("prior"), FloatsToJson(Curve.Prior, NumCurveControlPoints));
		CurveObj->SetArrayField(TEXT("proposal"), FloatsToJson(Curve.Proposal, NumCurveControlPoints));
		CurveObj->SetArrayField(TEXT("approved"), FloatsToJson(Curve.Approved, NumCurveControlPoints));
		CurveObj->SetNumberField(TEXT("epsilon"), Curve.Epsilon);
		Curves.Add(MakeShared<FJsonValueObject>(CurveObj));
	}
	Root->SetArrayField(TEXT("curves"), Curves);

	TArray<TSharedPtr<FJsonValue>> Manifest;
	for (const FCapabilityRecord& Rec : Model.Manifest)
	{
		TSharedRef<FJsonObject> RecObj = MakeShared<FJsonObject>();
		RecObj->SetStringField(TEXT("capability"), Rec.CapabilityId);
		RecObj->SetStringField(TEXT("implementation"), Rec.ImplementationClass);
		RecObj->SetStringField(TEXT("entities"), Rec.EntityTypes);
		RecObj->SetStringField(TEXT("parameters"), Rec.Parameters);
		RecObj->SetStringField(TEXT("evidence"), Rec.EvidenceFields);
		RecObj->SetBoolField(TEXT("registered"), Rec.bRegistered);
		Manifest.Add(MakeShared<FJsonValueObject>(RecObj));
	}
	Root->SetArrayField(TEXT("manifest"), Manifest);

	TArray<TSharedPtr<FJsonValue>> Episodes;
	for (const FAuthorizationEpisode& E : Model.Episodes)
	{
		Episodes.Add(MakeShared<FJsonValueObject>(EpisodeToJson(E)));
	}
	Root->SetArrayField(TEXT("episodes"), Episodes);

	TArray<TSharedPtr<FJsonValue>> Decisions;
	for (const FDecisionRecord& R : Model.DecisionLog)
	{
		TSharedRef<FJsonObject> RecObj = MakeShared<FJsonObject>();
		RecObj->SetStringField(TEXT("episode"), R.EpisodeId);
		RecObj->SetStringField(TEXT("title"), R.EpisodeTitle);
		RecObj->SetNumberField(TEXT("action"), static_cast<int32>(R.Action));
		RecObj->SetStringField(TEXT("option"), R.OptionLabel);
		RecObj->SetStringField(TEXT("actor"), R.Actor);
		RecObj->SetNumberField(TEXT("policy"), static_cast<int32>(R.PolicyAtDecision));
		RecObj->SetNumberField(TEXT("before"), R.GraphVersionBefore);
		RecObj->SetNumberField(TEXT("after"), R.GraphVersionAfter);
		RecObj->SetArrayField(TEXT("revalidated"), ToJsonArray(R.RevalidatedNodes));
		RecObj->SetStringField(TEXT("time"), R.Timestamp);
		Decisions.Add(MakeShared<FJsonValueObject>(RecObj));
	}
	Root->SetArrayField(TEXT("decisions"), Decisions);

	TArray<TSharedPtr<FJsonValue>> CurveEdits;
	for (const FCurveEditRecord& E : Model.CurveEdits)
	{
		TSharedRef<FJsonObject> EditObj = MakeShared<FJsonObject>();
		EditObj->SetNumberField(TEXT("axis"), static_cast<int32>(E.Axis));
		EditObj->SetNumberField(TEXT("index"), E.ControlIndex);
		EditObj->SetNumberField(TEXT("old"), E.OldValue);
		EditObj->SetNumberField(TEXT("new"), E.NewValue);
		EditObj->SetStringField(TEXT("actor"), E.Actor);
		EditObj->SetStringField(TEXT("time"), E.Timestamp);
		CurveEdits.Add(MakeShared<FJsonValueObject>(EditObj));
	}
	Root->SetArrayField(TEXT("curve_edits"), CurveEdits);

	Root->SetArrayField(TEXT("implementation_needs"), ToJsonArray(Model.ImplementationNeeds));

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

bool ContractSerialization::LoadFromJsonString(FContractModel& Model, const FString& Json, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("file is not valid JSON");
		return false;
	}
	if (GetInt(Root, TEXT("format_version")) != 1)
	{
		OutError = TEXT("unsupported format_version");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	if (!Root->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes || Nodes->Num() == 0)
	{
		OutError = TEXT("no nodes in file");
		return false;
	}

	Model.Nodes.Empty();
	Model.Episodes.Empty();
	Model.Manifest.Empty();
	Model.DecisionLog.Empty();
	Model.CurveEdits.Empty();
	Model.ImplementationNeeds.Empty();
	Model.LastRevalidationBoundary.Empty();

	Model.StoryTitle = GetStr(Root, TEXT("story_title"));
	Model.GenreLabel = GetStr(Root, TEXT("genre"));
	Model.Policy = static_cast<EAuthorizationPolicy>(GetInt(Root, TEXT("policy")));
	Model.ProposalMode = static_cast<EProposalSourceMode>(GetInt(Root, TEXT("proposal_mode")));
	Model.LiveEpisodeCounter = GetInt(Root, TEXT("live_counter"));
	Model.SelectedNodeId = GetStr(Root, TEXT("selected_node"));

	const TSharedPtr<FJsonObject>* Versions = nullptr;
	if (Root->TryGetObjectField(TEXT("versions"), Versions) && Versions && Versions->IsValid())
	{
		Model.Versions.CoreNarrativeLibrary = GetInt(*Versions, TEXT("library"), 1);
		Model.Versions.DomainNarrativeProfile = GetInt(*Versions, TEXT("domain"), 1);
		Model.Versions.EngineCapabilityManifest = GetInt(*Versions, TEXT("manifest"), 1);
		Model.Versions.StoryBible = GetInt(*Versions, TEXT("bible"), 1);
		Model.Versions.GraphVersion = GetInt(*Versions, TEXT("graph"), 1);
	}

	for (const TSharedPtr<FJsonValue>& V : *Nodes)
	{
		const TSharedPtr<FJsonObject>* NodeObj = nullptr;
		if (V->TryGetObject(NodeObj) && NodeObj && NodeObj->IsValid())
		{
			Model.Nodes.Add(NodeFromJson(*NodeObj));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Curves = nullptr;
	if (Root->TryGetArrayField(TEXT("curves"), Curves) && Curves)
	{
		for (const TSharedPtr<FJsonValue>& V : *Curves)
		{
			const TSharedPtr<FJsonObject>* CurveObj = nullptr;
			if (V->TryGetObject(CurveObj) && CurveObj && CurveObj->IsValid())
			{
				const int32 Axis = GetInt(*CurveObj, TEXT("axis"), -1);
				if (Axis >= 0 && Axis < NumAxes)
				{
					FTargetCurve& Curve = Model.Curves[Axis];
					Curve.Axis = static_cast<ENarrativeAxis>(Axis);
					JsonToFloats(*CurveObj, TEXT("prior"), Curve.Prior, NumCurveControlPoints);
					JsonToFloats(*CurveObj, TEXT("proposal"), Curve.Proposal, NumCurveControlPoints);
					JsonToFloats(*CurveObj, TEXT("approved"), Curve.Approved, NumCurveControlPoints);
					double Eps = 0.2;
					if ((*CurveObj)->TryGetNumberField(TEXT("epsilon"), Eps))
					{
						Curve.Epsilon = static_cast<float>(Eps);
					}
				}
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Manifest = nullptr;
	if (Root->TryGetArrayField(TEXT("manifest"), Manifest) && Manifest)
	{
		for (const TSharedPtr<FJsonValue>& V : *Manifest)
		{
			const TSharedPtr<FJsonObject>* RecObj = nullptr;
			if (V->TryGetObject(RecObj) && RecObj && RecObj->IsValid())
			{
				FCapabilityRecord Rec;
				Rec.CapabilityId = GetStr(*RecObj, TEXT("capability"));
				Rec.ImplementationClass = GetStr(*RecObj, TEXT("implementation"));
				Rec.EntityTypes = GetStr(*RecObj, TEXT("entities"));
				Rec.Parameters = GetStr(*RecObj, TEXT("parameters"));
				Rec.EvidenceFields = GetStr(*RecObj, TEXT("evidence"));
				Rec.bRegistered = GetBool(*RecObj, TEXT("registered"), true);
				Model.Manifest.Add(Rec);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Episodes = nullptr;
	if (Root->TryGetArrayField(TEXT("episodes"), Episodes) && Episodes)
	{
		for (const TSharedPtr<FJsonValue>& V : *Episodes)
		{
			const TSharedPtr<FJsonObject>* EpisodeObj = nullptr;
			if (V->TryGetObject(EpisodeObj) && EpisodeObj && EpisodeObj->IsValid())
			{
				Model.Episodes.Add(EpisodeFromJson(*EpisodeObj));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (Root->TryGetArrayField(TEXT("decisions"), Decisions) && Decisions)
	{
		for (const TSharedPtr<FJsonValue>& V : *Decisions)
		{
			const TSharedPtr<FJsonObject>* RecObj = nullptr;
			if (V->TryGetObject(RecObj) && RecObj && RecObj->IsValid())
			{
				FDecisionRecord R;
				R.EpisodeId = GetStr(*RecObj, TEXT("episode"));
				R.EpisodeTitle = GetStr(*RecObj, TEXT("title"));
				R.Action = static_cast<EProposalAction>(GetInt(*RecObj, TEXT("action")));
				R.OptionLabel = GetStr(*RecObj, TEXT("option"));
				R.Actor = GetStr(*RecObj, TEXT("actor"));
				R.PolicyAtDecision = static_cast<EAuthorizationPolicy>(GetInt(*RecObj, TEXT("policy")));
				R.GraphVersionBefore = GetInt(*RecObj, TEXT("before"));
				R.GraphVersionAfter = GetInt(*RecObj, TEXT("after"));
				R.RevalidatedNodes = FromJsonArray(*RecObj, TEXT("revalidated"));
				R.Timestamp = GetStr(*RecObj, TEXT("time"));
				Model.DecisionLog.Add(R);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* CurveEdits = nullptr;
	if (Root->TryGetArrayField(TEXT("curve_edits"), CurveEdits) && CurveEdits)
	{
		for (const TSharedPtr<FJsonValue>& V : *CurveEdits)
		{
			const TSharedPtr<FJsonObject>* EditObj = nullptr;
			if (V->TryGetObject(EditObj) && EditObj && EditObj->IsValid())
			{
				FCurveEditRecord E;
				E.Axis = static_cast<ENarrativeAxis>(GetInt(*EditObj, TEXT("axis")));
				E.ControlIndex = GetInt(*EditObj, TEXT("index"));
				double D = 0.0;
				if ((*EditObj)->TryGetNumberField(TEXT("old"), D)) { E.OldValue = static_cast<float>(D); }
				if ((*EditObj)->TryGetNumberField(TEXT("new"), D)) { E.NewValue = static_cast<float>(D); }
				E.Actor = GetStr(*EditObj, TEXT("actor"));
				E.Timestamp = GetStr(*EditObj, TEXT("time"));
				Model.CurveEdits.Add(E);
			}
		}
	}

	Model.ImplementationNeeds = FromJsonArray(Root, TEXT("implementation_needs"));

	// Blocked flags and statuses are derived state: recompute them.
	Model.Revalidate(TArray<FString>());
	Model.OnChanged.Broadcast();
	return true;
}

FString ContractSerialization::SaveToFile(const FContractModel& Model)
{
	const FString Json = SaveToJsonString(Model);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("ContractState.json");
	const FString Backup = FPaths::ProjectSavedDir() /
		FString::Printf(TEXT("ContractState_%s.json"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));

	if (!FFileHelper::SaveStringToFile(Json, *Path))
	{
		return TEXT("Save failed: could not write Saved/ContractState.json");
	}
	FFileHelper::SaveStringToFile(Json, *Backup);
	return FString::Printf(TEXT("Saved graph v%d to Saved/ContractState.json (+ timestamped backup)."),
		Model.Versions.GraphVersion);
}

FString ContractSerialization::LoadFromFile(FContractModel& Model)
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("ContractState.json");
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return TEXT("Load failed: Saved/ContractState.json not found (save first).");
	}
	FString Error;
	if (!LoadFromJsonString(Model, Json, Error))
	{
		return FString::Printf(TEXT("Load failed: %s."), *Error);
	}
	return FString::Printf(TEXT("Loaded graph v%d from Saved/ContractState.json."), Model.Versions.GraphVersion);
}
