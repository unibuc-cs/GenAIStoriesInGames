#include "ContractGameMode.h"

#include "GameFramework/DefaultPawn.h"
#include "ContractPlayerController.h"

AContractGameMode::AContractGameMode()
{
	PlayerControllerClass = AContractPlayerController::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}
