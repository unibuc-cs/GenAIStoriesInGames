// In-app scenario runner: a list of end-to-end scenarios (the full
// authoring loop among them), run on click against an isolated contract
// copy, with per-step pass/fail results and details.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "../Core/Scenarios.h"

class SVerticalBox;

class STestRunnerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STestRunnerPanel) {}
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FSimpleDelegate OnClose;
	TSharedPtr<SVerticalBox> ListBox;
	TSharedPtr<SVerticalBox> ResultBox;

	FString SelectedScenarioId;
	TMap<FString, FScenarioResult> Results;

	void RunOne(const FString& ScenarioId);
	void RunAll();
	void RefreshList();
	void RefreshResults();
};
