// The full authoring screen: contract header (artifact versions, policy
// selector), target curves, story graph, authorization episodes, and the
// inspectable history -- Figure 1's division of responsibility as one layout.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FContractModel;

class SMainScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMainScreen)
		: _Model(nullptr)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
		SLATE_EVENT(FSimpleDelegate, OnEnterDemo)
		SLATE_EVENT(FSimpleDelegate, OnRequestRebuild) // theme switched: owner rebuilds the screen
		SLATE_EVENT(FSimpleDelegate, OnScreenshot)     // owner runs HighResShot
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FContractModel* Model = nullptr;
	FSimpleDelegate OnEnterDemo;
	FSimpleDelegate OnRequestRebuild;
	FSimpleDelegate OnScreenshot;
	bool bShowTests = false;
	bool bShowBriefs = false;
};
