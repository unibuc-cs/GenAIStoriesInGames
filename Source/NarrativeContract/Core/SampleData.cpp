// Sample contract, curves, manifest, story graph, and authorization queue.
// The story is the hydro-station mystery from Figure 4 of the paper; the
// pending episodes exercise the four proposal classes discussed in the text:
// a blocked-transition repair, a registered normalization, an unsupported
// capability (placeholder decision), and an irreversible deletion.

#include "ContractModel.h"

#include <initializer_list>

namespace
{
	FStoryNode MakeNode(
		const FString& Id, const FString& Title, const FString& Description,
		std::initializer_list<float> Axes, int32 Depth, int32 Lane)
	{
		FStoryNode Node;
		Node.NodeId = Id;
		Node.Title = Title;
		Node.Description = Description;
		int32 i = 0;
		for (float V : Axes)
		{
			if (i < NumAxes)
			{
				Node.AxisEstimates[i] = V;
			}
			++i;
		}
		Node.Depth = Depth;
		Node.Lane = Lane;
		return Node;
	}

	void SetCurve(FTargetCurve& Curve, ENarrativeAxis Axis,
		std::initializer_list<float> Prior, std::initializer_list<float> Delta)
	{
		Curve.Axis = Axis;
		int32 i = 0;
		for (float V : Prior)
		{
			if (i < NumCurveControlPoints)
			{
				Curve.Prior[i] = V;
			}
			++i;
		}
		i = 0;
		for (float D : Delta)
		{
			if (i < NumCurveControlPoints)
			{
				// Bounded proposal (Eq. 3) and initially-approved curve.
				const float Clipped = FMath::Clamp(D, -Curve.Epsilon, Curve.Epsilon);
				Curve.Proposal[i] = FMath::Clamp(Curve.Prior[i] + Clipped, 0.f, 1.f);
				Curve.Approved[i] = Curve.Proposal[i];
			}
			++i;
		}
	}
}

void FContractModel::ResetContent()
{
	Nodes.Empty();
	Episodes.Empty();
	Manifest.Empty();
	DecisionLog.Empty();
	CurveEdits.Empty();
	ImplementationNeeds.Empty();
	LastRevalidationBoundary.Empty();
	UndoStack.Empty();
	Telemetry.Empty();
	LiveEpisodeCounter = 0;
	Policy = EAuthorizationPolicy::Assisted;
	Versions = FContractVersions();
	for (int32 a = 0; a < NumAxes; ++a)
	{
		Curves[a] = FTargetCurve();
		Curves[a].Axis = static_cast<ENarrativeAxis>(a);
	}
}

void FContractModel::BuildSampleData(bool bAutoApplyPolicy)
{
	ResetContent();

	StoryTitle = TEXT("Hydro-Station Mystery");
	GenreLabel = TEXT("Mystery");

	// -----------------------------------------------------------------
	// Target curves (Sec. 3.2, Figure 2). Prior = genre prior mu_a,
	// proposal adapted within +-epsilon.
	// -----------------------------------------------------------------
	SetCurve(Curves[0], ENarrativeAxis::Valence,     {0.40f, 0.30f, 0.45f, 0.55f, 0.70f}, {-0.02f, 0.00f, 0.05f, 0.05f, 0.00f});
	SetCurve(Curves[1], ENarrativeAxis::Tension,     {0.15f, 0.40f, 0.60f, 0.85f, 0.50f}, { 0.03f, 0.00f, 0.02f, 0.03f, 0.00f});
	SetCurve(Curves[2], ENarrativeAxis::Agency,      {0.50f, 0.65f, 0.40f, 0.70f, 0.65f}, { 0.00f, 0.02f,-0.03f, 0.00f, 0.05f});
	SetCurve(Curves[3], ENarrativeAxis::Information, {0.10f, 0.30f, 0.50f, 0.70f, 0.95f}, { 0.02f, 0.05f, 0.02f, 0.05f, 0.00f});
	SetCurve(Curves[4], ENarrativeAxis::Stakes,      {0.30f, 0.35f, 0.50f, 0.85f, 0.75f}, { 0.00f, 0.02f, 0.05f, 0.00f,-0.02f});

	// -----------------------------------------------------------------
	// Engine Capability Manifest (Sec. 3.4)
	// -----------------------------------------------------------------
	Manifest.Add({TEXT("InspectEvidence"),        TEXT("BP_EvidenceStation"),        TEXT("EvidenceProp"),        TEXT("decode_time: 2-8 s"),         TEXT("interaction_started, decode_completed"), true});
	Manifest.Add({TEXT("WriteKnowledgePredicate"),TEXT("KnowledgeSubsystem"),        TEXT("any"),                 TEXT("predicate id (licensed set)"), TEXT("predicate_written, save_persisted"),     true});
	Manifest.Add({TEXT("ConfrontDialogue"),       TEXT("BP_ConfrontationEncounter"), TEXT("NPC"),                 TEXT("branch count: 2-4"),           TEXT("dialogue_completed, branch_selected"),   true});
	Manifest.Add({TEXT("DetainAction"),           TEXT("BP_ArrestSequence"),         TEXT("NPC"),                 TEXT("none"),                        TEXT("actor_despawned, flag_set"),             true});
	Manifest.Add({TEXT("TrackTarget"),            TEXT("BP_TrackingMarker"),         TEXT("NPC, Waypoint"),       TEXT("range: 5-200 m"),              TEXT("marker_attached, path_logged"),          true});
	Manifest.Add({TEXT("CrowdSystem"),            TEXT("(none registered)"),         TEXT("-"),                   TEXT("-"),                           TEXT("-"),                                      false});

	// -----------------------------------------------------------------
	// Story graph (Figure 4, "before repair" -- graph v17)
	// -----------------------------------------------------------------
	{
		FStoryNode N1 = MakeNode(TEXT("N1"), TEXT("Pump alarm"),
			TEXT("A pressure alarm sounds in the hydro station's pump hall. Coolant is venting, and the duty roster says Ilya left an hour early."),
			{0.35f, 0.30f, 0.50f, 0.20f, 0.30f}, 0, 1);
		N1.Entities = {TEXT("player"), TEXT("pump_hall"), TEXT("ilya")};
		N1.AddEffects = {TEXT("at(hydro_station)"), TEXT("alarm(active)")};
		N1.GameplayBindings = {TEXT("ambient alarm loop"), TEXT("objective marker: pump hall")};
		N1.Provenance = {TEXT("approved root (Story Bible v2)"), TEXT("model proposal, contract C(L3,D5,M7,B2)")};
		FStoryChoice C1a;
		C1a.ChoiceId = TEXT("c_n1_locker");
		C1a.Label = TEXT("Open the staff locker");
		C1a.TargetNodeId = TEXT("N4");
		FStoryChoice C1b;
		C1b.ChoiceId = TEXT("c_n1_residue");
		C1b.Label = TEXT("Scan the coolant residue");
		C1b.TargetNodeId = TEXT("N5");
		N1.Choices = {C1a, C1b};
		Nodes.Add(N1);

		FStoryNode N4 = MakeNode(TEXT("N4"), TEXT("Open locker"),
			TEXT("Ilya's locker is unlocked. Inside: a maintenance log with pages torn out and a spare keycard."),
			{0.40f, 0.40f, 0.55f, 0.35f, 0.35f}, 1, 0);
		N4.Entities = {TEXT("player"), TEXT("locker"), TEXT("maintenance_log")};
		N4.Preconditions = {TEXT("at(hydro_station)")};
		N4.AddEffects = {TEXT("locker(open)"), TEXT("has(player, maintenance_log)")};
		N4.GameplayBindings = {TEXT("interactable locker prop")};
		N4.PrimaryParentId = TEXT("N1");
		N4.PrimaryIncomingChoiceId = TEXT("c_n1_locker");
		N4.Provenance = {TEXT("model proposal, admitted at graph v14")};
		FStoryChoice C4;
		C4.ChoiceId = TEXT("c_n4_read");
		C4.Label = TEXT("read the log"); // normalized by episode E2
		C4.TargetNodeId = TEXT("N6");
		N4.Choices = {C4};
		Nodes.Add(N4);

		FStoryNode N5 = MakeNode(TEXT("N5"), TEXT("Scan residue"),
			TEXT("A spectrometer pass over the coolant residue reveals a distinctive sabotage signature: someone forced the relief valve."),
			{0.30f, 0.45f, 0.40f, 0.60f, 0.40f}, 1, 3);
		N5.Entities = {TEXT("player"), TEXT("coolant_residue")};
		N5.Preconditions = {TEXT("at(hydro_station)")};
		N5.AddEffects = {TEXT("knows(player, sabotage_signature)"), TEXT("evidence(residue_scan)")};
		N5.GameplayBindings = {TEXT("evidence scan interaction")};
		N5.RequiredCapabilities = {TEXT("InspectEvidence"), TEXT("WriteKnowledgePredicate")};
		N5.SelectedMapping.Add(TEXT("InspectEvidence"), TEXT("BP_EvidenceStation"));
		N5.SelectedMapping.Add(TEXT("WriteKnowledgePredicate"), TEXT("KnowledgeSubsystem"));
		N5.Grounding = EGroundingStatus::Implemented;
		N5.PrimaryParentId = TEXT("N1");
		N5.PrimaryIncomingChoiceId = TEXT("c_n1_residue");
		N5.Provenance = {TEXT("model proposal, admitted at graph v15")};
		FStoryChoice C5;
		C5.ChoiceId = TEXT("c_n5_confront");
		C5.Label = TEXT("Confront Ilya in the turbine hall");
		C5.TargetNodeId = TEXT("N7");
		N5.Choices = {C5};
		Nodes.Add(N5);

		FStoryNode N6 = MakeNode(TEXT("N6"), TEXT("Read log"),
			TEXT("The surviving pages are written in the station's maintenance shorthand. Something about valve 7 -- but the notation is opaque."),
			{0.35f, 0.55f, 0.45f, 0.55f, 0.45f}, 2, 0);
		N6.Entities = {TEXT("player"), TEXT("maintenance_log")};
		N6.Preconditions = {TEXT("has(player, maintenance_log)")};
		N6.AddEffects = {TEXT("read(maintenance_log)")};
		N6.GameplayBindings = {TEXT("readable document UI")};
		N6.PrimaryParentId = TEXT("N4");
		N6.PrimaryIncomingChoiceId = TEXT("c_n4_read");
		N6.Provenance = {TEXT("model proposal, admitted at graph v15")};
		FStoryChoice C6;
		C6.ChoiceId = TEXT("c_n6_confront");
		C6.Label = TEXT("Head to the turbine hall");
		C6.TargetNodeId = TEXT("N7");
		N6.Choices = {C6};
		Nodes.Add(N6);

		FStoryNode N7 = MakeNode(TEXT("N7"), TEXT("Confront Ilya"),
			TEXT("Ilya is waiting among the turbines. If you can name the sabotage signature, he has no way to deny it."),
			{0.25f, 0.85f, 0.65f, 0.80f, 0.75f}, 3, 2);
		N7.Entities = {TEXT("player"), TEXT("ilya"), TEXT("turbine_hall")};
		N7.Preconditions = {TEXT("knows(player, sabotage_signature)")};
		N7.AddEffects = {TEXT("confronted(ilya)")};
		N7.GameplayBindings = {TEXT("confrontation dialogue encounter")};
		N7.RequiredCapabilities = {TEXT("ConfrontDialogue")};
		N7.SelectedMapping.Add(TEXT("ConfrontDialogue"), TEXT("BP_ConfrontationEncounter"));
		N7.Grounding = EGroundingStatus::Implemented;
		// Primary path is the locker branch; the N5 edge is a sibling route.
		N7.PrimaryParentId = TEXT("N6");
		N7.PrimaryIncomingChoiceId = TEXT("c_n6_confront");
		N7.Provenance = {TEXT("model proposal, admitted at graph v16")};
		FStoryChoice C7a;
		C7a.ChoiceId = TEXT("c_n7_detain");
		C7a.Label = TEXT("Detain Ilya");
		C7a.AddEffects = {TEXT("detained(ilya)")};
		C7a.TargetNodeId = TEXT("N8");
		FStoryChoice C7b;
		C7b.ChoiceId = TEXT("c_n7_track");
		C7b.Label = TEXT("Let him run and track the ally");
		C7b.AddEffects = {TEXT("tracking(ally)")};
		C7b.TargetNodeId = TEXT("N9");
		N7.Choices = {C7a, C7b};
		Nodes.Add(N7);

		FStoryNode N8 = MakeNode(TEXT("N8"), TEXT("Detain Ilya"),
			TEXT("Station security takes Ilya into custody. The pumps hold -- but whoever paid him is still out there."),
			{0.45f, 0.55f, 0.70f, 0.90f, 0.80f}, 4, 1);
		N8.Entities = {TEXT("player"), TEXT("ilya"), TEXT("security")};
		N8.Preconditions = {TEXT("confronted(ilya)"), TEXT("detained(ilya)")};
		N8.GameplayBindings = {TEXT("arrest sequence")};
		N8.RequiredCapabilities = {TEXT("DetainAction")};
		N8.SelectedMapping.Add(TEXT("DetainAction"), TEXT("BP_ArrestSequence"));
		N8.Grounding = EGroundingStatus::Implemented;
		N8.bEnding = true;
		N8.PrimaryParentId = TEXT("N7");
		N8.PrimaryIncomingChoiceId = TEXT("c_n7_detain");
		N8.Provenance = {TEXT("model proposal, admitted at graph v16")};
		Nodes.Add(N8);

		FStoryNode N9 = MakeNode(TEXT("N9"), TEXT("Track ally"),
			TEXT("You shadow Ilya to a service dock, where a second figure waits with a boat. The conspiracy is bigger than one technician."),
			{0.40f, 0.60f, 0.75f, 0.85f, 0.70f}, 4, 3);
		N9.Entities = {TEXT("player"), TEXT("ilya"), TEXT("ally"), TEXT("service_dock")};
		N9.Preconditions = {TEXT("confronted(ilya)"), TEXT("tracking(ally)")};
		N9.GameplayBindings = {TEXT("tracking marker, stealth follow")};
		N9.RequiredCapabilities = {TEXT("TrackTarget")};
		N9.SelectedMapping.Add(TEXT("TrackTarget"), TEXT("BP_TrackingMarker"));
		N9.Grounding = EGroundingStatus::Implemented;
		N9.bEnding = true;
		N9.PrimaryParentId = TEXT("N7");
		N9.PrimaryIncomingChoiceId = TEXT("c_n7_track");
		N9.Provenance = {TEXT("model proposal, admitted at graph v16")};
		Nodes.Add(N9);
	}

	// -----------------------------------------------------------------
	// Authorization episodes (Sec. 3.6)
	// -----------------------------------------------------------------

	// E1 -- the Figure 4 repair: N6 -> N7 blocked by a failed knowledge
	// precondition; bounded responses are insert / weaken / reject.
	{
		FAuthorizationEpisode E;
		E.EpisodeId = TEXT("E1");
		E.Title = TEXT("Blocked transition N6 - N7");
		E.ProposalClass = TEXT("Repair");
		E.ProposalText = TEXT("Insert setup node N6b 'Decode the log' so the confrontation is licensed on the locker branch.");
		E.Diagnostic = TEXT("N7 requires knows(player, sabotage_signature), absent after N6; N5 state is branch-local.");
		E.TargetNodeId = TEXT("N7");
		E.bGateValid = false;
		E.Profile.Reversibility = EReversibility::Reversible;
		E.Profile.Scope = EDependencyScope::Branch;
		E.Profile.bChangesNarrativeMeaning = true;
		E.Profile.bChangesReachability = true;
		E.Profile.bChangesPersistentState = true; // writes a knowledge predicate
		E.Profile.ImplConsequence = EImplConsequence::RegisteredMapping;

		E.Evidence.BranchState = {
			TEXT("at(hydro_station)"), TEXT("alarm(active)"), TEXT("locker(open)"),
			TEXT("has(player, maintenance_log)"), TEXT("read(maintenance_log)")};
		E.Evidence.AffectedRegion = {TEXT("N6"), TEXT("N7"), TEXT("N8"), TEXT("N9")};
		E.Evidence.MappingLines = {
			TEXT("InspectEvidence -> BP_EvidenceStation (decode_time 2-8 s)"),
			TEXT("WriteKnowledgePredicate -> KnowledgeSubsystem (licensed predicate)")};
		E.Evidence.ProvenanceLines = {
			TEXT("candidate generated under contract C(L3, D5, M7, B2)"),
			TEXT("validator: EdgeValid failed at stage 3 (successor precondition)")};
		E.Evidence.AlternativeLines = {
			TEXT("Insert decoding event N6b (registered capabilities)"),
			TEXT("Weaken the N7 precondition (drops the knowledge gate)"),
			TEXT("Reject N7 on this branch (keeps N5 route only)")};
		E.Evidence.RevalidationChecks = {
			TEXT("recheck N6b-N9 and incident edges"),
			TEXT("preserve N1, N4, N5 validation records"),
			TEXT("re-run path-fit measures on the locker spine")};

		FEpisodeOption Insert;
		Insert.OptionId = TEXT("E1_insert");
		Insert.Label = TEXT("Approve: insert N6b 'Decode the log'");
		Insert.Description = TEXT("Adds a decoding event between N6 and N7, backed by InspectEvidence and WriteKnowledgePredicate.");
		Insert.ActionKind = EProposalAction::Approve;
		Insert.bRecommended = true;
		{
			FGraphMutation M;
			M.Type = EGraphMutationType::InsertNodeBetween;
			M.TargetNodeId = TEXT("N7");
			FStoryNode N6b = MakeNode(TEXT("N6b"), TEXT("Decode log"),
				TEXT("Cross-referencing the shorthand with the station manual, the torn pages resolve into a record of forced valve work -- the sabotage signature."),
				{0.30f, 0.60f, 0.50f, 0.75f, 0.50f}, 3, 2);
			N6b.Entities = {TEXT("player"), TEXT("maintenance_log")};
			N6b.Preconditions = {TEXT("has(player, maintenance_log)"), TEXT("read(maintenance_log)")};
			N6b.AddEffects = {TEXT("knows(player, sabotage_signature)")};
			N6b.GameplayBindings = {TEXT("decode interaction at the manual station")};
			N6b.RequiredCapabilities = {TEXT("InspectEvidence"), TEXT("WriteKnowledgePredicate")};
			N6b.SelectedMapping.Add(TEXT("InspectEvidence"), TEXT("BP_EvidenceStation"));
			N6b.SelectedMapping.Add(TEXT("WriteKnowledgePredicate"), TEXT("KnowledgeSubsystem"));
			N6b.Grounding = EGroundingStatus::Implemented;
			N6b.Provenance = {TEXT("repair insert authorized via episode E1")};
			M.NewNode = N6b;
			Insert.Mutations.Add(M);
		}
		E.Options.Add(Insert);

		FEpisodeOption Weaken;
		Weaken.OptionId = TEXT("E1_weaken");
		Weaken.Label = TEXT("Edit: weaken the N7 precondition");
		Weaken.Description = TEXT("Removes knows(player, sabotage_signature) from N7. The confrontation loses its evidence gate on every branch.");
		Weaken.ActionKind = EProposalAction::Edit;
		{
			FGraphMutation M;
			M.Type = EGraphMutationType::RemovePrecondition;
			M.TargetNodeId = TEXT("N7");
			M.StringParamA = TEXT("knows(player, sabotage_signature)");
			Weaken.Mutations.Add(M);
		}
		E.Options.Add(Weaken);

		FEpisodeOption Reject;
		Reject.OptionId = TEXT("E1_reject");
		Reject.Label = TEXT("Reject the transition");
		Reject.Description = TEXT("N7 remains unreachable from the locker branch; the N5 route is unaffected.");
		Reject.ActionKind = EProposalAction::Reject;
		E.Options.Add(Reject);

		Episodes.Add(E);
	}

	// E2 -- registered local normalization (auto under Automatic/Assisted,
	// review under Strict).
	{
		FAuthorizationEpisode E;
		E.EpisodeId = TEXT("E2");
		E.Title = TEXT("Normalize choice label at N4");
		E.ProposalClass = TEXT("Normalization");
		E.ProposalText = TEXT("Rename choice 'read the log' to the canonical form 'Read the maintenance log'.");
		E.Diagnostic = TEXT("Label deviates from the Domain Narrative Profile's canonical object naming.");
		E.TargetNodeId = TEXT("N4");
		E.bGateValid = true;
		E.Profile.Reversibility = EReversibility::Reversible;
		E.Profile.Scope = EDependencyScope::Local;
		E.Profile.ImplConsequence = EImplConsequence::None;

		E.Evidence.BranchState = {TEXT("at(hydro_station)"), TEXT("alarm(active)"), TEXT("locker(open)"), TEXT("has(player, maintenance_log)")};
		E.Evidence.AffectedRegion = {TEXT("N4")};
		E.Evidence.ProvenanceLines = {TEXT("normalization rule: canonical_object_labels (Library v3)")};
		E.Evidence.AlternativeLines = {TEXT("Apply canonical label"), TEXT("Keep the original wording")};
		E.Evidence.RevalidationChecks = {TEXT("no state, reachability, or mapping change; wording only")};

		FEpisodeOption Apply;
		Apply.OptionId = TEXT("E2_apply");
		Apply.Label = TEXT("Approve: apply canonical label");
		Apply.Description = TEXT("Reversible wording-only correction, recorded and undoable.");
		Apply.ActionKind = EProposalAction::Approve;
		Apply.bRecommended = true;
		{
			FGraphMutation M;
			M.Type = EGraphMutationType::SetChoiceLabel;
			M.TargetNodeId = TEXT("N4");
			M.StringParamA = TEXT("c_n4_read");
			M.StringParamB = TEXT("Read the maintenance log");
			Apply.Mutations.Add(M);
		}
		E.Options.Add(Apply);

		FEpisodeOption Keep;
		Keep.OptionId = TEXT("E2_keep");
		Keep.Label = TEXT("Reject: keep original wording");
		Keep.Description = TEXT("Retains the generated label; records the deviation.");
		Keep.ActionKind = EProposalAction::Reject;
		E.Options.Add(Keep);

		Episodes.Add(E);
	}

	// E3 -- unsupported capability: the generator wants a crowd at the
	// confrontation, but no CrowdSystem is registered in the manifest.
	{
		FAuthorizationEpisode E;
		E.EpisodeId = TEXT("E3");
		E.Title = TEXT("Unsupported capability at N7");
		E.ProposalClass = TEXT("Capability");
		E.ProposalText = TEXT("'Off-shift workers crowd the turbine hall as the confrontation unfolds' -- requires CrowdSystem.");
		E.Diagnostic = TEXT("CrowdSystem has no registered implementation in the Engine Capability Manifest (M v7).");
		E.TargetNodeId = TEXT("N7");
		E.bGateValid = false;
		E.Profile.Reversibility = EReversibility::Reversible;
		E.Profile.Scope = EDependencyScope::Local;
		E.Profile.bChangesNarrativeMeaning = true;
		E.Profile.ImplConsequence = EImplConsequence::Placeholder;

		E.Evidence.BranchState = {TEXT("(applies to every branch reaching N7)")};
		E.Evidence.AffectedRegion = {TEXT("N7")};
		E.Evidence.MappingLines = {TEXT("CrowdSystem -> no registered implementation")};
		E.Evidence.ProvenanceLines = {TEXT("candidate enhancement generated under contract C(L3, D5, M7, B2)")};
		E.Evidence.AlternativeLines = {
			TEXT("Approve a visible placeholder (production commitment)"),
			TEXT("Record an implementation need without changing the scene"),
			TEXT("Reject the enhancement")};
		E.Evidence.RevalidationChecks = {TEXT("N7 grounding record; manifest need list")};

		FEpisodeOption Placeholder;
		Placeholder.OptionId = TEXT("E3_placeholder");
		Placeholder.Label = TEXT("Approve visible placeholder");
		Placeholder.Description = TEXT("N7 carries an approved CrowdSystem placeholder; the demo marks it explicitly until an implementation is registered.");
		Placeholder.ActionKind = EProposalAction::Approve;
		{
			FGraphMutation A;
			A.Type = EGraphMutationType::SetGrounding;
			A.TargetNodeId = TEXT("N7");
			A.Grounding = EGroundingStatus::ApprovedPlaceholder;
			Placeholder.Mutations.Add(A);
			FGraphMutation B;
			B.Type = EGraphMutationType::AddProvenance;
			B.TargetNodeId = TEXT("N7");
			B.StringParamA = TEXT("CrowdSystem placeholder approved via episode E3");
			Placeholder.Mutations.Add(B);
			FGraphMutation C;
			C.Type = EGraphMutationType::AddCapabilityNeed;
			C.StringParamA = TEXT("CrowdSystem for N7 confrontation (owner unassigned)");
			Placeholder.Mutations.Add(C);
		}
		E.Options.Add(Placeholder);

		FEpisodeOption NeedOnly;
		NeedOnly.OptionId = TEXT("E3_need");
		NeedOnly.Label = TEXT("Substitute: record implementation need only");
		NeedOnly.Description = TEXT("Keeps N7 unchanged and files the capability request against the manifest.");
		NeedOnly.ActionKind = EProposalAction::Substitute;
		{
			FGraphMutation C;
			C.Type = EGraphMutationType::AddCapabilityNeed;
			C.StringParamA = TEXT("CrowdSystem (deferred enhancement, no scene change)");
			NeedOnly.Mutations.Add(C);
		}
		E.Options.Add(NeedOnly);

		FEpisodeOption Reject;
		Reject.OptionId = TEXT("E3_reject");
		Reject.Label = TEXT("Reject the enhancement");
		Reject.Description = TEXT("The confrontation stays as authored.");
		Reject.ActionKind = EProposalAction::Reject;
		E.Options.Add(Reject);

		Episodes.Add(E);
	}

	// E4 -- irreversible deletion of a reachable ending.
	{
		FAuthorizationEpisode E;
		E.EpisodeId = TEXT("E4");
		E.Title = TEXT("Remove ending N9 'Track ally'");
		E.ProposalClass = TEXT("Deletion");
		E.ProposalText = TEXT("Drop the tracking ending to focus the finale on the arrest.");
		E.Diagnostic = TEXT("Removes a reachable ending; irreversible consequence for every path through c_n7_track.");
		E.TargetNodeId = TEXT("N9");
		E.bGateValid = true;
		E.Profile.Reversibility = EReversibility::Irreversible;
		E.Profile.Scope = EDependencyScope::Global;
		E.Profile.bChangesNarrativeMeaning = true;
		E.Profile.bChangesReachability = true;
		E.Profile.ImplConsequence = EImplConsequence::None;

		E.Evidence.BranchState = {TEXT("confronted(ilya)"), TEXT("tracking(ally)")};
		E.Evidence.AffectedRegion = {TEXT("N7"), TEXT("N9")};
		E.Evidence.ProvenanceLines = {TEXT("editorial proposal (model), no validator failure")};
		E.Evidence.AlternativeLines = {TEXT("Remove N9 and its incoming choice"), TEXT("Retain both endings")};
		E.Evidence.RevalidationChecks = {TEXT("N7 choice set; declared-endings check; path-fit on the dock spine")};

		FEpisodeOption Remove;
		Remove.OptionId = TEXT("E4_remove");
		Remove.Label = TEXT("Approve removal of N9");
		Remove.Description = TEXT("Irreversible: the dock ending and its provenance leave the playable graph.");
		Remove.ActionKind = EProposalAction::Approve;
		{
			FGraphMutation M;
			M.Type = EGraphMutationType::RemoveNode;
			M.TargetNodeId = TEXT("N9");
			Remove.Mutations.Add(M);
		}
		E.Options.Add(Remove);

		FEpisodeOption Keep;
		Keep.OptionId = TEXT("E4_keep");
		Keep.Label = TEXT("Reject: retain both endings");
		Keep.Description = TEXT("Keeps N9; records the editorial suggestion and rationale.");
		Keep.ActionKind = EProposalAction::Reject;
		Keep.bRecommended = true;
		E.Options.Add(Keep);

		Episodes.Add(E);
	}

	SelectedNodeId = TEXT("N7");
	LiveStatus = TEXT("Curated queue loaded.");

	// Initial validation of the whole graph (marks N6 -> N7 blocked), then
	// route the queue under the starting policy: Assisted applies the
	// registered normalization E2 automatically and logs it. Tests pass
	// bAutoApplyPolicy=false to keep the full queue pending.
	Revalidate(TArray<FString>());
	if (bAutoApplyPolicy)
	{
		RouteAndAutoApply(TEXT("policy:Assisted"));
	}
}
