// Target-curve authoring panel (Figure 2): a detailed editable view of the
// selected axis with the genre prior, the permitted +-epsilon band, the
// bounded proposal, and designer-adjustable handles; plus compact previews
// of the remaining approved curves.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class FContractModel;

// Paints one axis' curves; optionally editable by dragging control handles.
class SCurveCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCurveCanvas)
		: _Model(nullptr)
		, _AxisIndex(0)
		, _bEditable(false)
		, _bCompact(false)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
		SLATE_ATTRIBUTE(int32, AxisIndex)
		SLATE_ARGUMENT(bool, bEditable)
		SLATE_ARGUMENT(bool, bCompact)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
	FContractModel* Model = nullptr;
	TAttribute<int32> AxisIndex;
	bool bEditable = false;
	bool bCompact = false;
	FSimpleDelegate OnClicked;

	int32 DragHandleIndex = INDEX_NONE;
	int32 HoverHandleIndex = INDEX_NONE;

	// Plot-rect helpers (shared by paint and hit-testing).
	void GetPlotRect(const FVector2D& LocalSize, FVector2D& OutOrigin, FVector2D& OutSize) const;
	FVector2D ValueToLocal(const FVector2D& LocalSize, float X01, float V01) const;
	int32 HandleUnderPosition(const FGeometry& MyGeometry, const FVector2D& ScreenPos) const;
	float LocalYToValue(const FVector2D& LocalSize, float LocalY) const;
};

// The full panel: axis tabs, editable canvas, compact previews, adherence.
class SCurvePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCurvePanel)
		: _Model(nullptr)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FContractModel* Model = nullptr;
	int32 SelectedAxis = 1; // tension, matching Figure 2's detailed view
};
