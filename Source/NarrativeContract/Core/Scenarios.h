// Runnable end-to-end scenarios ("press a button, watch the loop pass").
// Each scenario executes a sequence of steps against a FRESH, isolated
// contract model -- never the live session state, and never the user's
// Saved/ContractState.json (persistence steps round-trip through strings).
// Generation steps use a deterministic mock generator that flows through
// the SAME ranking/validation code path as the real LLM source
// (LlmProposals::RankAndBuildEpisode), so the loop is reproducible.
//
// The same scenarios run from the in-app Tests panel and from the headless
// automation suite (NarrativeContract.Scenarios.*).

#pragma once

#include "CoreMinimal.h"

struct FScenarioStepResult
{
	FString Description;
	bool bPassed = false;
	FString Detail;
};

struct FScenarioResult
{
	FString ScenarioId;
	bool bAllPassed = true;
	bool bRan = false;
	TArray<FScenarioStepResult> Steps;

	int32 NumPassed() const
	{
		int32 Count = 0;
		for (const FScenarioStepResult& Step : Steps)
		{
			if (Step.bPassed)
			{
				Count++;
			}
		}
		return Count;
	}
};

struct FScenario
{
	FString Id;
	FString Name;
	FString Description;
	TFunction<void(FScenarioResult&)> Run;
};

namespace Scenarios
{
	// The registry, in display order.
	const TArray<FScenario>& All();

	// Runs one scenario (synchronous; scenarios are model-only and fast).
	FScenarioResult Run(const FScenario& Scenario);
}
