#include "ContractPlayerController.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/InputComponent.h"
#include "TimerManager.h"
#include "Widgets/SWidget.h"

#include "ContractGameInstance.h"
#include "DemoDirector.h"
#include "../UI/MainScreen.h"
#include "../UI/DemoHud.h"
#include "../UI/UiCommon.h"
#include "../Core/ContractModel.h"

void AContractPlayerController::BeginPlay()
{
	Super::BeginPlay();
	CreateAuthoringScreen();
	ApplyUiInputMode();
}

void AContractPlayerController::CreateAuthoringScreen()
{
	UContractGameInstance* GI = GetGameInstance<UContractGameInstance>();
	FContractModel* Model = GI ? GI->GetModel() : nullptr;

	if (GEngine && GEngine->GameViewport && Model)
	{
		TSharedRef<SMainScreen> Screen = SNew(SMainScreen)
			.Model(Model)
			.OnEnterDemo(FSimpleDelegate::CreateUObject(this, &AContractPlayerController::EnterDemo))
			.OnRequestRebuild(FSimpleDelegate::CreateUObject(this, &AContractPlayerController::RebuildAuthoringUI))
			.OnScreenshot(FSimpleDelegate::CreateUObject(this, &AContractPlayerController::TakeFigureScreenshot));
		MainScreen = Screen;
		GEngine->GameViewport->AddViewportWidgetContent(Screen, 10);
	}
}

void AContractPlayerController::RebuildAuthoringUI()
{
	// Constructed widgets bake palette colors, so a theme switch recreates
	// the authoring screen. Deferred a tick so the click that triggered it
	// finishes routing through the old widget tree first.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (GEngine && GEngine->GameViewport && MainScreen.IsValid())
			{
				GEngine->GameViewport->RemoveViewportWidgetContent(MainScreen.ToSharedRef());
				MainScreen.Reset();
			}
			CreateAuthoringScreen();
			if (bInDemo && MainScreen.IsValid())
			{
				MainScreen->SetVisibility(EVisibility::Collapsed);
			}
		}));
	}
}

void AContractPlayerController::TakeFigureScreenshot()
{
	// Writes a 2x-resolution capture to Saved/Screenshots/.
	ConsoleCommand(TEXT("HighResShot 2"), true);
}

void AContractPlayerController::ToggleCaptureMode()
{
	NCPalette::bCaptureMode = !NCPalette::bCaptureMode;
}

void AContractPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (MainScreen.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(MainScreen.ToSharedRef());
		}
		if (DemoHud.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(DemoHud.ToSharedRef());
		}
	}
	MainScreen.Reset();
	DemoHud.Reset();
	Super::EndPlay(EndPlayReason);
}

void AContractPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindAction(TEXT("ToggleAuthoringUI"), IE_Pressed, this, &AContractPlayerController::ToggleMode);
		InputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AContractPlayerController::OnInteract);
		InputComponent->BindAction(TEXT("TakeFigureShot"), IE_Pressed, this, &AContractPlayerController::TakeFigureScreenshot);
		InputComponent->BindAction(TEXT("ToggleCaptureMode"), IE_Pressed, this, &AContractPlayerController::ToggleCaptureMode);
		InputComponent->BindAction(TEXT("ChooseOption1"), IE_Pressed, this, &AContractPlayerController::OnChooseOption1);
		InputComponent->BindAction(TEXT("ChooseOption2"), IE_Pressed, this, &AContractPlayerController::OnChooseOption2);
	}
}

void AContractPlayerController::ApplyUiInputMode()
{
	// GameAndUI keeps unhandled keys (Tab) reaching player input while the
	// authoring screen is up.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

void AContractPlayerController::EnterDemo()
{
	if (bInDemo)
	{
		return;
	}
	UContractGameInstance* GI = GetGameInstance<UContractGameInstance>();
	FContractModel* Model = GI ? GI->GetModel() : nullptr;
	UWorld* World = GetWorld();
	if (!Model || !World)
	{
		return;
	}

	if (!Director)
	{
		Director = World->SpawnActor<ADemoDirector>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	if (!Director)
	{
		return;
	}

	// Fresh clean-state execution of the CURRENT accepted graph.
	Director->BuildScene(Model);

	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->TeleportTo(Director->GetPlayerStartLocation(), FRotator(0.f, 90.f, 0.f));
		if (AController* C = MyPawn->GetController())
		{
			C->SetControlRotation(FRotator(-10.f, 90.f, 0.f));
		}
	}

	if (MainScreen.IsValid())
	{
		MainScreen->SetVisibility(EVisibility::Collapsed);
	}

	if (!DemoHud.IsValid() && GEngine && GEngine->GameViewport)
	{
		TSharedRef<SDemoHud> Hud = SNew(SDemoHud).Director(Director);
		DemoHud = Hud;
		GEngine->GameViewport->AddViewportWidgetContent(Hud, 5);
	}
	if (DemoHud.IsValid())
	{
		DemoHud->SetVisibility(EVisibility::HitTestInvisible);
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	bInDemo = true;
	Model->LogEvent(TEXT("demo_entered"), FString::Printf(TEXT("graph_v%d"), Model->Versions.GraphVersion));
}

void AContractPlayerController::ExitDemo()
{
	if (!bInDemo)
	{
		return;
	}
	if (Director)
	{
		Director->TearDownScene();
	}
	if (DemoHud.IsValid())
	{
		DemoHud->SetVisibility(EVisibility::Collapsed);
	}
	if (MainScreen.IsValid())
	{
		MainScreen->SetVisibility(EVisibility::Visible);
	}
	ApplyUiInputMode();
	bInDemo = false;

	UContractGameInstance* GI = GetGameInstance<UContractGameInstance>();
	if (FContractModel* Model = GI ? GI->GetModel() : nullptr)
	{
		Model->LogEvent(TEXT("demo_exited"), FString());
	}
}

void AContractPlayerController::ToggleMode()
{
	if (bInDemo)
	{
		ExitDemo();
	}
	else
	{
		EnterDemo();
	}
}

void AContractPlayerController::OnInteract()
{
	if (bInDemo && Director)
	{
		if (APawn* MyPawn = GetPawn())
		{
			Director->Interact(MyPawn->GetActorLocation());
		}
	}
}

void AContractPlayerController::OnChooseOption1()
{
	if (bInDemo && Director)
	{
		Director->ChooseOption(0);
	}
}

void AContractPlayerController::OnChooseOption2()
{
	if (bInDemo && Director)
	{
		Director->ChooseOption(1);
	}
}
