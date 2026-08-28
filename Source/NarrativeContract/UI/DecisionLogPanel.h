// Inspectable history: the append-only decision log, curve edits, the
// Engine Capability Manifest, and recorded implementation needs.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FContractModel;
class SVerticalBox;

class SDecisionLogPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDecisionLogPanel)
		: _Model(nullptr)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SDecisionLogPanel() override;

private:
	FContractModel* Model = nullptr;
	TSharedPtr<SVerticalBox> ContentBox;
	FDelegateHandle ModelChangedHandle;

	void Refresh();
};
