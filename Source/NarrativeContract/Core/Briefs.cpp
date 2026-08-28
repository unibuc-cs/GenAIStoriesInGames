#include "Briefs.h"

#include "ContractModel.h"
#include "ContractSerialization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

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

	void SetCurve(FTargetCurve& Curve, ENarrativeAxis Axis, std::initializer_list<float> Prior)
	{
		Curve.Axis = Axis;
		int32 i = 0;
		for (float V : Prior)
		{
			if (i < NumCurveControlPoints)
			{
				Curve.Prior[i] = V;
				Curve.Proposal[i] = V;
				Curve.Approved[i] = V;
			}
			++i;
		}
	}

	FStoryChoice MakeChoice(const FString& Id, const FString& Label, const FString& Target)
	{
		FStoryChoice Choice;
		Choice.ChoiceId = Id;
		Choice.Label = Label;
		Choice.TargetNodeId = Target;
		return Choice;
	}

	// ==================================================================
	// Brief 2: "Dam Breach" (disaster). Same pattern as Figure 4 in a new
	// domain: the knowledge fact lives on the control-room branch, so the
	// village branch's shortcut to the manual release is blocked.
	// ==================================================================
	void BuildDamBreach(FContractModel& Model)
	{
		Model.ResetContent();
		Model.StoryTitle = TEXT("Dam Breach");
		Model.GenreLabel = TEXT("Disaster");

		SetCurve(Model.Curves[0], ENarrativeAxis::Valence,     {0.30f, 0.20f, 0.30f, 0.45f, 0.65f});
		SetCurve(Model.Curves[1], ENarrativeAxis::Tension,     {0.55f, 0.70f, 0.85f, 0.90f, 0.55f});
		SetCurve(Model.Curves[2], ENarrativeAxis::Agency,      {0.40f, 0.55f, 0.65f, 0.75f, 0.70f});
		SetCurve(Model.Curves[3], ENarrativeAxis::Information, {0.20f, 0.40f, 0.55f, 0.70f, 0.85f});
		SetCurve(Model.Curves[4], ENarrativeAxis::Stakes,      {0.60f, 0.70f, 0.85f, 0.95f, 0.80f});

		Model.Manifest.Add({TEXT("FloodSimulation"),  TEXT("BP_FloodVolume"),   TEXT("WaterVolume"),      TEXT("rise_rate: 0.2-2 m/min"), TEXT("volume_activated, level_reported"), true});
		Model.Manifest.Add({TEXT("CrowdEvac"),        TEXT("BP_EvacRoute"),     TEXT("Villager, Route"),  TEXT("group size: 5-60"),       TEXT("route_assigned, arrivals_counted"), true});
		Model.Manifest.Add({TEXT("ValveInteraction"), TEXT("BP_SpillwayValve"), TEXT("ValveProp"),        TEXT("turn_time: 3-10 s"),      TEXT("valve_turned, flow_changed"),       true});
		Model.Manifest.Add({TEXT("HelicopterRescue"), TEXT("(none registered)"),TEXT("-"),                TEXT("-"),                      TEXT("-"),                                false});

		FStoryNode D1 = MakeNode(TEXT("D1"), TEXT("Sirens at the dam"),
			TEXT("Flood sirens cut through the rain. The spillway gate has not opened, and the reservoir is climbing toward the crest."),
			{0.30f, 0.55f, 0.45f, 0.20f, 0.60f}, 0, 1);
		D1.Entities = {TEXT("player"), TEXT("dam"), TEXT("village")};
		D1.AddEffects = {TEXT("at(dam)"), TEXT("alarm(flood_warning)")};
		D1.GameplayBindings = {TEXT("siren ambience, rising water level")};
		D1.Provenance = {TEXT("approved root (Story Bible v2)")};
		D1.Choices = {MakeChoice(TEXT("c_d1_control"), TEXT("Climb to the control room"), TEXT("D2")),
		              MakeChoice(TEXT("c_d1_village"), TEXT("Head to the village square"), TEXT("D3"))};
		Model.Nodes.Add(D1);

		FStoryNode D2 = MakeNode(TEXT("D2"), TEXT("Control room"),
			TEXT("The gate console is dead, but the fault log is legible: the spillway gate is mechanically jammed, and there is a manual release below."),
			{0.35f, 0.65f, 0.55f, 0.50f, 0.70f}, 1, 0);
		D2.Preconditions = {TEXT("at(dam)")};
		D2.AddEffects = {TEXT("access(control_room)"), TEXT("knows(player, gate_jammed)")};
		D2.GameplayBindings = {TEXT("console interaction, fault log readout")};
		D2.PrimaryParentId = TEXT("D1");
		D2.PrimaryIncomingChoiceId = TEXT("c_d1_control");
		D2.Choices = {MakeChoice(TEXT("c_d2_release"), TEXT("Descend to the manual release"), TEXT("D4"))};
		Model.Nodes.Add(D2);

		FStoryNode D3 = MakeNode(TEXT("D3"), TEXT("Village square"),
			TEXT("Families are gathering with what they can carry. Someone has to decide where they go -- and whether anyone gambles on the dam holding."),
			{0.30f, 0.60f, 0.60f, 0.35f, 0.75f}, 1, 2);
		D3.Preconditions = {TEXT("at(dam)")};
		D3.AddEffects = {TEXT("organized(evacuation)")};
		D3.RequiredCapabilities = {TEXT("CrowdEvac")};
		D3.SelectedMapping.Add(TEXT("CrowdEvac"), TEXT("BP_EvacRoute"));
		D3.Grounding = EGroundingStatus::Implemented;
		D3.PrimaryParentId = TEXT("D1");
		D3.PrimaryIncomingChoiceId = TEXT("c_d1_village");
		// Branch-local gate: the village branch never learned the gate is
		// jammed, so the shortcut to the release is blocked (same pattern
		// as N6 -> N7 in the mystery brief).
		D3.Choices = {MakeChoice(TEXT("c_d3_ridge"), TEXT("Lead everyone to the ridge"), TEXT("D5")),
		              MakeChoice(TEXT("c_d3_release"), TEXT("Run for the manual release"), TEXT("D4"))};
		Model.Nodes.Add(D3);

		FStoryNode D4 = MakeNode(TEXT("D4"), TEXT("Manual release"),
			TEXT("Waist-deep in spray, the release wheel finally gives. The spillway roars open and the reservoir begins to fall."),
			{0.40f, 0.90f, 0.70f, 0.65f, 0.90f}, 2, 0);
		D4.Preconditions = {TEXT("knows(player, gate_jammed)")};
		D4.AddEffects = {TEXT("released(spillway)")};
		D4.RequiredCapabilities = {TEXT("ValveInteraction"), TEXT("FloodSimulation")};
		D4.SelectedMapping.Add(TEXT("ValveInteraction"), TEXT("BP_SpillwayValve"));
		D4.SelectedMapping.Add(TEXT("FloodSimulation"), TEXT("BP_FloodVolume"));
		D4.Grounding = EGroundingStatus::Implemented;
		D4.PrimaryParentId = TEXT("D2");
		D4.PrimaryIncomingChoiceId = TEXT("c_d2_release");
		D4.Choices = {MakeChoice(TEXT("c_d4_saved"), TEXT("Watch the water fall"), TEXT("D6"))};
		Model.Nodes.Add(D4);

		FStoryNode D5 = MakeNode(TEXT("D5"), TEXT("Ridge evacuation"),
			TEXT("From the ridge, the village lights go out one by one under the water. Everyone is alive. Almost nothing else is."),
			{0.25f, 0.60f, 0.65f, 0.75f, 0.85f}, 2, 2);
		D5.Preconditions = {TEXT("organized(evacuation)")};
		D5.bEnding = true;
		D5.PrimaryParentId = TEXT("D3");
		D5.PrimaryIncomingChoiceId = TEXT("c_d3_ridge");
		Model.Nodes.Add(D5);

		FStoryNode D6 = MakeNode(TEXT("D6"), TEXT("Village saved"),
			TEXT("The crest holds a hand's width above the waterline. In the square, the sirens finally stop."),
			{0.70f, 0.45f, 0.70f, 0.85f, 0.75f}, 3, 1);
		D6.Preconditions = {TEXT("released(spillway)")};
		D6.bEnding = true;
		D6.PrimaryParentId = TEXT("D4");
		D6.PrimaryIncomingChoiceId = TEXT("c_d4_saved");
		Model.Nodes.Add(D6);

		// One curated episode: an unsupported-capability decision in this
		// world's terms.
		{
			FAuthorizationEpisode E;
			E.EpisodeId = TEXT("ED1");
			E.Title = TEXT("Unsupported capability at D5");
			E.ProposalClass = TEXT("Capability");
			E.ProposalText = TEXT("'A rescue helicopter sweeps the ridge as the valley floods' -- requires HelicopterRescue.");
			E.Diagnostic = TEXT("HelicopterRescue has no registered implementation in the manifest.");
			E.TargetNodeId = TEXT("D5");
			E.bGateValid = false;
			E.Profile.Reversibility = EReversibility::Reversible;
			E.Profile.Scope = EDependencyScope::Local;
			E.Profile.bChangesNarrativeMeaning = true;
			E.Profile.ImplConsequence = EImplConsequence::Placeholder;
			E.Evidence.BranchState = {TEXT("at(dam)"), TEXT("alarm(flood_warning)"), TEXT("organized(evacuation)")};
			E.Evidence.AffectedRegion = {TEXT("D5")};
			E.Evidence.MappingLines = {TEXT("HelicopterRescue -> no registered implementation")};
			E.Evidence.AlternativeLines = {TEXT("Approve a visible placeholder"), TEXT("Record an implementation need"), TEXT("Reject")};

			FEpisodeOption Placeholder;
			Placeholder.OptionId = TEXT("ED1_placeholder");
			Placeholder.Label = TEXT("Approve visible placeholder");
			Placeholder.Description = TEXT("D5 carries an approved HelicopterRescue placeholder until an implementation is registered.");
			Placeholder.ActionKind = EProposalAction::Approve;
			{
				FGraphMutation A;
				A.Type = EGraphMutationType::SetGrounding;
				A.TargetNodeId = TEXT("D5");
				A.Grounding = EGroundingStatus::ApprovedPlaceholder;
				Placeholder.Mutations.Add(A);
				FGraphMutation B;
				B.Type = EGraphMutationType::AddCapabilityNeed;
				B.StringParamA = TEXT("HelicopterRescue for D5 ridge finale (owner unassigned)");
				Placeholder.Mutations.Add(B);
			}
			E.Options.Add(Placeholder);

			FEpisodeOption Reject;
			Reject.OptionId = TEXT("ED1_reject");
			Reject.Label = TEXT("Reject the enhancement");
			Reject.Description = TEXT("The ridge finale stays as authored.");
			Reject.ActionKind = EProposalAction::Reject;
			Reject.bRecommended = true;
			E.Options.Add(Reject);

			Model.Episodes.Add(E);
		}

		Model.SelectedNodeId = TEXT("D4");
		Model.LiveStatus = TEXT("Brief loaded: Dam Breach (disaster).");
		Model.Revalidate(TArray<FString>());
		Model.RouteAndAutoApply(TEXT("policy:Assisted"));
	}

	// ==================================================================
	// Brief 3: "Derelict Station" (science fiction). The sealed lab needs
	// restored power; the blood-trail branch reaches it first but cannot
	// license the transition.
	// ==================================================================
	void BuildDerelictStation(FContractModel& Model)
	{
		Model.ResetContent();
		Model.StoryTitle = TEXT("Derelict Station");
		Model.GenreLabel = TEXT("Science fiction");

		SetCurve(Model.Curves[0], ENarrativeAxis::Valence,     {0.35f, 0.30f, 0.25f, 0.35f, 0.50f});
		SetCurve(Model.Curves[1], ENarrativeAxis::Tension,     {0.30f, 0.50f, 0.70f, 0.90f, 0.65f});
		SetCurve(Model.Curves[2], ENarrativeAxis::Agency,      {0.55f, 0.60f, 0.50f, 0.70f, 0.75f});
		SetCurve(Model.Curves[3], ENarrativeAxis::Information, {0.10f, 0.35f, 0.60f, 0.80f, 0.90f});
		SetCurve(Model.Curves[4], ENarrativeAxis::Stakes,      {0.35f, 0.45f, 0.60f, 0.85f, 0.90f});

		Model.Manifest.Add({TEXT("PowerGrid"),      TEXT("BP_PowerGridController"), TEXT("GridSegment"),  TEXT("segments: 1-12"),      TEXT("segment_energized, lights_on"),    true});
		Model.Manifest.Add({TEXT("AirlockControl"), TEXT("BP_Airlock"),             TEXT("Door"),         TEXT("cycle_time: 2-6 s"),   TEXT("door_cycled, pressure_equalized"), true});
		Model.Manifest.Add({TEXT("EvidenceScan"),   TEXT("BP_ScannerProp"),         TEXT("ScannableProp"),TEXT("scan_time: 1-4 s"),    TEXT("scan_completed, entry_logged"),    true});
		Model.Manifest.Add({TEXT("SpecimenAI"),     TEXT("(none registered)"),      TEXT("-"),            TEXT("-"),                   TEXT("-"),                               false});

		FStoryNode S1 = MakeNode(TEXT("S1"), TEXT("Silent docking bay"),
			TEXT("The derelict accepts the docking clamp without a word. Emergency strips glow along a corridor that has not heard footsteps in years."),
			{0.35f, 0.30f, 0.55f, 0.10f, 0.35f}, 0, 1);
		S1.Entities = {TEXT("player"), TEXT("station")};
		S1.AddEffects = {TEXT("aboard(derelict)"), TEXT("suit(sealed)")};
		S1.GameplayBindings = {TEXT("airlock cycle, emergency lighting")};
		S1.Provenance = {TEXT("approved root (Story Bible v2)")};
		S1.Choices = {MakeChoice(TEXT("c_s1_power"), TEXT("Restore power at the reactor"), TEXT("S2")),
		              MakeChoice(TEXT("c_s1_trail"), TEXT("Follow the dried blood trail"), TEXT("S3"))};
		Model.Nodes.Add(S1);

		FStoryNode S2 = MakeNode(TEXT("S2"), TEXT("Reactor room"),
			TEXT("The reactor takes the restart sequence grudgingly. Deck by deck, the station wakes -- including whatever else is aboard."),
			{0.40f, 0.55f, 0.60f, 0.40f, 0.50f}, 1, 0);
		S2.Preconditions = {TEXT("aboard(derelict)")};
		S2.AddEffects = {TEXT("power(restored)")};
		S2.RequiredCapabilities = {TEXT("PowerGrid")};
		S2.SelectedMapping.Add(TEXT("PowerGrid"), TEXT("BP_PowerGridController"));
		S2.Grounding = EGroundingStatus::Implemented;
		S2.PrimaryParentId = TEXT("S1");
		S2.PrimaryIncomingChoiceId = TEXT("c_s1_power");
		S2.Choices = {MakeChoice(TEXT("c_s2_lab"), TEXT("Open the sealed lab"), TEXT("S4"))};
		Model.Nodes.Add(S2);

		FStoryNode S3 = MakeNode(TEXT("S3"), TEXT("Blood trail"),
			TEXT("The trail ends at a maintenance log and a name scratched into the bulkhead. Now you know what happened to the crew -- most of them."),
			{0.25f, 0.50f, 0.45f, 0.55f, 0.55f}, 1, 2);
		S3.Preconditions = {TEXT("aboard(derelict)")};
		S3.AddEffects = {TEXT("knows(player, crew_fate)")};
		S3.RequiredCapabilities = {TEXT("EvidenceScan")};
		S3.SelectedMapping.Add(TEXT("EvidenceScan"), TEXT("BP_ScannerProp"));
		S3.Grounding = EGroundingStatus::Implemented;
		S3.PrimaryParentId = TEXT("S1");
		S3.PrimaryIncomingChoiceId = TEXT("c_s1_trail");
		// Blocked without power: the lab door is dead on this branch.
		S3.Choices = {MakeChoice(TEXT("c_s3_lab"), TEXT("Reach the sealed lab"), TEXT("S4"))};
		Model.Nodes.Add(S3);

		FStoryNode S4 = MakeNode(TEXT("S4"), TEXT("Sealed lab"),
			TEXT("Behind the pressurized door: a specimen container, still active, and research notes that should never have left the ground."),
			{0.30f, 0.85f, 0.60f, 0.80f, 0.85f}, 2, 1);
		S4.Preconditions = {TEXT("power(restored)")};
		S4.AddEffects = {TEXT("found(specimen)")};
		S4.RequiredCapabilities = {TEXT("AirlockControl")};
		S4.SelectedMapping.Add(TEXT("AirlockControl"), TEXT("BP_Airlock"));
		S4.Grounding = EGroundingStatus::Implemented;
		S4.PrimaryParentId = TEXT("S2");
		S4.PrimaryIncomingChoiceId = TEXT("c_s2_lab");
		FStoryChoice Purge = MakeChoice(TEXT("c_s4_purge"), TEXT("Purge the lab into vacuum"), TEXT("S5"));
		Purge.AddEffects = {TEXT("purged(lab)")};
		FStoryChoice Take = MakeChoice(TEXT("c_s4_take"), TEXT("Take the specimen aboard"), TEXT("S6"));
		Take.AddEffects = {TEXT("carrying(specimen)")};
		S4.Choices = {Purge, Take};
		Model.Nodes.Add(S4);

		FStoryNode S5 = MakeNode(TEXT("S5"), TEXT("Clean vacuum"),
			TEXT("The lab vents in one long breath. Whatever the researchers found stays between the stars."),
			{0.55f, 0.55f, 0.75f, 0.85f, 0.80f}, 3, 0);
		S5.Preconditions = {TEXT("purged(lab)")};
		S5.bEnding = true;
		S5.PrimaryParentId = TEXT("S4");
		S5.PrimaryIncomingChoiceId = TEXT("c_s4_purge");
		Model.Nodes.Add(S5);

		FStoryNode S6 = MakeNode(TEXT("S6"), TEXT("Cargo of consequence"),
			TEXT("The container rides home in your hold, humming softly. The paycheck is enormous. The decision is not over."),
			{0.40f, 0.70f, 0.70f, 0.85f, 0.95f}, 3, 2);
		S6.Preconditions = {TEXT("carrying(specimen)")};
		S6.bEnding = true;
		S6.PrimaryParentId = TEXT("S4");
		S6.PrimaryIncomingChoiceId = TEXT("c_s4_take");
		Model.Nodes.Add(S6);

		// One curated episode: an irreversible deletion in this world.
		{
			FAuthorizationEpisode E;
			E.EpisodeId = TEXT("ES1");
			E.Title = TEXT("Remove ending S6 'Cargo of consequence'");
			E.ProposalClass = TEXT("Deletion");
			E.ProposalText = TEXT("Cut the take-the-specimen ending to keep the finale unambiguous.");
			E.Diagnostic = TEXT("Removes a reachable ending; irreversible for every path through c_s4_take.");
			E.TargetNodeId = TEXT("S6");
			E.bGateValid = true;
			E.Profile.Reversibility = EReversibility::Irreversible;
			E.Profile.Scope = EDependencyScope::Global;
			E.Profile.bChangesNarrativeMeaning = true;
			E.Profile.bChangesReachability = true;
			E.Evidence.BranchState = {TEXT("found(specimen)"), TEXT("carrying(specimen)")};
			E.Evidence.AffectedRegion = {TEXT("S4"), TEXT("S6")};
			E.Evidence.AlternativeLines = {TEXT("Remove S6 and its incoming choice"), TEXT("Retain both endings")};

			FEpisodeOption Remove;
			Remove.OptionId = TEXT("ES1_remove");
			Remove.Label = TEXT("Approve removal of S6");
			Remove.Description = TEXT("Irreversible: the specimen ending leaves the playable graph.");
			Remove.ActionKind = EProposalAction::Approve;
			{
				FGraphMutation M;
				M.Type = EGraphMutationType::RemoveNode;
				M.TargetNodeId = TEXT("S6");
				Remove.Mutations.Add(M);
			}
			E.Options.Add(Remove);

			FEpisodeOption Keep;
			Keep.OptionId = TEXT("ES1_keep");
			Keep.Label = TEXT("Reject: retain both endings");
			Keep.Description = TEXT("Keeps S6; records the editorial suggestion.");
			Keep.ActionKind = EProposalAction::Reject;
			Keep.bRecommended = true;
			E.Options.Add(Keep);

			Model.Episodes.Add(E);
		}

		Model.SelectedNodeId = TEXT("S4");
		Model.LiveStatus = TEXT("Brief loaded: Derelict Station (science fiction).");
		Model.Revalidate(TArray<FString>());
		Model.RouteAndAutoApply(TEXT("policy:Assisted"));
	}

	void BuildHydro(FContractModel& Model)
	{
		Model.BuildSampleData(true);
		Model.LiveStatus = TEXT("Brief loaded: Hydro-Station Mystery.");
	}
}

// ===========================================================================
// Registry and JSON briefs
// ===========================================================================

const TArray<FBriefDescriptor>& Briefs::BuiltIn()
{
	static TArray<FBriefDescriptor> Registry = []()
	{
		TArray<FBriefDescriptor> List;
		List.Add({TEXT("hydro"), TEXT("Hydro-Station Mystery"), TEXT("Mystery"),
			TEXT("The Figure 4 sample: blocked confrontation, four curated episodes, full demo path."),
			&BuildHydro});
		List.Add({TEXT("dam"), TEXT("Dam Breach"), TEXT("Disaster"),
			TEXT("Race the reservoir: the jammed-gate fact is branch-local, so the village shortcut to the release is blocked."),
			&BuildDamBreach});
		List.Add({TEXT("derelict"), TEXT("Derelict Station"), TEXT("Science fiction"),
			TEXT("Power before entry: the sealed lab refuses the blood-trail branch until the reactor branch restores the grid."),
			&BuildDerelictStation});
		return List;
	}();
	return Registry;
}

FString Briefs::BriefsDir()
{
	const FString Dir = FPaths::ProjectDir() / TEXT("Briefs");
	IFileManager::Get().MakeDirectory(*Dir, true);
	return Dir;
}

TArray<FString> Briefs::DiscoverJsonBriefs()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(BriefsDir() / TEXT("*.json")), true, false);
	Files.Sort();
	return Files;
}

FString Briefs::LoadJsonBrief(FContractModel& Model, const FString& FileName)
{
	FString Json;
	const FString Path = BriefsDir() / FileName;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return FString::Printf(TEXT("Brief load failed: cannot read %s."), *FileName);
	}
	FString Error;
	if (!ContractSerialization::LoadFromJsonString(Model, Json, Error))
	{
		return FString::Printf(TEXT("Brief load failed: %s."), *Error);
	}
	return FString::Printf(TEXT("Brief loaded from %s: %s (graph v%d)."),
		*FileName, *Model.StoryTitle, Model.Versions.GraphVersion);
}

FString Briefs::SaveCurrentAsBrief(const FContractModel& Model)
{
	FString Slug = Model.StoryTitle.ToLower();
	const FString Allowed = TEXT("abcdefghijklmnopqrstuvwxyz0123456789");
	FString Clean;
	for (TCHAR C : Slug)
	{
		if (Allowed.Contains(FString::Chr(C)))
		{
			Clean.AppendChar(C);
		}
		else if (Clean.Len() > 0 && !Clean.EndsWith(TEXT("-")))
		{
			Clean.AppendChar(TEXT('-'));
		}
	}
	if (Clean.IsEmpty())
	{
		Clean = TEXT("brief");
	}

	// Non-colliding filename.
	FString FileName = Clean + TEXT(".json");
	int32 Suffix = 2;
	while (IFileManager::Get().FileExists(*(BriefsDir() / FileName)))
	{
		FileName = FString::Printf(TEXT("%s-%d.json"), *Clean, Suffix++);
	}

	const FString Json = ContractSerialization::SaveToJsonString(Model);
	if (!FFileHelper::SaveStringToFile(Json, *(BriefsDir() / FileName)))
	{
		return TEXT("Brief save failed: could not write to the Briefs folder.");
	}
	return FString::Printf(TEXT("Saved current state as brief Briefs/%s."), *FileName);
}
