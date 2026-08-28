#include "DemoStation.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FLinearColor StateColor(EDemoStationState State)
	{
		switch (State)
		{
		case EDemoStationState::Available:      return FLinearColor(0.25f, 0.55f, 1.0f);
		case EDemoStationState::Executed:       return FLinearColor(0.20f, 0.90f, 0.35f);
		case EDemoStationState::RuntimeBlocked: return FLinearColor(1.0f, 0.15f, 0.10f);
		default:                                return FLinearColor(0.25f, 0.25f, 0.30f);
		}
	}
}

ADemoStation::ADemoStation()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.8f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(42.f);

	Subtitle = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Subtitle"));
	Subtitle->SetupAttachment(Root);
	Subtitle->SetRelativeLocation(FVector(0.f, 0.f, 215.f));
	Subtitle->SetHorizontalAlignment(EHTA_Center);
	Subtitle->SetWorldSize(22.f);
	Subtitle->SetTextRenderColor(FColor(180, 190, 205));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 320.f));
	Light->SetIntensity(2500.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetCastShadows(false);

	Accent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Accent"));
	Accent->SetupAttachment(Root);
	Accent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Accent->SetVisibility(false);
}

void ADemoStation::Setup(const FString& InNodeId, const FString& Title, bool bEnding, bool bPlaceholder)
{
	NodeId = InNodeId;
	FString Text = FString::Printf(TEXT("%s  %s"), *InNodeId, *Title);
	if (bEnding)
	{
		Text += TEXT("  [ENDING]");
	}
	Label->SetText(FText::FromString(Text));

	if (bPlaceholder)
	{
		SetSubtitle(TEXT("[PLACEHOLDER: CrowdSystem - approved, unimplemented]"));
		Subtitle->SetTextRenderColor(FColor(230, 175, 60));
	}

	if (Mesh->GetStaticMesh())
	{
		MeshMID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	SetStationState(EDemoStationState::Locked);
}

void ADemoStation::SetStationState(EDemoStationState NewState)
{
	StationState = NewState;
	const FLinearColor Color = StateColor(NewState);
	Light->SetLightColor(Color);
	Label->SetTextRenderColor(Color.ToFColor(true));
	if (MeshMID)
	{
		// /Engine/BasicShapes/BasicShapeMaterial exposes a "Color" parameter.
		MeshMID->SetVectorParameterValue(TEXT("Color"), Color * 0.6f);
	}
}

void ADemoStation::SetSubtitle(const FString& Text)
{
	Subtitle->SetText(FText::FromString(Text));
}

void ADemoStation::ApplyPropDressing(const FString& Keywords)
{
	const FString K = Keywords.ToLower();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// Named PropMesh: a local called "Mesh" would shadow the member
	// component (C4458, an error under UE's warning settings).
	UStaticMesh* PropMesh = nullptr;
	FVector Scale(1.f, 1.f, 1.f);
	FVector Location(140.f, 0.f, 0.f);
	FRotator Rotation = FRotator::ZeroRotator;
	FLinearColor Tint(0.35f, 0.35f, 0.40f);

	if (K.Contains(TEXT("locker")))
	{
		PropMesh = Cube; Scale = FVector(0.5f, 0.8f, 1.9f); Location.Z = 95.f;
		Tint = FLinearColor(0.30f, 0.35f, 0.45f);
	}
	else if (K.Contains(TEXT("log")) || K.Contains(TEXT("document")) || K.Contains(TEXT("readable")))
	{
		PropMesh = Cube; Scale = FVector(0.7f, 0.5f, 0.9f); Location.Z = 45.f;
		Tint = FLinearColor(0.45f, 0.38f, 0.25f);
	}
	else if (K.Contains(TEXT("valve")) || K.Contains(TEXT("release")) || K.Contains(TEXT("wheel")))
	{
		PropMesh = Cylinder; Scale = FVector(0.9f, 0.9f, 0.16f); Location.Z = 110.f;
		Rotation = FRotator(90.f, 0.f, 0.f);
		Tint = FLinearColor(0.55f, 0.20f, 0.15f);
	}
	else if (K.Contains(TEXT("scan")) || K.Contains(TEXT("residue")) || K.Contains(TEXT("evidence")) || K.Contains(TEXT("decode")))
	{
		PropMesh = Sphere; Scale = FVector(0.55f, 0.55f, 0.55f); Location.Z = 120.f;
		Tint = FLinearColor(0.20f, 0.55f, 0.60f);
	}
	else if (K.Contains(TEXT("reactor")) || K.Contains(TEXT("power")) || K.Contains(TEXT("console")) || K.Contains(TEXT("grid")))
	{
		PropMesh = Cylinder; Scale = FVector(0.7f, 0.7f, 2.4f); Location.Z = 120.f;
		Tint = FLinearColor(0.60f, 0.55f, 0.15f);
	}
	else if (K.Contains(TEXT("airlock")) || K.Contains(TEXT("door")) || K.Contains(TEXT("lab")) || K.Contains(TEXT("sealed")))
	{
		PropMesh = Cube; Scale = FVector(0.25f, 1.6f, 2.3f); Location.Z = 115.f;
		Tint = FLinearColor(0.40f, 0.42f, 0.50f);
	}
	else if (K.Contains(TEXT("dialogue")) || K.Contains(TEXT("confront")) || K.Contains(TEXT("encounter")))
	{
		// Character stations get their dressing from the NPC actor instead.
		return;
	}

	if (!PropMesh)
	{
		return;
	}
	Accent->SetStaticMesh(PropMesh);
	Accent->SetRelativeScale3D(Scale);
	Accent->SetRelativeLocation(Location);
	Accent->SetRelativeRotation(Rotation);
	Accent->SetVisibility(true);
	if (UMaterialInstanceDynamic* MID = Accent->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Tint);
	}
}
