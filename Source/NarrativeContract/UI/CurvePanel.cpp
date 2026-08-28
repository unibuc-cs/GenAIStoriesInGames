#include "CurvePanel.h"

#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "UiCommon.h"
#include "../Core/ContractModel.h"

// ===========================================================================
// SCurveCanvas
// ===========================================================================

void SCurveCanvas::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	AxisIndex = InArgs._AxisIndex;
	bEditable = InArgs._bEditable;
	bCompact = InArgs._bCompact;
	OnClicked = InArgs._OnClicked;
	SetCursor(bEditable ? EMouseCursor::CardinalCross : EMouseCursor::Default);
}

FVector2D SCurveCanvas::ComputeDesiredSize(float) const
{
	return bCompact ? FVector2D(150.0, 84.0) : FVector2D(380.0, 240.0);
}

void SCurveCanvas::GetPlotRect(const FVector2D& LocalSize, FVector2D& OutOrigin, FVector2D& OutSize) const
{
	const float L = bCompact ? 6.f : 30.f;
	const float R = bCompact ? 6.f : 10.f;
	const float T = bCompact ? 6.f : 10.f;
	const float B = bCompact ? 6.f : 20.f;
	OutOrigin = FVector2D(L, T);
	OutSize = FVector2D(FMath::Max(10.0, LocalSize.X - L - R), FMath::Max(10.0, LocalSize.Y - T - B));
}

FVector2D SCurveCanvas::ValueToLocal(const FVector2D& LocalSize, float X01, float V01) const
{
	FVector2D Origin, Size;
	GetPlotRect(LocalSize, Origin, Size);
	return FVector2D(Origin.X + X01 * Size.X, Origin.Y + (1.f - V01) * Size.Y);
}

float SCurveCanvas::LocalYToValue(const FVector2D& LocalSize, float LocalY) const
{
	FVector2D Origin, Size;
	GetPlotRect(LocalSize, Origin, Size);
	return 1.f - FMath::Clamp((LocalY - static_cast<float>(Origin.Y)) / static_cast<float>(Size.Y), 0.f, 1.f);
}

int32 SCurveCanvas::HandleUnderPosition(const FGeometry& MyGeometry, const FVector2D& ScreenPos) const
{
	if (!Model)
	{
		return INDEX_NONE;
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(ScreenPos);
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const int32 Axis = FMath::Clamp(AxisIndex.Get(), 0, NumAxes - 1);
	const FTargetCurve& Curve = Model->Curves[Axis];

	for (int32 i = 0; i < NumCurveControlPoints; ++i)
	{
		const FVector2D P = ValueToLocal(LocalSize, FTargetCurve::ControlX(i), Curve.Approved[i]);
		if (FVector2D::Distance(P, Local) < 12.0)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 SCurveCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FSlateBrush* Brush = NCWidgets::WhiteBrush();

	auto DrawBoxAt = [&](const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(static_cast<float>(Size.X), static_cast<float>(Size.Y)),
				FSlateLayoutTransform(FVector2f(static_cast<float>(Pos.X), static_cast<float>(Pos.Y)))),
			Brush, ESlateDrawEffect::None, Color);
	};

	auto DrawPolyline = [&](const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness, int32 Layer)
	{
		TArray<FVector2f> Pts;
		Pts.Reserve(Points.Num());
		for (const FVector2D& P : Points)
		{
			Pts.Add(FVector2f(static_cast<float>(P.X), static_cast<float>(P.Y)));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(),
			Pts, ESlateDrawEffect::None, Color, true, Thickness);
	};

	if (!Model)
	{
		return LayerId;
	}

	const int32 Axis = FMath::Clamp(AxisIndex.Get(), 0, NumAxes - 1);
	const FTargetCurve& Curve = Model->Curves[Axis];
	const FLinearColor AxisCol = NCPalette::AxisColor(Axis);

	FVector2D Origin, Size;
	GetPlotRect(LocalSize, Origin, Size);

	// Background
	DrawBoxAt(FVector2D::ZeroVector, LocalSize, NCPalette::PanelDark, LayerId);

	// Grid
	const int32 GridLayer = LayerId + 1;
	for (int32 g = 0; g <= 4; ++g)
	{
		const float F = static_cast<float>(g) / 4.f;
		DrawBoxAt(FVector2D(Origin.X, Origin.Y + F * Size.Y), FVector2D(Size.X, 1.0),
			NCPalette::GridLine, GridLayer);
		DrawBoxAt(FVector2D(Origin.X + F * Size.X, Origin.Y), FVector2D(1.0, Size.Y),
			NCPalette::GridLine, GridLayer);
	}

	// Permitted band: prior +- epsilon, drawn as translucent columns.
	const int32 BandLayer = LayerId + 2;
	const float Step = 6.f;
	for (float X = 0.f; X < static_cast<float>(Size.X); X += Step)
	{
		const float X01 = X / static_cast<float>(Size.X);
		const float Mu = Curve.EvalPrior(X01);
		const float Hi = FMath::Clamp(Mu + Curve.Epsilon, 0.f, 1.f);
		const float Lo = FMath::Clamp(Mu - Curve.Epsilon, 0.f, 1.f);
		const FVector2D Top = ValueToLocal(LocalSize, X01, Hi);
		const FVector2D Bottom = ValueToLocal(LocalSize, X01, Lo);
		DrawBoxAt(FVector2D(Origin.X + X, Top.Y),
			FVector2D(FMath::Min(Step, static_cast<float>(Size.X) - X), Bottom.Y - Top.Y),
			FLinearColor(NCPalette::Accent.R, NCPalette::Accent.G, NCPalette::Accent.B, 0.10f), BandLayer);
	}

	// Prior (dashed) and proposal (dotted)
	const int32 CurveLayer = LayerId + 3;
	const int32 Samples = FMath::Max(16, static_cast<int32>(Size.X / 4.f));
	{
		bool bPen = true;
		TArray<FVector2D> Segment;
		for (int32 s = 0; s <= Samples; ++s)
		{
			const float X01 = static_cast<float>(s) / static_cast<float>(Samples);
			const FVector2D P = ValueToLocal(LocalSize, X01, Curve.EvalPrior(X01));
			Segment.Add(P);
			if (Segment.Num() >= 4)
			{
				if (bPen)
				{
					DrawPolyline(Segment, NCPalette::PriorLine, 1.2f, CurveLayer);
				}
				bPen = !bPen;
				const FVector2D Last = Segment.Last();
				Segment.Reset();
				Segment.Add(Last);
			}
		}
		if (bPen && Segment.Num() >= 2)
		{
			DrawPolyline(Segment, NCPalette::PriorLine, 1.2f, CurveLayer);
		}
	}
	for (int32 s = 0; s <= Samples; s += 2)
	{
		const float X01 = static_cast<float>(s) / static_cast<float>(Samples);
		const FVector2D P = ValueToLocal(LocalSize, X01, FTargetCurve::EvalPoints(Curve.Proposal, X01));
		DrawBoxAt(P - FVector2D(1.0, 1.0), FVector2D(2.0, 2.0), NCPalette::PriorLine, CurveLayer);
	}

	// Approved curve
	{
		TArray<FVector2D> Points;
		for (int32 s = 0; s <= Samples; ++s)
		{
			const float X01 = static_cast<float>(s) / static_cast<float>(Samples);
			Points.Add(ValueToLocal(LocalSize, X01, Curve.EvalApproved(X01)));
		}
		DrawPolyline(Points, AxisCol, bCompact ? 1.6f : 2.4f, CurveLayer + 1);
	}

	// Handles
	if (!bCompact)
	{
		const int32 HandleLayer = CurveLayer + 2;
		for (int32 i = 0; i < NumCurveControlPoints; ++i)
		{
			const FVector2D P = ValueToLocal(LocalSize, FTargetCurve::ControlX(i), Curve.Approved[i]);
			const bool bActive = (i == DragHandleIndex) || (i == HoverHandleIndex);
			const bool bOutside = Curve.IsHandleOutsideBand(i);
			const float Half = bActive ? 6.f : 4.5f;
			if (bActive)
			{
				DrawBoxAt(P - FVector2D(Half + 2.f, Half + 2.f), FVector2D(2.f * (Half + 2.f), 2.f * (Half + 2.f)),
					NCPalette::TextPrimary, HandleLayer);
			}
			DrawBoxAt(P - FVector2D(Half, Half), FVector2D(2.f * Half, 2.f * Half),
				bOutside ? NCPalette::Warn : AxisCol, HandleLayer + 1);
		}

		// Axis value labels
		const int32 TextLayer = HandleLayer + 2;
		const FSlateFontInfo SmallFont = NCWidgets::Font(8);
		for (int32 g = 0; g <= 4; ++g)
		{
			const float V = 1.f - static_cast<float>(g) / 4.f;
			FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
				AllottedGeometry.ToPaintGeometry(FVector2f(28.f, 12.f),
					FSlateLayoutTransform(FVector2f(2.f, static_cast<float>(Origin.Y) + (static_cast<float>(g) / 4.f) * static_cast<float>(Size.Y) - 6.f))),
				FString::Printf(TEXT("%.2f"), V), SmallFont, ESlateDrawEffect::None, NCPalette::TextMuted);
		}
		for (int32 i = 0; i < NumCurveControlPoints; ++i)
		{
			const float X01 = FTargetCurve::ControlX(i);
			FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
				AllottedGeometry.ToPaintGeometry(FVector2f(30.f, 12.f),
					FSlateLayoutTransform(FVector2f(static_cast<float>(Origin.X) + X01 * static_cast<float>(Size.X) - 8.f,
						static_cast<float>(Origin.Y + Size.Y) + 5.f))),
				FString::Printf(TEXT("%.2f"), X01), SmallFont, ESlateDrawEffect::None, NCPalette::TextMuted);
		}
	}

	return LayerId + 10;
}

FReply SCurveCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (OnClicked.IsBound())
	{
		OnClicked.Execute();
	}
	if (!bEditable || !Model)
	{
		return FReply::Handled();
	}
	DragHandleIndex = HandleUnderPosition(MyGeometry, MouseEvent.GetScreenSpacePosition());
	if (DragHandleIndex != INDEX_NONE)
	{
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Handled();
}

FReply SCurveCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bEditable || !Model)
	{
		return FReply::Unhandled();
	}
	if (DragHandleIndex != INDEX_NONE && HasMouseCapture())
	{
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const float NewValue = LocalYToValue(MyGeometry.GetLocalSize(), static_cast<float>(Local.Y));
		Model->SetApprovedCurveValue(static_cast<ENarrativeAxis>(AxisIndex.Get()), DragHandleIndex, NewValue, TEXT("designer"));
		return FReply::Handled();
	}
	HoverHandleIndex = HandleUnderPosition(MyGeometry, MouseEvent.GetScreenSpacePosition());
	return FReply::Handled();
}

FReply SCurveCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DragHandleIndex != INDEX_NONE)
	{
		DragHandleIndex = INDEX_NONE;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void SCurveCanvas::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	HoverHandleIndex = INDEX_NONE;
}

// ===========================================================================
// SCurvePanel
// ===========================================================================

void SCurvePanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	// Axis tab row
	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	for (int32 a = 0; a < NumAxes; ++a)
	{
		Tabs->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8.f, 3.f))
			.ButtonColorAndOpacity_Lambda([this, a]()
			{
				return (a == SelectedAxis) ? NCPalette::AxisColor(a) : NCPalette::PanelLight;
			})
			.OnClicked_Lambda([this, a]()
			{
				SelectedAxis = a;
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(AxisDisplayName(static_cast<ENarrativeAxis>(a))))
				.Font(NCWidgets::Font(9, true))
				.ColorAndOpacity_Lambda([this, a]()
				{
					return (a == SelectedAxis) ? FLinearColor::Black : NCPalette::TextMuted;
				})
			]
		];
	}

	// Compact previews of all five approved curves
	TSharedRef<SHorizontalBox> Minis = SNew(SHorizontalBox);
	for (int32 a = 0; a < NumAxes; ++a)
	{
		Minis->AddSlot()
		.FillWidth(1.f)
		.Padding(a == 0 ? 0.f : 4.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(AxisDisplayName(static_cast<ENarrativeAxis>(a))))
				.Font(NCWidgets::Font(8))
				.ColorAndOpacity_Lambda([this, a]()
				{
					return (a == SelectedAxis) ? NCPalette::AxisColor(a) : NCPalette::TextMuted;
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SCurveCanvas)
				.Model(Model)
				.AxisIndex(a)
				.bEditable(false)
				.bCompact(true)
				.OnClicked(FSimpleDelegate::CreateLambda([this, a]()
				{
					SelectedAxis = a;
				}))
			]
		];
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TARGET CURVES")))
				.Font(NCWidgets::Font(10, true))
				.ColorAndOpacity(NCPalette::TextPrimary)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(NCWidgets::Font(9))
				.ColorAndOpacity(NCPalette::TextMuted)
				.Text_Lambda([this]()
				{
					if (!Model)
					{
						return FText::GetEmpty();
					}
					return FText::FromString(FString::Printf(TEXT("prior +-%.2f  ·  %d curve edits logged"),
						Model->Curves[SelectedAxis].Epsilon, Model->CurveEdits.Num()));
				})
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 4.f)
		[
			Tabs
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SCurveCanvas)
			.Model(Model)
			.AxisIndex_Lambda([this]() { return SelectedAxis; })
			.bEditable(true)
			.bCompact(false)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			Minis
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([]() { return NCPalette::HintVisibility(); })
			.Font(NCWidgets::Font(8))
			.ColorAndOpacity(NCPalette::TextMuted)
			.Text(FText::FromString(TEXT("dashed: genre prior  ·  shaded: permitted range  ·  dotted: bounded proposal  ·  solid: approved (drag handles; amber = outside band)")))
			.AutoWrapText(true)
		]
	];
}
