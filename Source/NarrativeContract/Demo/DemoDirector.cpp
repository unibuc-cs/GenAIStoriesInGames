#include "DemoDirector.h"

#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "DemoStation.h"
#include "DemoNpc.h"
#include "../Core/ContractModel.h"

namespace
{
	FVector StationLocation(const FStoryNode& Node)
	{
		return FVector(Node.Lane * 520.f, Node.Depth * 560.f, 0.f);
	}

	// Per-genre atmosphere, keyed off the brief's genre label.
	struct FGenrePalette
	{
		FLinearColor KeyLight;
		float KeyIntensity;
		FLinearColor FillLight;
		float FillIntensity;
		FLinearColor Fog;
		float FogDensity;
		FLinearColor Floor;
		FLinearColor Character;
	};

	FGenrePalette PaletteForGenre(const FString& Genre)
	{
		if (Genre.Contains(TEXT("Disaster")))
		{
			// Storm grey-green, flat light, heavy fog.
			return {FLinearColor(0.75f, 0.78f, 0.82f), 2.6f,
			        FLinearColor(0.35f, 0.42f, 0.45f), 1.0f,
			        FLinearColor(0.045f, 0.055f, 0.050f), 0.030f,
			        FLinearColor(0.06f, 0.07f, 0.06f),
			        FLinearColor(0.85f, 0.70f, 0.30f)};
		}
		if (Genre.Contains(TEXT("Science")))
		{
			// Cold blue with a red emergency accent.
			return {FLinearColor(0.65f, 0.72f, 0.95f), 3.0f,
			        FLinearColor(0.90f, 0.28f, 0.22f), 0.8f,
			        FLinearColor(0.020f, 0.030f, 0.060f), 0.020f,
			        FLinearColor(0.035f, 0.04f, 0.06f),
			        FLinearColor(0.30f, 0.75f, 0.85f)};
		}
		// Mystery default: warm key, teal night fill.
		return {FLinearColor(1.0f, 0.92f, 0.80f), 3.6f,
		        FLinearColor(0.40f, 0.60f, 0.75f), 1.2f,
		        FLinearColor(0.025f, 0.045f, 0.050f), 0.015f,
		        FLinearColor(0.05f, 0.06f, 0.08f),
		        FLinearColor(0.90f, 0.60f, 0.25f)};
	}
}

ADemoDirector::ADemoDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADemoDirector::TearDownScene()
{
	for (ADemoStation* Station : Stations)
	{
		if (IsValid(Station))
		{
			Station->Destroy();
		}
	}
	Stations.Empty();
	for (AActor* Prop : SceneProps)
	{
		if (IsValid(Prop))
		{
			Prop->Destroy();
		}
	}
	SceneProps.Empty();
}

void ADemoDirector::BuildScene(FContractModel* InModel)
{
	TearDownScene();
	Model = InModel;
	Facts.Empty();
	ExecutedNodes.Empty();
	RuntimeEvidence.Empty();
	UnlockedNodes.Empty();
	PendingChoiceNodeId.Empty();
	PendingChoices.Empty();
	LastMessage = TEXT("Approach the blue station and press E to execute the root node.");
	bEndingReached = false;

	UWorld* World = GetWorld();
	if (!World || !Model)
	{
		return;
	}
	GraphVersionAtBuild = Model->Versions.GraphVersion;
	const FGenrePalette Palette = PaletteForGenre(Model->GenreLabel);

	// --- Floor ----------------------------------------------------------
	{
		AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(
			FVector(800.f, 1200.f, -55.f), FRotator::ZeroRotator);
		if (Floor)
		{
			Floor->SetMobility(EComponentMobility::Movable);
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (Cube && Floor->GetStaticMeshComponent())
			{
				Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
				Floor->SetActorScale3D(FVector(70.f, 80.f, 1.f));
				if (UMaterialInstanceDynamic* MID = Floor->GetStaticMeshComponent()->CreateAndSetMaterialInstanceDynamic(0))
				{
					MID->SetVectorParameterValue(TEXT("Color"), Palette.Floor);
				}
			}
			SceneProps.Add(Floor);
		}
	}

	// --- Lights + fog (per-genre atmosphere) ----------------------------
	{
		ADirectionalLight* Key = World->SpawnActor<ADirectionalLight>(
			FVector(0.f, 0.f, 800.f), FRotator(-55.f, 40.f, 0.f));
		if (Key)
		{
			if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Key->GetLightComponent()))
			{
				LC->SetMobility(EComponentMobility::Movable);
				LC->SetIntensity(Palette.KeyIntensity);
				LC->SetLightColor(Palette.KeyLight);
			}
			SceneProps.Add(Key);
		}
		ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(
			FVector(0.f, 0.f, 800.f), FRotator(-25.f, 220.f, 0.f));
		if (Fill)
		{
			if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Fill->GetLightComponent()))
			{
				LC->SetMobility(EComponentMobility::Movable);
				LC->SetIntensity(Palette.FillIntensity);
				LC->SetLightColor(Palette.FillLight);
			}
			SceneProps.Add(Fill);
		}
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(
			FVector(0.f, 0.f, -100.f), FRotator::ZeroRotator);
		if (Fog)
		{
			if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
			{
				FC->SetMobility(EComponentMobility::Movable);
				FC->FogDensity = Palette.FogDensity;
				FC->FogHeightFalloff = 0.25f;
				FC->FogInscatteringLuminance = Palette.Fog;
				FC->MarkRenderStateDirty();
			}
			SceneProps.Add(Fog);
		}
	}

	// --- Stations for every live node ----------------------------------
	for (const FStoryNode& Node : Model->Nodes)
	{
		if (Node.Status == ENodeStatus::Removed || Node.Status == ENodeStatus::Proposed)
		{
			continue;
		}
		ADemoStation* Station = World->SpawnActor<ADemoStation>(StationLocation(Node), FRotator::ZeroRotator);
		if (Station)
		{
			Station->Setup(Node.NodeId, Node.Title, Node.bEnding,
				Node.Grounding == EGroundingStatus::ApprovedPlaceholder);
			// Prop silhouette from the node's title + gameplay bindings.
			Station->ApplyPropDressing(Node.Title + TEXT(" ") + FString::Join(Node.GameplayBindings, TEXT(" ")));
			Stations.Add(Station);
		}

		// Character stations get a procedural NPC (or a small crowd).
		const bool bCharacter = Node.RequiredCapabilities.Contains(TEXT("ConfrontDialogue"))
			|| Node.RequiredCapabilities.Contains(TEXT("DetainAction"));
		const bool bCrowd = Node.RequiredCapabilities.Contains(TEXT("CrowdEvac"))
			|| Node.RequiredCapabilities.Contains(TEXT("CrowdSystem"));
		if (bCharacter || bCrowd)
		{
			FString DisplayName = TEXT("Figure");
			for (const FString& Entity : Node.Entities)
			{
				if (Entity != TEXT("player") && !Entity.Contains(TEXT("_")))
				{
					DisplayName = Entity.Left(1).ToUpper() + Entity.Mid(1);
					break;
				}
			}
			const int32 Count = bCrowd ? 3 : 1;
			for (int32 i = 0; i < Count; ++i)
			{
				const FVector Offset(200.f + 90.f * i, -150.f + 70.f * i, 0.f);
				ADemoNpc* Npc = World->SpawnActor<ADemoNpc>(StationLocation(Node) + Offset, FRotator(0.f, 200.f, 0.f));
				if (Npc)
				{
					Npc->Setup(bCrowd ? (i == 1 ? TEXT("Crowd") : FString()) : DisplayName, Palette.Character);
					if (bCrowd)
					{
						Npc->SetActorScale3D(FVector(0.85f));
					}
					SceneProps.Add(Npc);
				}
			}
		}
	}

	RefreshStationStates();
}

FVector ADemoDirector::GetPlayerStartLocation() const
{
	if (Model)
	{
		if (const FStoryNode* Root = Model->FindNode(Model->RootNodeId()))
		{
			return StationLocation(*Root) + FVector(0.f, -700.f, 220.f);
		}
	}
	return FVector(0.f, -700.f, 220.f);
}

ADemoStation* ADemoDirector::FindStation(const FString& NodeId) const
{
	for (ADemoStation* Station : Stations)
	{
		if (IsValid(Station) && Station->NodeId == NodeId)
		{
			return Station;
		}
	}
	return nullptr;
}

bool ADemoDirector::IsNodePlayable(const FString& NodeId) const
{
	if (!Model || ExecutedNodes.Contains(NodeId))
	{
		return false;
	}
	if (NodeId == Model->RootNodeId())
	{
		return ExecutedNodes.Num() == 0;
	}
	// Only successors explicitly unlocked by a selected choice are playable
	// (a run is one path through the graph).
	return UnlockedNodes.Contains(NodeId);
}

void ADemoDirector::Interact(const FVector& PawnLocation)
{
	if (!Model || bEndingReached)
	{
		return;
	}

	ADemoStation* Nearest = nullptr;
	float BestDist = ADemoStation::InteractRadius;
	for (ADemoStation* Station : Stations)
	{
		if (!IsValid(Station))
		{
			continue;
		}
		const float Dist = FVector::Dist2D(Station->GetActorLocation(), PawnLocation);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Nearest = Station;
		}
	}

	if (!Nearest)
	{
		LastMessage = TEXT("No station in range.");
		return;
	}

	ExecuteStation(*Nearest);
	RefreshStationStates();
}

void ADemoDirector::ExecuteStation(ADemoStation& Station)
{
	const FStoryNode* Node = Model->FindNode(Station.NodeId);
	if (!Node)
	{
		return;
	}

	if (ExecutedNodes.Contains(Node->NodeId))
	{
		LastMessage = FString::Printf(TEXT("%s already executed this run."), *Node->NodeId);
		return;
	}

	if (!IsNodePlayable(Node->NodeId))
	{
		LastMessage = FString::Printf(TEXT("%s is not reachable yet -- follow an offered choice."), *Node->NodeId);
		return;
	}

	// Runtime gate: find the incoming choice from an executed node and
	// re-check guard + successor preconditions against live facts
	// (EdgeValid evaluated at runtime rather than design time).
	TSet<FString> Candidate = Facts;
	if (ExecutedNodes.Num() > 0)
	{
		const FStoryChoice* Incoming = nullptr;
		const FStoryNode* FromNode = nullptr;
		for (const FString& DoneId : ExecutedNodes)
		{
			if (const FStoryNode* Done = Model->FindNode(DoneId))
			{
				for (const FStoryChoice& Choice : Done->Choices)
				{
					if (Choice.TargetNodeId == Node->NodeId)
					{
						Incoming = &Choice;
						FromNode = Done;
						break;
					}
				}
			}
			if (Incoming)
			{
				break;
			}
		}
		if (Incoming)
		{
			for (const FString& Pred : Incoming->Guard)
			{
				if (!Candidate.Contains(Pred))
				{
					Station.SetStationState(EDemoStationState::RuntimeBlocked);
					LastMessage = FString::Printf(TEXT("RUNTIME GATE at %s: guard %s not satisfied."),
						*Node->NodeId, *Pred);
					RuntimeEvidence.Add(FString::Printf(TEXT("%s: guard_mismatch(%s)"), *Node->NodeId, *Pred));
					return;
				}
			}
			for (const FString& Del : Incoming->DelEffects) { Candidate.Remove(Del); }
			for (const FString& Add : Incoming->AddEffects) { Candidate.Add(Add); }
		}
	}

	for (const FString& Pred : Node->Preconditions)
	{
		if (!Candidate.Contains(Pred))
		{
			Station.SetStationState(EDemoStationState::RuntimeBlocked);
			LastMessage = FString::Printf(
				TEXT("RUNTIME GATE at %s: precondition %s is false on this run -- the engine enforces the same gate the contract validated."),
				*Node->NodeId, *Pred);
			RuntimeEvidence.Add(FString::Printf(TEXT("%s: precondition_mismatch(%s)"), *Node->NodeId, *Pred));
			Model->LogEvent(TEXT("runtime_blocked"), Node->NodeId, Pred);
			return;
		}
	}

	// Execute: apply effects, record evidence.
	Facts = Candidate;
	for (const FString& Del : Node->DelEffects) { Facts.Remove(Del); }
	for (const FString& Add : Node->AddEffects) { Facts.Add(Add); }
	ExecutedNodes.Add(Node->NodeId);
	Model->LogEvent(TEXT("node_executed"), Node->NodeId,
		FString::Printf(TEXT("run graph v%d"), GraphVersionAtBuild));

	FString EvidenceLine = FString::Printf(TEXT("%s: executed, state_conformant"), *Node->NodeId);
	if (Node->RequiredCapabilities.Num() > 0)
	{
		EvidenceLine += FString::Printf(TEXT(" [%s]"), *FString::Join(Node->RequiredCapabilities, TEXT(", ")));
	}
	RuntimeEvidence.Add(EvidenceLine);

	if (Node->bEnding)
	{
		bEndingReached = true;
		LastMessage = FString::Printf(TEXT("ENDING REACHED -- %s"), *Node->Title);
	}
	else
	{
		LastMessage = FString::Printf(TEXT("%s executed. %s"), *Node->NodeId, *Node->Description);
		OfferChoicesOrAdvance(*Node);
	}
	ExportRuntimeTrace();
}

void ADemoDirector::OfferChoicesOrAdvance(const FStoryNode& Node)
{
	// Collect live choices.
	TArray<FStoryChoice> Live;
	for (const FStoryChoice& Choice : Node.Choices)
	{
		const FStoryNode* Target = Model ? Model->FindNode(Choice.TargetNodeId) : nullptr;
		if (Target && Target->Status != ENodeStatus::Removed && Target->Status != ENodeStatus::Proposed)
		{
			Live.Add(Choice);
		}
	}

	if (Live.Num() == 0)
	{
		return;
	}
	if (Live.Num() == 1)
	{
		// Single continuation: select it implicitly.
		UnlockedNodes.Add(Live[0].TargetNodeId);
		LastMessage += FString::Printf(TEXT("  Continue: %s."), *Live[0].Label);
		return;
	}

	// Several choices: open the dialogue prompt; successors stay locked
	// until one is selected.
	PendingChoiceNodeId = Node.NodeId;
	PendingChoices = Live;
	LastMessage += TEXT("  Choose how to continue (press 1 or 2).");
}

void ADemoDirector::ChooseOption(int32 Index)
{
	if (!Model || !HasPendingChoice() || !PendingChoices.IsValidIndex(Index))
	{
		return;
	}
	const FStoryChoice Choice = PendingChoices[Index];

	// Guards are re-checked against live facts at selection time.
	for (const FString& Pred : Choice.Guard)
	{
		if (!Facts.Contains(Pred))
		{
			LastMessage = FString::Printf(TEXT("RUNTIME GATE on choice '%s': guard %s not satisfied -- pick another option."),
				*Choice.Label, *Pred);
			RuntimeEvidence.Add(FString::Printf(TEXT("%s: choice_guard_mismatch(%s)"), *PendingChoiceNodeId, *Pred));
			return;
		}
	}

	PendingChoiceNodeId.Empty();
	PendingChoices.Empty();
	UnlockedNodes.Add(Choice.TargetNodeId);
	LastMessage = FString::Printf(TEXT("Chose '%s' -- head to %s."), *Choice.Label, *Choice.TargetNodeId);
	RuntimeEvidence.Add(FString::Printf(TEXT("%s: choice_selected(%s)"), *Choice.ChoiceId, *Choice.TargetNodeId));
	Model->LogEvent(TEXT("choice_selected"), Choice.ChoiceId, Choice.TargetNodeId);
	RefreshStationStates();
	ExportRuntimeTrace();
}

void ADemoDirector::RefreshStationStates()
{
	for (ADemoStation* Station : Stations)
	{
		if (!IsValid(Station))
		{
			continue;
		}
		if (ExecutedNodes.Contains(Station->NodeId))
		{
			Station->SetStationState(EDemoStationState::Executed);
		}
		else if (Station->StationState == EDemoStationState::RuntimeBlocked)
		{
			// keep the blocked flag visible until the run unblocks it
			if (IsNodePlayable(Station->NodeId))
			{
				Station->SetStationState(EDemoStationState::Available);
			}
		}
		else
		{
			Station->SetStationState(IsNodePlayable(Station->NodeId)
				? EDemoStationState::Available : EDemoStationState::Locked);
		}
	}
}

// ---------------------------------------------------------------------------
// HUD-facing helpers
// ---------------------------------------------------------------------------

FString ADemoDirector::ObjectiveText() const
{
	if (!Model)
	{
		return FString();
	}
	if (bEndingReached)
	{
		return TEXT("Run complete.");
	}
	if (HasPendingChoice())
	{
		return TEXT("A choice is open -- pick an option below.");
	}
	TArray<FString> Available;
	for (const FStoryNode& Node : Model->Nodes)
	{
		if (Node.Status != ENodeStatus::Removed && Node.Status != ENodeStatus::Proposed
			&& IsNodePlayable(Node.NodeId))
		{
			Available.Add(FString::Printf(TEXT("%s %s"), *Node.NodeId, *Node.Title));
		}
	}
	if (Available.Num() == 0)
	{
		return TEXT("No station is currently unlocked.");
	}
	return FString::Printf(TEXT("Go to: %s"), *FString::Join(Available, TEXT("  ·  ")));
}

FString ADemoDirector::ChoicePromptText() const
{
	if (!HasPendingChoice())
	{
		return FString();
	}
	FString Out;
	for (int32 i = 0; i < PendingChoices.Num(); ++i)
	{
		const FStoryChoice& Choice = PendingChoices[i];
		bool bGuardOk = true;
		for (const FString& Pred : Choice.Guard)
		{
			if (!Facts.Contains(Pred))
			{
				bGuardOk = false;
			}
		}
		Out += FString::Printf(TEXT("[%d]  %s%s\n"), i + 1, *Choice.Label,
			bGuardOk ? TEXT("") : TEXT("   (guard unsatisfied)"));
	}
	Out.TrimEndInline();
	return Out;
}

float ADemoDirector::ComputeRunAdherence() const
{
	if (!Model || ExecutedNodes.Num() < 2)
	{
		return 1.f;
	}
	const int32 L = ExecutedNodes.Num();
	float Total = 0.f;
	int32 Samples = 0;
	for (int32 i = 0; i < L; ++i)
	{
		const FStoryNode* Node = Model->FindNode(ExecutedNodes[i]);
		if (!Node)
		{
			continue;
		}
		const float X = static_cast<float>(i) / static_cast<float>(L - 1);
		for (int32 a = 0; a < NumAxes; ++a)
		{
			Total += FMath::Abs(Node->AxisEstimates[a] - Model->Curves[a].EvalApproved(X));
			Samples++;
		}
	}
	return Samples > 0 ? 1.f - Total / static_cast<float>(Samples) : 1.f;
}

FString ADemoDirector::EndingSummaryText() const
{
	if (!bEndingReached || !Model || ExecutedNodes.Num() == 0)
	{
		return FString();
	}
	const FStoryNode* Ending = Model->FindNode(ExecutedNodes.Last());
	FString Out;
	Out += FString::Printf(TEXT("%s\n\n"), Ending ? *Ending->Title.ToUpper() : TEXT("ENDING"));
	if (Ending)
	{
		Out += FString::Printf(TEXT("%s\n\n"), *Ending->Description);
	}
	Out += FString::Printf(TEXT("Path: %s\n"), *FString::Join(ExecutedNodes, TEXT(" - ")));
	Out += FString::Printf(TEXT("Facts established: %d   ·   Run adherence to approved curves: %.3f\n"),
		Facts.Num(), ComputeRunAdherence());
	Out += FString::Printf(TEXT("Executed on graph v%d   ·   trace: Saved/RuntimeTrace.json\n\n"), GraphVersionAtBuild);
	Out += TEXT("Tab returns to authoring.");
	return Out;
}

FString ADemoDirector::FactsText() const
{
	TArray<FString> Lines = Facts.Array();
	Lines.Sort();
	return Lines.Num() > 0 ? FString::Join(Lines, TEXT("\n")) : TEXT("(empty)");
}

FString ADemoDirector::EvidenceText() const
{
	const int32 First = FMath::Max(0, RuntimeEvidence.Num() - 5);
	TArray<FString> Recent;
	for (int32 i = First; i < RuntimeEvidence.Num(); ++i)
	{
		Recent.Add(RuntimeEvidence[i]);
	}
	return Recent.Num() > 0 ? FString::Join(Recent, TEXT("\n")) : TEXT("(none yet)");
}

FString ADemoDirector::AxisReadout() const
{
	if (!Model || ExecutedNodes.Num() == 0)
	{
		return TEXT("axis trace: (execute a node)");
	}
	const FStoryNode* Last = Model->FindNode(ExecutedNodes.Last());
	if (!Last)
	{
		return FString();
	}
	// Position along the run, against the approved curves.
	const int32 GraphDepthMax = 5;
	const float X = FMath::Clamp(static_cast<float>(Last->Depth) / static_cast<float>(GraphDepthMax), 0.f, 1.f);
	FString Out = FString::Printf(TEXT("axis trace at %s (x=%.2f):"), *Last->NodeId, X);
	for (int32 a = 0; a < NumAxes; ++a)
	{
		Out += FString::Printf(TEXT("\n%-12s r=%.2f  target=%.2f"),
			AxisDisplayName(static_cast<ENarrativeAxis>(a)),
			Last->AxisEstimates[a], Model->Curves[a].EvalApproved(X));
	}
	return Out;
}

void ADemoDirector::ExportRuntimeTrace() const
{
	FString Out = FString::Printf(TEXT("{\n  \"graph_version\": %d,\n  \"executed\": [%s],\n  \"evidence\": [\n"),
		GraphVersionAtBuild,
		*FString::JoinBy(ExecutedNodes, TEXT(","), [](const FString& S) { return FString::Printf(TEXT("\"%s\""), *S); }));
	for (int32 i = 0; i < RuntimeEvidence.Num(); ++i)
	{
		Out += FString::Printf(TEXT("    \"%s\"%s\n"), *RuntimeEvidence[i],
			(i + 1 < RuntimeEvidence.Num()) ? TEXT(",") : TEXT(""));
	}
	Out += TEXT("  ]\n}\n");
	FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("RuntimeTrace.json")));
}
