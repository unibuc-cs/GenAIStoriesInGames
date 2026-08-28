// One interactable station representing a story node in the playable demo.
// Entirely procedural: engine basic-shape mesh, text label, colored light.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoStation.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class EDemoStationState : uint8
{
	Locked,     // not yet reachable in this run
	Available,  // an executed node offers a choice leading here
	Executed,   // ran this run; effects applied
	RuntimeBlocked // attempted, but the runtime gate refused it
};

UCLASS()
class NARRATIVECONTRACT_API ADemoStation : public AActor
{
	GENERATED_BODY()

public:
	ADemoStation();

	void Setup(const FString& InNodeId, const FString& Title, bool bEnding, bool bPlaceholder);
	void SetStationState(EDemoStationState NewState);
	void SetSubtitle(const FString& Text);

	// Chooses an accent prop silhouette from binding/title keywords
	// (locker, log, valve, scan, reactor, door...). All basic shapes.
	void ApplyPropDressing(const FString& Keywords);

	FString NodeId;
	EDemoStationState StationState = EDemoStationState::Locked;

	static constexpr float InteractRadius = 320.f;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Subtitle;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Accent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MeshMID;
};
