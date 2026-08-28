// Full JSON persistence of the contract state: graph (nodes + choices),
// target curves, manifest, authorization episodes (including their bounded
// options and mutations), decision log, curve edits, implementation needs,
// artifact versions, and policy. A loaded state is revalidated so blocked
// flags and node statuses are recomputed rather than trusted from disk.

#pragma once

#include "CoreMinimal.h"

class FContractModel;

namespace ContractSerialization
{
	FString SaveToJsonString(const FContractModel& Model);
	bool LoadFromJsonString(FContractModel& Model, const FString& Json, FString& OutError);

	// Saved/ContractState.json, plus a timestamped backup copy on save.
	// Both return a human-readable status line for the UI.
	FString SaveToFile(const FContractModel& Model);
	FString LoadFromFile(FContractModel& Model);
}
