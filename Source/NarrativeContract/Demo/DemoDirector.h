// Builds the playable demo scene from the CURRENT accepted graph and runs a
// clean-state execution: stations unlock along live edges, and every
// interaction re-checks guards and preconditions against runtime facts --
// the runtime-conformance side of the contract (RQ3).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Core/ContractTypes.h"
#include "DemoDirector.generated.h"

class FContractModel;
class ADemoStation;

UCLASS()
class NARRATIVECONTRACT_API ADemoDirector : public AActor
{
	GENERATED_BODY()

public:
	ADemoDirector();

	// (Re)builds the scene from the model's current graph and resets the run.
	void BuildScene(FContractModel* InModel);
	void TearDownScene();

	// Attempts to execute the nearest station within interaction range.
	void Interact(const FVector& PawnLocation);

	// Selects one of the pending dialogue choices (0-based; bound to 1/2).
	// Guards are re-checked against live facts at selection time.
	void ChooseOption(int32 Index);

	FVector GetPlayerStartLocation() const;

	// --- HUD-facing state -------------------------------------------------
	FString ObjectiveText() const;
	FString FactsText() const;
	FString EvidenceText() const;
	FString ChoicePromptText() const;   // empty when no choice is pending
	bool HasPendingChoice() const { return PendingChoiceNodeId.Len() > 0; }
	FString EndingSummaryText() const;  // filled once bEndingReached
	FString LastMessage;
	FString AxisReadout() const;
	float ComputeRunAdherence() const;  // executed path vs approved curves
	bool bEndingReached = false;
	int32 GraphVersionAtBuild = 0;

private:
	FContractModel* Model = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<ADemoStation>> Stations;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SceneProps;

	// Clean-state run
	TSet<FString> Facts;
	TArray<FString> ExecutedNodes;
	TArray<FString> RuntimeEvidence;

	// Choice-selection flow: only unlocked nodes are executable; executing
	// a node with several choices opens a dialogue prompt, and selecting
	// one unlocks exactly that successor (a run is one path, as in the
	// player study's selected sequences).
	TSet<FString> UnlockedNodes;
	FString PendingChoiceNodeId;
	TArray<FStoryChoice> PendingChoices;

	void OfferChoicesOrAdvance(const FStoryNode& Node);

	ADemoStation* FindStation(const FString& NodeId) const;
	bool IsNodePlayable(const FString& NodeId) const;
	void ExecuteStation(ADemoStation& Station);
	void RefreshStationStates();
	void ExportRuntimeTrace() const;
};
