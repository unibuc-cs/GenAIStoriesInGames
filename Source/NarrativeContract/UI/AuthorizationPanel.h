// Authorization panel (Figure 3): the episode queue and, for the selected
// episode, the five functional stages -- AI proposal, commitment profile,
// evidence package, authorization actions, accountable outcome.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FContractModel;
class SVerticalBox;

class SAuthorizationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAuthorizationPanel)
		: _Model(nullptr)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAuthorizationPanel() override;

private:
	FContractModel* Model = nullptr;
	FString SelectedEpisodeId;
	TSharedPtr<SVerticalBox> QueueBox;
	TSharedPtr<SVerticalBox> DetailBox;
	FDelegateHandle ModelChangedHandle;

	void Refresh();
	void RefreshQueue();
	void RefreshDetail();
};
