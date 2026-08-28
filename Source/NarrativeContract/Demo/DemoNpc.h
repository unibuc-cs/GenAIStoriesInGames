// A procedural character marker: capsule-ish body, sphere head, name label.
// Built entirely from engine basic shapes -- no assets.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoNpc.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UCLASS()
class NARRATIVECONTRACT_API ADemoNpc : public AActor
{
	GENERATED_BODY()

public:
	ADemoNpc();

	void Setup(const FString& DisplayName, const FLinearColor& Color);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> NameLabel;
};
