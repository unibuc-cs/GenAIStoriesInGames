#include "DemoNpc.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADemoNpc::ADemoNpc()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.1f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	if (CylinderMesh.Succeeded())
	{
		Body->SetStaticMesh(CylinderMesh.Object);
	}

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.42f));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 135.f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded())
	{
		Head->SetStaticMesh(SphereMesh.Object);
	}

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(Root);
	NameLabel->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetWorldSize(26.f);
}

void ADemoNpc::Setup(const FString& DisplayName, const FLinearColor& Color)
{
	NameLabel->SetText(FText::FromString(DisplayName));
	NameLabel->SetTextRenderColor(Color.ToFColor(true));

	if (Body->GetStaticMesh())
	{
		if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color * 0.5f);
		}
	}
	if (Head->GetStaticMesh())
	{
		if (UMaterialInstanceDynamic* MID = Head->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color * 0.8f);
		}
	}
}
