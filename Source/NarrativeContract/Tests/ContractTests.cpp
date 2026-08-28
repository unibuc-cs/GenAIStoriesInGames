// Functional tests over the contract model, using the curated episode queue
// as fixtures with known expected outcomes. Run from the editor via
// Tools > Session Frontend > Automation (filter "NarrativeContract"), or
// headless:
//   UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests NarrativeContract; Quit" -unattended -nopause -nullrhi -log

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Core/ContractModel.h"
#include "../Core/LlmProposals.h"
#include "../Core/ContractSerialization.h"
#include "../Core/Scenarios.h"

namespace
{
	const FStoryChoice* FindChoiceTo(const FContractModel& Model, const FString& FromId, const FString& ToId)
	{
		if (const FStoryNode* From = Model.FindNode(FromId))
		{
			for (const FStoryChoice& Choice : From->Choices)
			{
				if (Choice.TargetNodeId == ToId)
				{
					return &Choice;
				}
			}
		}
		return nullptr;
	}
}

#define NC_TEST(ClassName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, \
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// ---------------------------------------------------------------------------
// Gate: the Figure 4 state -- N6->N7 blocked, N5->N7 licensed
// ---------------------------------------------------------------------------
NC_TEST(FNCBlockedEdgeTest, "NarrativeContract.Gate.BlockedEdgeDiagnosis")
bool FNCBlockedEdgeTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	const FStoryChoice* N6toN7 = FindChoiceTo(Model, TEXT("N6"), TEXT("N7"));
	const FStoryChoice* N5toN7 = FindChoiceTo(Model, TEXT("N5"), TEXT("N7"));

	TestNotNull(TEXT("N6->N7 exists"), N6toN7);
	TestNotNull(TEXT("N5->N7 exists"), N5toN7);
	if (!N6toN7 || !N5toN7)
	{
		return false;
	}
	TestTrue(TEXT("N6->N7 blocked (knowledge precondition unmet on locker branch)"), N6toN7->bBlocked);
	TestTrue(TEXT("diagnostic names the missing predicate"),
		N6toN7->BlockDiagnostic.Contains(TEXT("knows(player, sabotage_signature)")));
	TestFalse(TEXT("N5->N7 licensed (sibling branch carries the fact)"), N5toN7->bBlocked);

	const FStoryNode* N7 = Model.FindNode(TEXT("N7"));
	TestTrue(TEXT("N7 flagged for review on its primary path"),
		N7 && N7->Status == ENodeStatus::NeedsReview);
	return true;
}

// ---------------------------------------------------------------------------
// E1 approve-insert: repair + scoped revalidation (Figure 4 after-state)
// ---------------------------------------------------------------------------
NC_TEST(FNCRepairInsertTest, "NarrativeContract.Episodes.E1RepairInsertScopedRevalidation")
bool FNCRepairInsertTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);
	const int32 VersionBefore = Model.Versions.GraphVersion;

	Model.ResolveEpisode(TEXT("E1"), TEXT("E1_insert"), EProposalAction::Approve, TEXT("test"));

	const FStoryNode* N6b = Model.FindNode(TEXT("N6b"));
	TestNotNull(TEXT("N6b inserted"), N6b);
	TestNotNull(TEXT("N6 rewired to N6b"), FindChoiceTo(Model, TEXT("N6"), TEXT("N6b")));
	TestNotNull(TEXT("N6b continues to N7"), FindChoiceTo(Model, TEXT("N6b"), TEXT("N7")));
	TestNull(TEXT("direct N6->N7 edge gone"), FindChoiceTo(Model, TEXT("N6"), TEXT("N7")));

	const FStoryChoice* N6toN6b = FindChoiceTo(Model, TEXT("N6"), TEXT("N6b"));
	const FStoryChoice* N6bToN7 = FindChoiceTo(Model, TEXT("N6b"), TEXT("N7"));
	TestTrue(TEXT("incident edge revalidated clean"), N6toN6b && !N6toN6b->bBlocked);
	TestTrue(TEXT("repaired transition licensed"), N6bToN7 && !N6bToN7->bBlocked);

	const FStoryNode* N7 = Model.FindNode(TEXT("N7"));
	TestTrue(TEXT("N7 valid after repair"), N7 && N7->Status == ENodeStatus::Valid);
	TestEqual(TEXT("accepted edit bumps the graph version"), Model.Versions.GraphVersion, VersionBefore + 1);

	// Eq. 13: boundary covers the touched closure, spares the siblings.
	TestTrue(TEXT("boundary holds N6b"), Model.LastRevalidationBoundary.Contains(TEXT("N6b")));
	TestTrue(TEXT("boundary holds N7"), Model.LastRevalidationBoundary.Contains(TEXT("N7")));
	TestTrue(TEXT("boundary holds N8"), Model.LastRevalidationBoundary.Contains(TEXT("N8")));
	TestFalse(TEXT("N1 outside boundary"), Model.LastRevalidationBoundary.Contains(TEXT("N1")));
	TestFalse(TEXT("N4 outside boundary"), Model.LastRevalidationBoundary.Contains(TEXT("N4")));
	TestFalse(TEXT("N5 retains its earlier record"), Model.LastRevalidationBoundary.Contains(TEXT("N5")));
	return true;
}

// ---------------------------------------------------------------------------
// E1 edit-weaken: dropping the precondition also unblocks the edge
// ---------------------------------------------------------------------------
NC_TEST(FNCWeakenTest, "NarrativeContract.Episodes.E1WeakenPrecondition")
bool FNCWeakenTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	Model.ResolveEpisode(TEXT("E1"), TEXT("E1_weaken"), EProposalAction::Edit, TEXT("test"));

	const FStoryNode* N7 = Model.FindNode(TEXT("N7"));
	TestTrue(TEXT("knowledge gate removed"),
		N7 && !N7->Preconditions.Contains(TEXT("knows(player, sabotage_signature)")));
	const FStoryChoice* N6toN7 = FindChoiceTo(Model, TEXT("N6"), TEXT("N7"));
	TestTrue(TEXT("edge unblocked after the edit"), N6toN7 && !N6toN7->bBlocked);
	return true;
}

// ---------------------------------------------------------------------------
// Policy routing (Table 3 semantics)
// ---------------------------------------------------------------------------
NC_TEST(FNCRoutingTest, "NarrativeContract.Policy.RoutingPerPolicy")
bool FNCRoutingTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	const FAuthorizationEpisode* E1 = Model.FindEpisode(TEXT("E1"));
	const FAuthorizationEpisode* E2 = Model.FindEpisode(TEXT("E2"));
	const FAuthorizationEpisode* E3 = Model.FindEpisode(TEXT("E3"));
	const FAuthorizationEpisode* E4 = Model.FindEpisode(TEXT("E4"));
	if (!E1 || !E2 || !E3 || !E4)
	{
		TestTrue(TEXT("all curated episodes present"), false);
		return false;
	}

	Model.Policy = EAuthorizationPolicy::Automatic;
	TestEqual(TEXT("Automatic: gate-invalid repair still reviews"), Model.RouteEpisode(*E1), EPolicyRoute::Review);
	TestEqual(TEXT("Automatic: registered normalization auto"), Model.RouteEpisode(*E2), EPolicyRoute::Auto);
	TestEqual(TEXT("Automatic: placeholder always reviews"), Model.RouteEpisode(*E3), EPolicyRoute::Review);
	TestEqual(TEXT("Automatic: other gate-valid changes auto"), Model.RouteEpisode(*E4), EPolicyRoute::Auto);

	Model.Policy = EAuthorizationPolicy::Assisted;
	TestEqual(TEXT("Assisted: registered normalization auto"), Model.RouteEpisode(*E2), EPolicyRoute::Auto);
	TestEqual(TEXT("Assisted: irreversible deletion reviews"), Model.RouteEpisode(*E4), EPolicyRoute::Review);

	Model.Policy = EAuthorizationPolicy::Strict;
	TestEqual(TEXT("Strict: even the normalization reviews"), Model.RouteEpisode(*E2), EPolicyRoute::Review);
	TestEqual(TEXT("Strict: deletion reviews"), Model.RouteEpisode(*E4), EPolicyRoute::Review);
	return true;
}

// ---------------------------------------------------------------------------
// Startup auto-apply under Assisted logs E2 and leaves three pending
// ---------------------------------------------------------------------------
NC_TEST(FNCAutoApplyTest, "NarrativeContract.Policy.AssistedAutoAppliesNormalization")
bool FNCAutoApplyTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(true);

	const FAuthorizationEpisode* E2 = Model.FindEpisode(TEXT("E2"));
	TestTrue(TEXT("E2 auto-applied"), E2 && E2->ResolvedAction == EProposalAction::AutoApplied);
	TestEqual(TEXT("three episodes pending"), Model.NumPendingEpisodes(), 3);
	TestTrue(TEXT("automation is logged, inspectable"), Model.DecisionLog.Num() >= 1);

	const FStoryChoice* Renamed = Model.FindNode(TEXT("N4")) ? &Model.FindNode(TEXT("N4"))->Choices[0] : nullptr;
	TestTrue(TEXT("canonical label applied"), Renamed && Renamed->Label == TEXT("Read the maintenance log"));
	return true;
}

// ---------------------------------------------------------------------------
// E3 placeholder: explicit production commitment
// ---------------------------------------------------------------------------
NC_TEST(FNCPlaceholderTest, "NarrativeContract.Episodes.E3PlaceholderCommitment")
bool FNCPlaceholderTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);
	const int32 ManifestBefore = Model.Versions.EngineCapabilityManifest;

	Model.ResolveEpisode(TEXT("E3"), TEXT("E3_placeholder"), EProposalAction::Approve, TEXT("test"));

	const FStoryNode* N7 = Model.FindNode(TEXT("N7"));
	TestTrue(TEXT("N7 carries an approved placeholder"),
		N7 && N7->Grounding == EGroundingStatus::ApprovedPlaceholder);
	TestEqual(TEXT("implementation need recorded"), Model.ImplementationNeeds.Num(), 1);
	TestEqual(TEXT("manifest version advanced"), Model.Versions.EngineCapabilityManifest, ManifestBefore + 1);
	return true;
}

// ---------------------------------------------------------------------------
// E4 deletion: node leaves the live graph
// ---------------------------------------------------------------------------
NC_TEST(FNCDeletionTest, "NarrativeContract.Episodes.E4RemoveEnding")
bool FNCDeletionTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	Model.ResolveEpisode(TEXT("E4"), TEXT("E4_remove"), EProposalAction::Approve, TEXT("test"));

	const FStoryNode* N9 = Model.FindNode(TEXT("N9"));
	TestTrue(TEXT("N9 marked removed"), N9 && N9->Status == ENodeStatus::Removed);
	TestFalse(TEXT("N9 no longer a live child of N7"), Model.ChildNodeIds(TEXT("N7")).Contains(TEXT("N9")));
	return true;
}

// ---------------------------------------------------------------------------
// Curves: adherence bounds and logged edits
// ---------------------------------------------------------------------------
NC_TEST(FNCCurvesTest, "NarrativeContract.Curves.AdherenceAndLoggedEdits")
bool FNCCurvesTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	const float Adh = Model.ComputePathAdherence(TEXT("N8"));
	TestTrue(TEXT("adherence in [0,1]"), Adh >= 0.f && Adh <= 1.f);

	const int32 BibleBefore = Model.Versions.StoryBible;
	Model.SetApprovedCurveValue(ENarrativeAxis::Tension, 2, 1.7f, TEXT("test"));
	TestEqual(TEXT("edit logged"), Model.CurveEdits.Num(), 1);
	TestEqual(TEXT("value clamped to [0,1]"), Model.Curves[1].Approved[2], 1.f);
	TestEqual(TEXT("Story Bible version advanced"), Model.Versions.StoryBible, BibleBefore + 1);
	TestTrue(TEXT("deviation outside the band is visible, not concealed"),
		Model.Curves[1].IsHandleOutsideBand(2));
	return true;
}

// ---------------------------------------------------------------------------
// Runtime parity: the demo's gate mirrors the validator (RQ3 in miniature)
// ---------------------------------------------------------------------------
NC_TEST(FNCRuntimeParityTest, "NarrativeContract.Runtime.GateParityAcrossRepair")
bool FNCRuntimeParityTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	// Clean-state replay of the locker path, exactly as the demo applies it.
	auto Replay = [&Model](const TArray<FString>& Path)
	{
		TSet<FString> Facts;
		for (const FString& Id : Path)
		{
			if (const FStoryNode* Node = Model.FindNode(Id))
			{
				for (const FString& Del : Node->DelEffects) { Facts.Remove(Del); }
				for (const FString& Add : Node->AddEffects) { Facts.Add(Add); }
			}
		}
		return Facts;
	};

	const TSet<FString> PreRepair = Replay({TEXT("N1"), TEXT("N4"), TEXT("N6")});
	TestFalse(TEXT("pre-repair: runtime facts cannot license N7"),
		PreRepair.Contains(TEXT("knows(player, sabotage_signature)")));

	Model.ResolveEpisode(TEXT("E1"), TEXT("E1_insert"), EProposalAction::Approve, TEXT("test"));
	const TSet<FString> PostRepair = Replay({TEXT("N1"), TEXT("N4"), TEXT("N6"), TEXT("N6b")});
	TestTrue(TEXT("post-repair: decoding licenses the confrontation"),
		PostRepair.Contains(TEXT("knows(player, sabotage_signature)")));
	return true;
}

// ---------------------------------------------------------------------------
// Live source: parser and extractor/validator on canned replies (no network)
// ---------------------------------------------------------------------------
NC_TEST(FNCLlmParseTest, "NarrativeContract.Live.ParseAndValidateProposal")
bool FNCLlmParseTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	const FString Canned = TEXT("```json\n{\"title\":\"Question the foreman\",")
		TEXT("\"description\":\"The night foreman saw someone at valve 7.\",")
		TEXT("\"choice_label\":\"Find the night foreman\",")
		TEXT("\"preconditions\":[\"at(hydro_station)\"],")
		TEXT("\"add_effects\":[\"knows(player, foreman_account)\"],")
		TEXT("\"del_effects\":[],")
		TEXT("\"required_capabilities\":[\"ConfrontDialogue\"],")
		TEXT("\"axis_estimates\":{\"valence\":0.4,\"tension\":0.5,\"agency\":0.6,\"information\":0.7,\"stakes\":0.4},")
		TEXT("\"is_ending\":false,\"rationale\":\"raises information mid-arc\"}\n```");

	FLlmProposal Proposal;
	FString Error;
	TestTrue(TEXT("canned reply parses despite the fence"), LlmProposals::ParseProposalJson(Canned, Proposal, Error));
	TestEqual(TEXT("title extracted"), Proposal.Title, FString(TEXT("Question the foreman")));
	TestEqual(TEXT("capability extracted"), Proposal.RequiredCapabilities.Num(), 1);

	FAuthorizationEpisode Episode = LlmProposals::BuildEpisode(Model, TEXT("N5"), Proposal);
	TestTrue(TEXT("gate valid: precondition holds on the N5 branch"), Episode.bGateValid);
	TestTrue(TEXT("new predicate flagged as a new-label commitment"), Episode.Profile.bIntroducesNewLabel);
	TestEqual(TEXT("registered capability grounds the mapping"),
		Episode.Profile.ImplConsequence, EImplConsequence::RegisteredMapping);

	// Accepting the episode appends the node under the expansion point.
	Model.AddEpisode(Episode);
	FAuthorizationEpisode* Queued = Model.FindEpisode(Episode.EpisodeId);
	TestNotNull(TEXT("episode queued"), Queued);
	if (Queued && Queued->ResolvedAction == EProposalAction::None)
	{
		Model.ResolveEpisode(Queued->EpisodeId, Queued->Options[0].OptionId, EProposalAction::Approve, TEXT("test"));
	}
	const FStoryNode* L1 = Model.FindNode(TEXT("L1"));
	TestNotNull(TEXT("generated node admitted"), L1);
	TestNotNull(TEXT("edge from expansion node exists"), FindChoiceTo(Model, TEXT("N5"), TEXT("L1")));

	// Malformed reply is refused with a diagnostic, not admitted.
	FLlmProposal Bad;
	TestFalse(TEXT("malformed reply rejected"), LlmProposals::ParseProposalJson(TEXT("sorry, no json here"), Bad, Error));
	TestFalse(TEXT("parser explains the refusal"), Error.IsEmpty());
	return true;
}

// ---------------------------------------------------------------------------
// Live source: unregistered capability becomes a placeholder decision
// ---------------------------------------------------------------------------
NC_TEST(FNCLlmPlaceholderTest, "NarrativeContract.Live.UnregisteredCapabilityRoutesToPlaceholder")
bool FNCLlmPlaceholderTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	FLlmProposal Proposal;
	Proposal.Title = TEXT("Crowd blocks the gate");
	Proposal.Description = TEXT("Off-shift workers mass at the entrance.");
	Proposal.ChoiceLabel = TEXT("Push through the crowd");
	Proposal.Preconditions = {TEXT("at(hydro_station)")};
	Proposal.RequiredCapabilities = {TEXT("CrowdSystem")};

	FAuthorizationEpisode Episode = LlmProposals::BuildEpisode(Model, TEXT("N1"), Proposal);
	TestFalse(TEXT("unregistered capability fails the gate"), Episode.bGateValid);
	TestEqual(TEXT("consequence classified as placeholder"),
		Episode.Profile.ImplConsequence, EImplConsequence::Placeholder);

	Model.Policy = EAuthorizationPolicy::Automatic;
	TestEqual(TEXT("even Automatic must pause for a placeholder"),
		Model.RouteEpisode(Episode), EPolicyRoute::Review);

	bool bHasPlaceholderOption = false;
	for (const FEpisodeOption& Option : Episode.Options)
	{
		if (Option.Label.Contains(TEXT("placeholder")))
		{
			bHasPlaceholderOption = true;
		}
	}
	TestTrue(TEXT("bounded response offers a visible placeholder"), bHasPlaceholderOption);
	return true;
}

// ---------------------------------------------------------------------------
// Persistence: full round-trip through JSON (v4)
// ---------------------------------------------------------------------------
NC_TEST(FNCSerializationRoundTripTest, "NarrativeContract.Persistence.JsonRoundTrip")
bool FNCSerializationRoundTripTest::RunTest(const FString& Parameters)
{
	FContractModel Source;
	Source.BuildSampleData(false);
	Source.ResolveEpisode(TEXT("E1"), TEXT("E1_insert"), EProposalAction::Approve, TEXT("test"));
	Source.SetApprovedCurveValue(ENarrativeAxis::Tension, 3, 0.9f, TEXT("test"));

	const FString Json = ContractSerialization::SaveToJsonString(Source);
	TestTrue(TEXT("serialized state is non-trivial"), Json.Len() > 1000);

	FContractModel Loaded;
	FString Error;
	TestTrue(TEXT("state loads back"), ContractSerialization::LoadFromJsonString(Loaded, Json, Error));
	TestTrue(TEXT("no load error"), Error.IsEmpty());

	TestEqual(TEXT("node count preserved"), Loaded.Nodes.Num(), Source.Nodes.Num());
	TestNotNull(TEXT("repair node survives the trip"), Loaded.FindNode(TEXT("N6b")));
	TestEqual(TEXT("graph version preserved"), Loaded.Versions.GraphVersion, Source.Versions.GraphVersion);
	TestEqual(TEXT("decision log preserved"), Loaded.DecisionLog.Num(), Source.DecisionLog.Num());
	TestEqual(TEXT("curve edit preserved"), Loaded.CurveEdits.Num(), Source.CurveEdits.Num());
	TestEqual(TEXT("edited handle value preserved"), Loaded.Curves[1].Approved[3], 0.9f);
	TestEqual(TEXT("manifest preserved"), Loaded.Manifest.Num(), Source.Manifest.Num());

	const FAuthorizationEpisode* E1 = Loaded.FindEpisode(TEXT("E1"));
	TestTrue(TEXT("episode resolution preserved"), E1 && E1->ResolvedAction == EProposalAction::Approve);

	// Derived validation state is recomputed on load, not trusted from disk.
	const FStoryChoice* N6toN6b = FindChoiceTo(Loaded, TEXT("N6"), TEXT("N6b"));
	TestTrue(TEXT("repaired edge licensed after reload"), N6toN6b && !N6toN6b->bBlocked);
	const FStoryNode* N7 = Loaded.FindNode(TEXT("N7"));
	TestTrue(TEXT("N7 valid after reload"), N7 && N7->Status == ENodeStatus::Valid);

	FContractModel Rejecting;
	TestFalse(TEXT("garbage input refused"), ContractSerialization::LoadFromJsonString(Rejecting, TEXT("not json"), Error));
	return true;
}

// ---------------------------------------------------------------------------
// Frontier: candidate parsing and ranking (v4)
// ---------------------------------------------------------------------------
NC_TEST(FNCCandidateRankingTest, "NarrativeContract.Live.CandidateParsingAndRanking")
bool FNCCandidateRankingTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	// Two candidates: one hugging the approved targets, one far off.
	const FString Canned = TEXT("{\"candidates\":[")
		TEXT("{\"title\":\"Off target\",\"description\":\"d\",\"choice_label\":\"a\",")
		TEXT("\"preconditions\":[],\"add_effects\":[],\"del_effects\":[],\"required_capabilities\":[],")
		TEXT("\"axis_estimates\":{\"valence\":0.0,\"tension\":0.0,\"agency\":0.0,\"information\":0.0,\"stakes\":0.0},")
		TEXT("\"is_ending\":false,\"rationale\":\"r\"},")
		TEXT("{\"title\":\"On target\",\"description\":\"d\",\"choice_label\":\"b\",")
		TEXT("\"preconditions\":[],\"add_effects\":[],\"del_effects\":[],\"required_capabilities\":[],")
		TEXT("\"axis_estimates\":{\"valence\":0.5,\"tension\":0.6,\"agency\":0.6,\"information\":0.5,\"stakes\":0.5},")
		TEXT("\"is_ending\":false,\"rationale\":\"r\"}]}");

	TArray<FLlmProposal> Candidates;
	FString Error;
	TestTrue(TEXT("candidates array parses"), LlmProposals::ParseCandidatesJson(Canned, Candidates, Error));
	TestEqual(TEXT("both candidates read"), Candidates.Num(), 2);

	const float ScoreOff = LlmProposals::ScoreProposal(Model, TEXT("N4"), Candidates[0]);
	const float ScoreOn = LlmProposals::ScoreProposal(Model, TEXT("N4"), Candidates[1]);
	TestTrue(TEXT("target-fitting candidate ranks higher"), ScoreOn > ScoreOff);

	// Single-object fallback still works.
	TArray<FLlmProposal> Single;
	const FString Bare = TEXT("{\"title\":\"Solo\",\"description\":\"d\",\"choice_label\":\"c\",")
		TEXT("\"preconditions\":[],\"add_effects\":[],\"del_effects\":[],\"required_capabilities\":[],")
		TEXT("\"axis_estimates\":{\"valence\":0.5,\"tension\":0.5,\"agency\":0.5,\"information\":0.5,\"stakes\":0.5},")
		TEXT("\"is_ending\":false,\"rationale\":\"r\"}");
	TestTrue(TEXT("bare object accepted"), LlmProposals::ParseCandidatesJson(Bare, Single, Error));
	TestEqual(TEXT("one proposal from bare object"), Single.Num(), 1);

	// Ranked alternatives surface in the queued episode's evidence.
	TArray<FString> Ranked = {TEXT("rank 1: 'On target' score 0.90 (this proposal)"), TEXT("rank 2: 'Off target' score 0.40 (not queued)")};
	FAuthorizationEpisode Episode = LlmProposals::BuildEpisode(Model, TEXT("N4"), Candidates[1], Ranked);
	bool bHasRankLine = false;
	for (const FString& Line : Episode.Evidence.AlternativeLines)
	{
		if (Line.Contains(TEXT("rank 2")))
		{
			bHasRankLine = true;
		}
	}
	TestTrue(TEXT("sibling candidates recorded as reviewed alternatives"), bHasRankLine);
	return true;
}

// ---------------------------------------------------------------------------
// Frontier: expansion-point selection honors the graph shape (v4)
// ---------------------------------------------------------------------------
NC_TEST(FNCFrontierSelectionTest, "NarrativeContract.Live.FrontierSelection")
bool FNCFrontierSelectionTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);

	const TArray<FString> Frontier = Model.FrontierNodeIds();
	TestFalse(TEXT("full-choice root not expandable"), Frontier.Contains(TEXT("N1")));
	TestFalse(TEXT("full-choice N7 not expandable"), Frontier.Contains(TEXT("N7")));
	TestFalse(TEXT("endings not expandable"), Frontier.Contains(TEXT("N8")) || Frontier.Contains(TEXT("N9")));
	TestTrue(TEXT("single-choice N4 expandable"), Frontier.Contains(TEXT("N4")));
	TestTrue(TEXT("single-choice N5 expandable"), Frontier.Contains(TEXT("N5")));
	if (Frontier.Num() >= 2)
	{
		const FStoryNode* FirstNode = Model.FindNode(Frontier[0]);
		const FStoryNode* LastNode = Model.FindNode(Frontier.Last());
		TestTrue(TEXT("oldest (shallowest) paths come first"),
			FirstNode && LastNode && FirstNode->Depth <= LastNode->Depth);
	}
	TestTrue(TEXT("live node count within budget"), Model.NumLiveNodes() < FContractModel::LiveNodeBudget);
	return true;
}

// ---------------------------------------------------------------------------
// Scenario suite: the same end-to-end scenarios the in-app Tests panel
// runs, executed headless (v5)
// ---------------------------------------------------------------------------
NC_TEST(FNCScenarioSuiteTest, "NarrativeContract.Scenarios.AllScenariosPass")
bool FNCScenarioSuiteTest::RunTest(const FString& Parameters)
{
	for (const FScenario& Scenario : Scenarios::All())
	{
		const FScenarioResult Result = Scenarios::Run(Scenario);
		if (!Result.bAllPassed)
		{
			for (const FScenarioStepResult& Step : Result.Steps)
			{
				if (!Step.bPassed)
				{
					AddError(FString::Printf(TEXT("scenario '%s' failed at step '%s': %s"),
						*Scenario.Name, *Step.Description, *Step.Detail));
				}
			}
		}
		TestTrue(FString::Printf(TEXT("scenario '%s' passes (%d/%d steps)"),
			*Scenario.Name, Result.NumPassed(), Result.Steps.Num()), Result.bAllPassed);
	}
	TestTrue(TEXT("scenario registry is not empty"), Scenarios::All().Num() >= 8);
	return true;
}

// ---------------------------------------------------------------------------
// Undo + telemetry basics (v6)
// ---------------------------------------------------------------------------
NC_TEST(FNCUndoTelemetryTest, "NarrativeContract.Undo.RollbackAndTelemetry")
bool FNCUndoTelemetryTest::RunTest(const FString& Parameters)
{
	FContractModel Model;
	Model.BuildSampleData(false);
	TestFalse(TEXT("fresh queue: nothing to undo"), Model.CanUndo());

	const int32 TelemetryBefore = Model.Telemetry.Num();
	Model.LogEvent(TEXT("episode_selected"), TEXT("E1"));
	TestEqual(TEXT("event recorded"), Model.Telemetry.Num(), TelemetryBefore + 1);

	Model.ResolveEpisode(TEXT("E1"), TEXT("E1_insert"), EProposalAction::Approve, TEXT("test"));
	TestTrue(TEXT("decision pushed a snapshot"), Model.CanUndo());
	TestNotNull(TEXT("repair applied"), Model.FindNode(TEXT("N6b")));

	const int32 VersionBefore = Model.Versions.GraphVersion;
	const int32 LogBefore = Model.DecisionLog.Num();
	Model.UndoLastDecision(TEXT("test"));

	TestNull(TEXT("repair rolled back"), Model.FindNode(TEXT("N6b")));
	TestTrue(TEXT("version moves forward on rollback"), Model.Versions.GraphVersion > VersionBefore);
	TestEqual(TEXT("log grew by the rollback record"), Model.DecisionLog.Num(), LogBefore + 1);
	TestEqual(TEXT("rollback recorded as an action"), Model.DecisionLog.Last().Action, EProposalAction::Rollback);
	TestTrue(TEXT("telemetry survived the restore"), Model.Telemetry.Num() > TelemetryBefore);

	const FAuthorizationEpisode* E1 = Model.FindEpisode(TEXT("E1"));
	TestTrue(TEXT("episode pending again, decidable anew"), E1 && E1->ResolvedAction == EProposalAction::None);
	return true;
}

#undef NC_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
