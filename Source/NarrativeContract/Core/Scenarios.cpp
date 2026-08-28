#include "Scenarios.h"

#include "ContractModel.h"
#include "LlmProposals.h"
#include "ContractSerialization.h"
#include "Briefs.h"

#include <initializer_list>

namespace
{
	// ------------------------------------------------------------------
	// Step helper: runs steps in order, stops at the first failure so
	// later, dependent steps do not cascade noise.
	// ------------------------------------------------------------------
	struct FStepper
	{
		FScenarioResult& Result;
		bool bStopped = false;

		explicit FStepper(FScenarioResult& InResult) : Result(InResult) {}

		void Step(const FString& Description, TFunction<bool(FString&)> Body)
		{
			if (bStopped)
			{
				return;
			}
			FScenarioStepResult StepResult;
			StepResult.Description = Description;
			StepResult.bPassed = Body(StepResult.Detail);
			Result.Steps.Add(StepResult);
			if (!StepResult.bPassed)
			{
				Result.bAllPassed = false;
				bStopped = true;
			}
		}
	};

	// ------------------------------------------------------------------
	// Runtime replay: mirrors the demo director's gate -- guard after the
	// previous node, choice effects, then successor preconditions.
	// ------------------------------------------------------------------
	bool ReplayPath(const FContractModel& Model, const TArray<FString>& Path, TSet<FString>& OutFacts, FString& OutError)
	{
		OutFacts.Empty();
		for (int32 i = 0; i < Path.Num(); ++i)
		{
			const FStoryNode* Node = Model.FindNode(Path[i]);
			if (!Node || Node->Status == ENodeStatus::Removed)
			{
				OutError = FString::Printf(TEXT("%s is not a live node"), *Path[i]);
				return false;
			}
			if (i > 0)
			{
				const FStoryNode* Prev = Model.FindNode(Path[i - 1]);
				const FStoryChoice* Incoming = nullptr;
				if (Prev)
				{
					for (const FStoryChoice& Choice : Prev->Choices)
					{
						if (Choice.TargetNodeId == Node->NodeId)
						{
							Incoming = &Choice;
							break;
						}
					}
				}
				if (!Incoming)
				{
					OutError = FString::Printf(TEXT("no edge %s -> %s"), *Path[i - 1], *Path[i]);
					return false;
				}
				for (const FString& Pred : Incoming->Guard)
				{
					if (!OutFacts.Contains(Pred))
					{
						OutError = FString::Printf(TEXT("guard %s unsatisfied at %s"), *Pred, *Path[i]);
						return false;
					}
				}
				for (const FString& Del : Incoming->DelEffects) { OutFacts.Remove(Del); }
				for (const FString& Add : Incoming->AddEffects) { OutFacts.Add(Add); }
			}
			for (const FString& Pred : Node->Preconditions)
			{
				if (!OutFacts.Contains(Pred))
				{
					OutError = FString::Printf(TEXT("precondition %s false at %s"), *Pred, *Path[i]);
					return false;
				}
			}
			for (const FString& Del : Node->DelEffects) { OutFacts.Remove(Del); }
			for (const FString& Add : Node->AddEffects) { OutFacts.Add(Add); }
		}
		return true;
	}

	// Depth-first search for ANY runtime-playable route to an ending
	// (Facts must already include Node's own effects).
	bool CanReachEnding(const FContractModel& Model, const FStoryNode& Node, const TSet<FString>& Facts, int32 Depth = 0)
	{
		if (Node.bEnding)
		{
			return true;
		}
		if (Depth > 16)
		{
			return false;
		}
		for (const FStoryChoice& Choice : Node.Choices)
		{
			const FStoryNode* Target = Model.FindNode(Choice.TargetNodeId);
			if (!Target || Target->Status == ENodeStatus::Removed)
			{
				continue;
			}
			TSet<FString> Next = Facts;
			bool bLicensed = true;
			for (const FString& Pred : Choice.Guard)
			{
				if (!Next.Contains(Pred)) { bLicensed = false; break; }
			}
			if (!bLicensed)
			{
				continue;
			}
			for (const FString& Del : Choice.DelEffects) { Next.Remove(Del); }
			for (const FString& Add : Choice.AddEffects) { Next.Add(Add); }
			for (const FString& Pred : Target->Preconditions)
			{
				if (!Next.Contains(Pred)) { bLicensed = false; break; }
			}
			if (!bLicensed)
			{
				continue;
			}
			for (const FString& Del : Target->DelEffects) { Next.Remove(Del); }
			for (const FString& Add : Target->AddEffects) { Next.Add(Add); }
			if (CanReachEnding(Model, *Target, Next, Depth + 1))
			{
				return true;
			}
		}
		return false;
	}

	FLlmProposal MockCandidate(const FString& Title, const FString& ChoiceLabel,
		std::initializer_list<float> Axes,
		const TArray<FString>& Preconditions = {}, const TArray<FString>& AddEffects = {},
		const TArray<FString>& Capabilities = {})
	{
		FLlmProposal Proposal;
		Proposal.Title = Title;
		Proposal.Description = FString::Printf(TEXT("Mock candidate '%s' from the deterministic scenario generator."), *Title);
		Proposal.ChoiceLabel = ChoiceLabel;
		Proposal.Preconditions = Preconditions;
		Proposal.AddEffects = AddEffects;
		Proposal.RequiredCapabilities = Capabilities;
		Proposal.Rationale = TEXT("scenario mock");
		int32 i = 0;
		for (float V : Axes)
		{
			if (i < NumAxes)
			{
				Proposal.AxisEstimates[i] = V;
			}
			++i;
		}
		return Proposal;
	}

	// ==================================================================
	// Scenario bodies
	// ==================================================================

	void RunFullLoop(FScenarioResult& Result)
	{
		FContractModel Model;
		FContractModel Reloaded;
		FString SavedJson;
		FString QueuedEpisodeId;
		FStepper S(Result);

		S.Step(TEXT("Build the sample contract (Assisted policy, E2 auto-applies)"), [&](FString& Detail)
		{
			Model.BuildSampleData(true);
			Detail = FString::Printf(TEXT("graph v%d, %d pending episodes"), Model.Versions.GraphVersion, Model.NumPendingEpisodes());
			return Model.NumPendingEpisodes() == 3 && Model.Versions.GraphVersion == 18;
		});

		S.Step(TEXT("Select the frontier (oldest expandable path first)"), [&](FString& Detail)
		{
			const TArray<FString> Frontier = Model.FrontierNodeIds();
			Detail = FString::Printf(TEXT("frontier: %s"), *FString::Join(Frontier, TEXT(", ")));
			return Frontier.Num() > 0 && Frontier[0] == TEXT("N4");
		});

		S.Step(TEXT("Generate 3 candidates (mock generator) and rank by target fit"), [&](FString& Detail)
		{
			TArray<FLlmProposal> Candidates;
			Candidates.Add(MockCandidate(TEXT("Question the foreman"), TEXT("Find the night foreman"),
				{0.45f, 0.55f, 0.50f, 0.45f, 0.50f},
				{TEXT("at(hydro_station)")}, {TEXT("knows(player, foreman_account)")}, {TEXT("ConfrontDialogue")}));
			Candidates.Add(MockCandidate(TEXT("Flood the hall"), TEXT("Vent the reservoir"),
				{0.0f, 1.0f, 0.0f, 0.0f, 1.0f}));
			Candidates.Add(MockCandidate(TEXT("Guards swarm the gate"), TEXT("Push through the guards"),
				{0.40f, 0.50f, 0.50f, 0.50f, 0.50f},
				{}, {}, {TEXT("CrowdSystem")}));

			int32 BestIndex = -1;
			float BestScore = 0.f;
			FAuthorizationEpisode Episode = LlmProposals::RankAndBuildEpisode(Model, TEXT("N4"), Candidates, BestIndex, BestScore);
			QueuedEpisodeId = Episode.EpisodeId;
			const bool bRankedLinePresent = Episode.Evidence.AlternativeLines.Num() >= 3;
			Model.AddEpisode(MoveTemp(Episode));
			Detail = FString::Printf(TEXT("best: '%s' score %.2f; %s"),
				*Candidates[BestIndex].Title, BestScore, *QueuedEpisodeId);
			return Candidates[BestIndex].Title == TEXT("Question the foreman") && bRankedLinePresent;
		});

		S.Step(TEXT("Episode routed for review (new predicate = new-label commitment)"), [&](FString& Detail)
		{
			const FAuthorizationEpisode* Episode = Model.FindEpisode(QueuedEpisodeId);
			if (!Episode)
			{
				Detail = TEXT("episode not found");
				return false;
			}
			Detail = FString::Printf(TEXT("gate %s, route %d"), Episode->bGateValid ? TEXT("valid") : TEXT("failed"),
				static_cast<int32>(Episode->Route));
			return Episode->bGateValid && Episode->Profile.bIntroducesNewLabel
				&& Episode->Route == EPolicyRoute::Review;
		});

		S.Step(TEXT("Authorize the proposal (recorded human action)"), [&](FString& Detail)
		{
			const int32 VersionBefore = Model.Versions.GraphVersion;
			const FAuthorizationEpisode* Episode = Model.FindEpisode(QueuedEpisodeId);
			if (!Episode || Episode->Options.Num() == 0)
			{
				Detail = TEXT("no options");
				return false;
			}
			Model.ResolveEpisode(QueuedEpisodeId, Episode->Options[0].OptionId, EProposalAction::Approve, TEXT("scenario"));
			const FStoryNode* NewNode = Model.FindNode(TEXT("L1"));
			Detail = FString::Printf(TEXT("v%d -> v%d, node L1 %s"), VersionBefore, Model.Versions.GraphVersion,
				NewNode ? TEXT("admitted") : TEXT("missing"));
			return NewNode != nullptr && Model.Versions.GraphVersion == VersionBefore + 1;
		});

		S.Step(TEXT("Scoped revalidation licensed the new edge only"), [&](FString& Detail)
		{
			const FStoryNode* L1 = Model.FindNode(TEXT("L1"));
			bool bEdge = false;
			if (const FStoryNode* N4 = Model.FindNode(TEXT("N4")))
			{
				for (const FStoryChoice& Choice : N4->Choices)
				{
					if (Choice.TargetNodeId == TEXT("L1") && !Choice.bBlocked)
					{
						bEdge = true;
					}
				}
			}
			Detail = FString::Printf(TEXT("boundary: {%s}"), *FString::Join(Model.LastRevalidationBoundary, TEXT(", ")));
			return L1 && L1->Status == ENodeStatus::Valid && bEdge
				&& Model.LastRevalidationBoundary.Contains(TEXT("L1"))
				&& !Model.LastRevalidationBoundary.Contains(TEXT("N5"));
		});

		S.Step(TEXT("Save the grown state to JSON (string round-trip, no files)"), [&](FString& Detail)
		{
			SavedJson = ContractSerialization::SaveToJsonString(Model);
			Detail = FString::Printf(TEXT("%d bytes"), SavedJson.Len());
			return SavedJson.Len() > 1000;
		});

		S.Step(TEXT("Load into a fresh model; derived state recomputed"), [&](FString& Detail)
		{
			FString Error;
			if (!ContractSerialization::LoadFromJsonString(Reloaded, SavedJson, Error))
			{
				Detail = Error;
				return false;
			}
			const FStoryNode* L1 = Reloaded.FindNode(TEXT("L1"));
			bool bStillBlocked = false;
			if (const FStoryNode* N6 = Reloaded.FindNode(TEXT("N6")))
			{
				for (const FStoryChoice& Choice : N6->Choices)
				{
					bStillBlocked |= Choice.bBlocked;
				}
			}
			Detail = FString::Printf(TEXT("%d nodes, graph v%d, unrepaired N6 edge still blocked: %s"),
				Reloaded.Nodes.Num(), Reloaded.Versions.GraphVersion, bStillBlocked ? TEXT("yes") : TEXT("no"));
			return L1 != nullptr && Reloaded.Versions.GraphVersion == Model.Versions.GraphVersion && bStillBlocked;
		});

		S.Step(TEXT("Runtime gate parity: locker branch still refuses the confrontation"), [&](FString& Detail)
		{
			TSet<FString> Facts;
			FString Error;
			const bool bBlockedAtRuntime = !ReplayPath(Reloaded, {TEXT("N1"), TEXT("N4"), TEXT("N6"), TEXT("N7")}, Facts, Error);
			Detail = Error;
			return bBlockedAtRuntime && Error.Contains(TEXT("knows(player, sabotage_signature)"));
		});

		S.Step(TEXT("Play the residue route to the arrest ending (clean-state execution)"), [&](FString& Detail)
		{
			TSet<FString> Facts;
			FString Error;
			if (!ReplayPath(Reloaded, {TEXT("N1"), TEXT("N5"), TEXT("N7"), TEXT("N8")}, Facts, Error))
			{
				Detail = Error;
				return false;
			}
			Detail = FString::Printf(TEXT("ending reached with %d facts, incl. detained(ilya): %s"),
				Facts.Num(), Facts.Contains(TEXT("detained(ilya)")) ? TEXT("yes") : TEXT("no"));
			return Facts.Contains(TEXT("detained(ilya)")) && Facts.Contains(TEXT("confronted(ilya)"));
		});
	}

	void RunRepairLoop(FScenarioResult& Result)
	{
		FContractModel Model;
		FStepper S(Result);

		S.Step(TEXT("Build sample; N6 -> N7 blocked with diagnostic"), [&](FString& Detail)
		{
			Model.BuildSampleData(false);
			if (const FStoryNode* N6 = Model.FindNode(TEXT("N6")))
			{
				for (const FStoryChoice& Choice : N6->Choices)
				{
					if (Choice.bBlocked)
					{
						Detail = Choice.BlockDiagnostic;
						return true;
					}
				}
			}
			Detail = TEXT("edge unexpectedly licensed");
			return false;
		});

		S.Step(TEXT("Approve E1: insert N6b 'Decode the log'"), [&](FString& Detail)
		{
			Model.ResolveEpisode(TEXT("E1"), TEXT("E1_insert"), EProposalAction::Approve, TEXT("scenario"));
			Detail = FString::Printf(TEXT("graph v%d"), Model.Versions.GraphVersion);
			return Model.FindNode(TEXT("N6b")) != nullptr;
		});

		S.Step(TEXT("Revalidation boundary covers N6b-N9 and spares N1, N4, N5"), [&](FString& Detail)
		{
			Detail = FString::Printf(TEXT("{%s}"), *FString::Join(Model.LastRevalidationBoundary, TEXT(", ")));
			return Model.LastRevalidationBoundary.Contains(TEXT("N6b"))
				&& Model.LastRevalidationBoundary.Contains(TEXT("N7"))
				&& Model.LastRevalidationBoundary.Contains(TEXT("N8"))
				&& Model.LastRevalidationBoundary.Contains(TEXT("N9"))
				&& !Model.LastRevalidationBoundary.Contains(TEXT("N1"))
				&& !Model.LastRevalidationBoundary.Contains(TEXT("N5"));
		});

		S.Step(TEXT("Runtime replay through the repaired branch to an ending"), [&](FString& Detail)
		{
			TSet<FString> Facts;
			FString Error;
			if (!ReplayPath(Model, {TEXT("N1"), TEXT("N4"), TEXT("N6"), TEXT("N6b"), TEXT("N7"), TEXT("N8")}, Facts, Error))
			{
				Detail = Error;
				return false;
			}
			Detail = FString::Printf(TEXT("%d facts at the ending"), Facts.Num());
			return Facts.Contains(TEXT("knows(player, sabotage_signature)"));
		});
	}

	void RunPolicySweep(FScenarioResult& Result)
	{
		FContractModel Model;
		FStepper S(Result);

		S.Step(TEXT("Build sample with the full queue pending"), [&](FString& Detail)
		{
			Model.BuildSampleData(false);
			Detail = FString::Printf(TEXT("%d episodes"), Model.Episodes.Num());
			return Model.Episodes.Num() == 4;
		});

		auto RouteOf = [&Model](const TCHAR* Id)
		{
			const FAuthorizationEpisode* Episode = Model.FindEpisode(Id);
			return Episode ? Model.RouteEpisode(*Episode) : EPolicyRoute::Review;
		};

		S.Step(TEXT("Automatic: normalization + gate-valid deletion auto; repair + placeholder review"), [&](FString& Detail)
		{
			Model.Policy = EAuthorizationPolicy::Automatic;
			const bool bOk = RouteOf(TEXT("E2")) == EPolicyRoute::Auto
				&& RouteOf(TEXT("E4")) == EPolicyRoute::Auto
				&& RouteOf(TEXT("E1")) == EPolicyRoute::Review
				&& RouteOf(TEXT("E3")) == EPolicyRoute::Review;
			Detail = TEXT("E2/E4 auto, E1/E3 review");
			return bOk;
		});

		S.Step(TEXT("Assisted: only the registered normalization is automatic"), [&](FString& Detail)
		{
			Model.Policy = EAuthorizationPolicy::Assisted;
			const bool bOk = RouteOf(TEXT("E2")) == EPolicyRoute::Auto
				&& RouteOf(TEXT("E1")) == EPolicyRoute::Review
				&& RouteOf(TEXT("E3")) == EPolicyRoute::Review
				&& RouteOf(TEXT("E4")) == EPolicyRoute::Review;
			Detail = TEXT("E2 auto; E1/E3/E4 review");
			return bOk;
		});

		S.Step(TEXT("Strict: every consequential change pauses"), [&](FString& Detail)
		{
			Model.Policy = EAuthorizationPolicy::Strict;
			const bool bOk = RouteOf(TEXT("E1")) == EPolicyRoute::Review
				&& RouteOf(TEXT("E2")) == EPolicyRoute::Review
				&& RouteOf(TEXT("E3")) == EPolicyRoute::Review
				&& RouteOf(TEXT("E4")) == EPolicyRoute::Review;
			Detail = TEXT("all review");
			return bOk;
		});
	}

	void RunPlaceholderPipeline(FScenarioResult& Result)
	{
		FContractModel Model;
		FStepper S(Result);

		S.Step(TEXT("Build sample; CrowdSystem is not in the manifest"), [&](FString& Detail)
		{
			Model.BuildSampleData(false);
			for (const FCapabilityRecord& Rec : Model.Manifest)
			{
				if (Rec.CapabilityId == TEXT("CrowdSystem"))
				{
					Detail = Rec.ImplementationClass;
					return !Rec.bRegistered;
				}
			}
			return false;
		});

		S.Step(TEXT("Approve E3 as a visible placeholder"), [&](FString& Detail)
		{
			const int32 ManifestBefore = Model.Versions.EngineCapabilityManifest;
			Model.ResolveEpisode(TEXT("E3"), TEXT("E3_placeholder"), EProposalAction::Approve, TEXT("scenario"));
			const FStoryNode* N7 = Model.FindNode(TEXT("N7"));
			Detail = FString::Printf(TEXT("N7 grounding: %s; needs: %d; M v%d -> v%d"),
				N7 ? GroundingDisplayName(N7->Grounding) : TEXT("?"),
				Model.ImplementationNeeds.Num(), ManifestBefore, Model.Versions.EngineCapabilityManifest);
			return N7 && N7->Grounding == EGroundingStatus::ApprovedPlaceholder
				&& Model.ImplementationNeeds.Num() == 1
				&& Model.Versions.EngineCapabilityManifest == ManifestBefore + 1;
		});

		S.Step(TEXT("Placeholder stays a visible production commitment in provenance"), [&](FString& Detail)
		{
			const FStoryNode* N7 = Model.FindNode(TEXT("N7"));
			bool bFound = false;
			if (N7)
			{
				for (const FString& Line : N7->Provenance)
				{
					if (Line.Contains(TEXT("placeholder")))
					{
						bFound = true;
						Detail = Line;
					}
				}
			}
			return bFound;
		});
	}

	void RunDeletionGuard(FScenarioResult& Result)
	{
		FContractModel Model;
		FStepper S(Result);

		S.Step(TEXT("Build sample; N9 reachable via the tracking choice"), [&](FString& Detail)
		{
			Model.BuildSampleData(false);
			TSet<FString> Facts;
			FString Error;
			const bool bReachable = ReplayPath(Model, {TEXT("N1"), TEXT("N5"), TEXT("N7"), TEXT("N9")}, Facts, Error);
			Detail = bReachable ? TEXT("dock ending playable") : Error;
			return bReachable;
		});

		S.Step(TEXT("Assisted policy pauses the irreversible deletion"), [&](FString& Detail)
		{
			Model.Policy = EAuthorizationPolicy::Assisted;
			const FAuthorizationEpisode* E4 = Model.FindEpisode(TEXT("E4"));
			Detail = TEXT("E4 routed to review");
			return E4 && Model.RouteEpisode(*E4) == EPolicyRoute::Review;
		});

		S.Step(TEXT("Approve removal; N9 leaves the playable graph"), [&](FString& Detail)
		{
			Model.ResolveEpisode(TEXT("E4"), TEXT("E4_remove"), EProposalAction::Approve, TEXT("scenario"));
			TSet<FString> Facts;
			FString Error;
			const bool bStillReachable = ReplayPath(Model, {TEXT("N1"), TEXT("N5"), TEXT("N7"), TEXT("N9")}, Facts, Error);
			Detail = bStillReachable ? TEXT("N9 unexpectedly playable") : Error;
			return !bStillReachable;
		});
	}

	void RunUndoRollback(FScenarioResult& Result)
	{
		FContractModel Model;
		FStepper S(Result);

		S.Step(TEXT("Build sample; the dock ending N9 is playable"), [&](FString& Detail)
		{
			Model.BuildSampleData(false);
			TSet<FString> Facts;
			FString Error;
			const bool bPlayable = ReplayPath(Model, {TEXT("N1"), TEXT("N5"), TEXT("N7"), TEXT("N9")}, Facts, Error);
			Detail = bPlayable ? TEXT("reachable") : Error;
			return bPlayable;
		});

		S.Step(TEXT("Switch to Automatic: the gate-valid irreversible deletion auto-applies"), [&](FString& Detail)
		{
			Model.SetPolicy(EAuthorizationPolicy::Automatic, TEXT("scenario"));
			const FAuthorizationEpisode* E4 = Model.FindEpisode(TEXT("E4"));
			const FStoryNode* N9 = Model.FindNode(TEXT("N9"));
			Detail = FString::Printf(TEXT("E4 %s; N9 %s; graph v%d"),
				(E4 && E4->ResolvedAction == EProposalAction::AutoApplied) ? TEXT("auto-applied") : TEXT("still pending"),
				(N9 && N9->Status == ENodeStatus::Removed) ? TEXT("removed") : TEXT("live"),
				Model.Versions.GraphVersion);
			return E4 && E4->ResolvedAction == EProposalAction::AutoApplied
				&& N9 && N9->Status == ENodeStatus::Removed;
		});

		S.Step(TEXT("Undo: rollback restores N9 as a NEW versioned transformation"), [&](FString& Detail)
		{
			const int32 VersionBefore = Model.Versions.GraphVersion;
			const int32 LogBefore = Model.DecisionLog.Num();
			Model.UndoLastDecision(TEXT("scenario"));
			const FStoryNode* N9 = Model.FindNode(TEXT("N9"));
			const FAuthorizationEpisode* E4 = Model.FindEpisode(TEXT("E4"));
			const bool bRollbackRecorded = Model.DecisionLog.Num() == LogBefore + 1
				&& Model.DecisionLog.Last().Action == EProposalAction::Rollback;
			Detail = FString::Printf(TEXT("v%d -> v%d; log %d -> %d entries"),
				VersionBefore, Model.Versions.GraphVersion, LogBefore, Model.DecisionLog.Num());
			return N9 && N9->Status != ENodeStatus::Removed
				&& E4 && E4->ResolvedAction == EProposalAction::None
				&& Model.Versions.GraphVersion > VersionBefore
				&& bRollbackRecorded;
		});

		S.Step(TEXT("History is append-only: the auto-decision is still in the log"), [&](FString& Detail)
		{
			bool bAutoStillLogged = false;
			for (const FDecisionRecord& Record : Model.DecisionLog)
			{
				if (Record.EpisodeId == TEXT("E4") && Record.Action == EProposalAction::AutoApplied)
				{
					bAutoStillLogged = true;
				}
			}
			Detail = FString::Printf(TEXT("%d records retained"), Model.DecisionLog.Num());
			return bAutoStillLogged;
		});

		S.Step(TEXT("The dock ending plays again"), [&](FString& Detail)
		{
			TSet<FString> Facts;
			FString Error;
			const bool bPlayable = ReplayPath(Model, {TEXT("N1"), TEXT("N5"), TEXT("N7"), TEXT("N9")}, Facts, Error);
			Detail = bPlayable ? FString::Printf(TEXT("%d facts at the ending"), Facts.Num()) : Error;
			return bPlayable;
		});
	}

	void RunBriefIntegrity(FScenarioResult& Result)
	{
		FStepper S(Result);

		for (const FBriefDescriptor& Brief : Briefs::BuiltIn())
		{
			// Each built-in brief must build a well-formed, playable world.
			S.Step(FString::Printf(TEXT("Brief '%s' builds and validates"), *Brief.Name), [&](FString& Detail)
			{
				FContractModel Model;
				Brief.Build(Model);

				if (Model.Nodes.Num() == 0)
				{
					Detail = TEXT("no nodes");
					return false;
				}

				// Structural integrity: every choice target exists; primary
				// parents resolve; at least two live endings.
				int32 Endings = 0;
				for (const FStoryNode& Node : Model.Nodes)
				{
					if (Node.Status == ENodeStatus::Removed)
					{
						continue;
					}
					if (Node.bEnding)
					{
						Endings++;
					}
					if (!Node.PrimaryParentId.IsEmpty() && !Model.FindNode(Node.PrimaryParentId))
					{
						Detail = FString::Printf(TEXT("%s: dangling primary parent %s"), *Node.NodeId, *Node.PrimaryParentId);
						return false;
					}
					for (const FStoryChoice& Choice : Node.Choices)
					{
						if (!Model.FindNode(Choice.TargetNodeId))
						{
							Detail = FString::Printf(TEXT("%s: dangling choice target %s"), *Node.NodeId, *Choice.TargetNodeId);
							return false;
						}
					}
				}
				if (Endings < 2)
				{
					Detail = FString::Printf(TEXT("only %d endings"), Endings);
					return false;
				}

				// Every live node reachable from the root.
				const TArray<FString> Reachable = Model.ReachableFrom(Model.RootNodeId());
				if (Reachable.Num() != Model.NumLiveNodes())
				{
					Detail = FString::Printf(TEXT("%d of %d nodes reachable"), Reachable.Num(), Model.NumLiveNodes());
					return false;
				}

				// At least one ending playable through the runtime gate
				// (searching ALL routes, not just primary spines -- a brief
				// may deliberately block its spine pre-repair), and at least
				// one branch-locally blocked edge to authorize.
				bool bSomeEndingPlayable = false;
				if (const FStoryNode* Root = Model.FindNode(Model.RootNodeId()))
				{
					TSet<FString> RootFacts;
					for (const FString& Del : Root->DelEffects) { RootFacts.Remove(Del); }
					for (const FString& Add : Root->AddEffects) { RootFacts.Add(Add); }
					bSomeEndingPlayable = CanReachEnding(Model, *Root, RootFacts);
				}
				bool bHasBlockedEdge = false;
				for (const FStoryNode& Node : Model.Nodes)
				{
					for (const FStoryChoice& Choice : Node.Choices)
					{
						bHasBlockedEdge |= Choice.bBlocked;
					}
				}

				Detail = FString::Printf(TEXT("%d nodes, %d endings, playable spine: %s, blocked edge to authorize: %s"),
					Model.NumLiveNodes(), Endings,
					bSomeEndingPlayable ? TEXT("yes") : TEXT("no"),
					bHasBlockedEdge ? TEXT("yes") : TEXT("no"));
				return bSomeEndingPlayable && bHasBlockedEdge;
			});
		}
	}

	void RunPersistenceRoundTrip(FScenarioResult& Result)
	{
		FContractModel Model;
		FContractModel Reloaded;
		FString SavedJson;
		FStepper S(Result);

		S.Step(TEXT("Build sample and make designer edits"), [&](FString& Detail)
		{
			Model.BuildSampleData(true);
			Model.SetApprovedCurveValue(ENarrativeAxis::Tension, 3, 0.9f, TEXT("scenario"));
			Model.ResolveEpisode(TEXT("E1"), TEXT("E1_insert"), EProposalAction::Approve, TEXT("scenario"));
			Detail = FString::Printf(TEXT("graph v%d, %d decisions"), Model.Versions.GraphVersion, Model.DecisionLog.Num());
			return Model.DecisionLog.Num() >= 2;
		});

		S.Step(TEXT("Serialize to JSON"), [&](FString& Detail)
		{
			SavedJson = ContractSerialization::SaveToJsonString(Model);
			Detail = FString::Printf(TEXT("%d bytes"), SavedJson.Len());
			return SavedJson.Len() > 1000;
		});

		S.Step(TEXT("Reload and verify content and derived state"), [&](FString& Detail)
		{
			FString Error;
			if (!ContractSerialization::LoadFromJsonString(Reloaded, SavedJson, Error))
			{
				Detail = Error;
				return false;
			}
			const FStoryNode* N7 = Reloaded.FindNode(TEXT("N7"));
			Detail = FString::Printf(TEXT("%d nodes, v%d, tension[3]=%.2f"),
				Reloaded.Nodes.Num(), Reloaded.Versions.GraphVersion, Reloaded.Curves[1].Approved[3]);
			return Reloaded.Nodes.Num() == Model.Nodes.Num()
				&& Reloaded.Versions.GraphVersion == Model.Versions.GraphVersion
				&& Reloaded.DecisionLog.Num() == Model.DecisionLog.Num()
				&& FMath::IsNearlyEqual(Reloaded.Curves[1].Approved[3], 0.9f, 0.001f)
				&& N7 && N7->Status == ENodeStatus::Valid;
		});

		S.Step(TEXT("Corrupted input is refused, not half-loaded"), [&](FString& Detail)
		{
			FContractModel Refusing;
			FString Error;
			const bool bRefused = !ContractSerialization::LoadFromJsonString(Refusing, TEXT("{\"broken\": true}"), Error);
			Detail = Error;
			return bRefused && !Error.IsEmpty();
		});
	}
}

// ===========================================================================
// Registry
// ===========================================================================

const TArray<FScenario>& Scenarios::All()
{
	static TArray<FScenario> Registry = []()
	{
		TArray<FScenario> List;
		List.Add({TEXT("full-loop"), TEXT("Full authoring loop (mock generator)"),
			TEXT("Frontier - 3 ranked candidates - authorization - scoped revalidation - save/load - runtime replay to an ending."),
			&RunFullLoop});
		List.Add({TEXT("repair-loop"), TEXT("Figure 4 repair loop"),
			TEXT("Blocked N6-N7 diagnosis, N6b insert, exact revalidation boundary, replay through the repaired branch."),
			&RunRepairLoop});
		List.Add({TEXT("policy-sweep"), TEXT("Policy routing sweep"),
			TEXT("Automatic / Assisted / Strict route the four curated episode classes as the paper specifies."),
			&RunPolicySweep});
		List.Add({TEXT("placeholder"), TEXT("Placeholder pipeline"),
			TEXT("Unregistered CrowdSystem becomes an approved, visible production commitment with a recorded need."),
			&RunPlaceholderPipeline});
		List.Add({TEXT("deletion"), TEXT("Irreversible deletion guard"),
			TEXT("The dock ending is playable, the deletion pauses for review, and after approval it is truly gone."),
			&RunDeletionGuard});
		List.Add({TEXT("undo-rollback"), TEXT("Undo as versioned rollback"),
			TEXT("Automatic policy silently deletes the N9 ending (the observed incident); Undo restores it as a recorded, forward-versioned transformation with history intact."),
			&RunUndoRollback});
		List.Add({TEXT("brief-integrity"), TEXT("Brief integrity"),
			TEXT("Every built-in story brief builds a well-formed world: no dangling edges, all nodes reachable, a playable ending, and a branch-local blocked edge to authorize."),
			&RunBriefIntegrity});
		List.Add({TEXT("persistence"), TEXT("Persistence round-trip"),
			TEXT("Designer edits survive JSON save/load; derived validation is recomputed; corrupt input is refused."),
			&RunPersistenceRoundTrip});
		return List;
	}();
	return Registry;
}

FScenarioResult Scenarios::Run(const FScenario& Scenario)
{
	FScenarioResult Result;
	Result.ScenarioId = Scenario.Id;
	Result.bRan = true;
	if (Scenario.Run)
	{
		Scenario.Run(Result);
	}
	return Result;
}
