// Versioned story-graph panel (Figure 4): nodes colored by validation
// status, blocked transitions drawn as red dashed edges with their
// diagnostic, and the most recent scoped-revalidation boundary outlined.
// Selecting a node exposes its contract record (preconditions, effects,
// bindings, mapping, provenance, path adherence).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class FContractModel;
class SVerticalBox;

DECLARE_DELEGATE_OneParam(FOnNodeClicked, const FString& /*NodeId*/);

class SStoryGraphCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SStoryGraphCanvas)
		: _Model(nullptr)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
		SLATE_ATTRIBUTE(FString, SelectedNodeId)
		SLATE_EVENT(FOnNodeClicked, OnNodeClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FContractModel* Model = nullptr;
	TAttribute<FString> SelectedNodeId;
	FOnNodeClicked OnNodeClicked;

	FVector2D NodePosition(const struct FStoryNode& Node) const;
	static const FVector2D NodeSize;
};

class SStoryGraphPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStoryGraphPanel)
		: _Model(nullptr)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SStoryGraphPanel() override;

private:
	FContractModel* Model = nullptr;
	FString SelectedNodeId;
	TSharedPtr<SVerticalBox> DetailsBox;
	FDelegateHandle ModelChangedHandle;

	void RefreshDetails();
};
