// Switches between the authoring screen (Slate, mouse-driven) and the
// playable demo (DefaultPawn walk/fly, E to execute, Tab to return).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ContractPlayerController.generated.h"

class ADemoDirector;
class SWidget;

UCLASS()
class NARRATIVECONTRACT_API AContractPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void EnterDemo();
	void ExitDemo();

private:
	bool bInDemo = false;

	UPROPERTY()
	TObjectPtr<ADemoDirector> Director;

	TSharedPtr<SWidget> MainScreen;
	TSharedPtr<SWidget> DemoHud;

	void ToggleMode();
	void OnInteract();
	void OnChooseOption1();
	void OnChooseOption2();
	void ApplyUiInputMode();

	// Figure-mode helpers
	void CreateAuthoringScreen();
	void RebuildAuthoringUI();   // after a theme switch
	void TakeFigureScreenshot(); // HighResShot into Saved/Screenshots
	void ToggleCaptureMode();
};
