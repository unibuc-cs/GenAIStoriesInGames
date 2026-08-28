// Shared palette and small widget helpers for the authoring UI.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"

#include "../Core/ContractTypes.h"

namespace NCPalette
{
	// Theme-switchable palette. Defaults to the dark authoring theme; the
	// light "paper figure" theme is applied via ApplyTheme(true). Widgets
	// that paint every frame pick changes up live; constructed widgets are
	// rebuilt by the player controller on a theme switch.
	inline FLinearColor Background   {0.045f, 0.050f, 0.062f, 1.f};
	inline FLinearColor PanelDark    {0.070f, 0.078f, 0.098f, 1.f};
	inline FLinearColor Panel        {0.095f, 0.105f, 0.130f, 1.f};
	inline FLinearColor PanelLight   {0.130f, 0.142f, 0.172f, 1.f};
	inline FLinearColor Outline      {0.220f, 0.240f, 0.290f, 1.f};
	inline FLinearColor TextPrimary  {0.900f, 0.915f, 0.940f, 1.f};
	inline FLinearColor TextMuted    {0.520f, 0.560f, 0.630f, 1.f};
	inline FLinearColor Accent       {0.290f, 0.560f, 0.890f, 1.f}; // blue: contract / selection
	inline FLinearColor Valid        {0.290f, 0.680f, 0.420f, 1.f}; // green: valid / accepted
	inline FLinearColor Warn         {0.900f, 0.680f, 0.250f, 1.f}; // amber: designer control / placeholder
	inline FLinearColor Blocked      {0.870f, 0.320f, 0.300f, 1.f}; // red: blocked / failed
	inline FLinearColor Reval        {0.420f, 0.640f, 0.930f, 1.f}; // dashed revalidation boundary
	inline FLinearColor GridLine     {1.f, 1.f, 1.f, 0.05f};        // plot grid
	inline FLinearColor PriorLine    {1.f, 1.f, 1.f, 0.45f};        // dashed genre prior / dotted proposal
	inline FLinearColor EdgeLine     {0.55f, 0.60f, 0.70f, 0.9f};   // graph edges

	inline bool bLightTheme = false;

	// Capture mode: hides legends, hints, and helper text for clean
	// paper-figure screenshots. Read through Visibility lambdas, so
	// toggling it takes effect immediately with no rebuild.
	inline bool bCaptureMode = false;

	inline void ApplyTheme(bool bLight)
	{
		bLightTheme = bLight;
		if (bLight)
		{
			Background  = FLinearColor(0.965f, 0.970f, 0.980f, 1.f);
			PanelDark   = FLinearColor(0.905f, 0.915f, 0.935f, 1.f);
			Panel       = FLinearColor(1.000f, 1.000f, 1.000f, 1.f);
			PanelLight  = FLinearColor(0.930f, 0.938f, 0.955f, 1.f);
			Outline     = FLinearColor(0.700f, 0.720f, 0.760f, 1.f);
			TextPrimary = FLinearColor(0.100f, 0.115f, 0.145f, 1.f);
			TextMuted   = FLinearColor(0.380f, 0.410f, 0.470f, 1.f);
			Accent      = FLinearColor(0.130f, 0.380f, 0.720f, 1.f);
			Valid       = FLinearColor(0.120f, 0.500f, 0.270f, 1.f);
			Warn        = FLinearColor(0.720f, 0.500f, 0.080f, 1.f);
			Blocked     = FLinearColor(0.760f, 0.180f, 0.160f, 1.f);
			Reval       = FLinearColor(0.230f, 0.450f, 0.780f, 1.f);
			GridLine    = FLinearColor(0.f, 0.f, 0.f, 0.07f);
			PriorLine   = FLinearColor(0.f, 0.f, 0.f, 0.45f);
			EdgeLine    = FLinearColor(0.40f, 0.44f, 0.52f, 1.f);
		}
		else
		{
			Background  = FLinearColor(0.045f, 0.050f, 0.062f, 1.f);
			PanelDark   = FLinearColor(0.070f, 0.078f, 0.098f, 1.f);
			Panel       = FLinearColor(0.095f, 0.105f, 0.130f, 1.f);
			PanelLight  = FLinearColor(0.130f, 0.142f, 0.172f, 1.f);
			Outline     = FLinearColor(0.220f, 0.240f, 0.290f, 1.f);
			TextPrimary = FLinearColor(0.900f, 0.915f, 0.940f, 1.f);
			TextMuted   = FLinearColor(0.520f, 0.560f, 0.630f, 1.f);
			Accent      = FLinearColor(0.290f, 0.560f, 0.890f, 1.f);
			Valid       = FLinearColor(0.290f, 0.680f, 0.420f, 1.f);
			Warn        = FLinearColor(0.900f, 0.680f, 0.250f, 1.f);
			Blocked     = FLinearColor(0.870f, 0.320f, 0.300f, 1.f);
			Reval       = FLinearColor(0.420f, 0.640f, 0.930f, 1.f);
			GridLine    = FLinearColor(1.f, 1.f, 1.f, 0.05f);
			PriorLine   = FLinearColor(1.f, 1.f, 1.f, 0.45f);
			EdgeLine    = FLinearColor(0.55f, 0.60f, 0.70f, 0.9f);
		}
	}

	// EVisibility for hint/legend widgets that should vanish in capture mode.
	inline EVisibility HintVisibility()
	{
		return bCaptureMode ? EVisibility::Collapsed : EVisibility::Visible;
	}

	inline FLinearColor StatusColor(ENodeStatus Status)
	{
		switch (Status)
		{
		case ENodeStatus::Valid:       return Valid;
		case ENodeStatus::NeedsReview: return Blocked;
		case ENodeStatus::Unsupported: return Warn;
		case ENodeStatus::Proposed:    return Accent;
		default:                       return TextMuted;
		}
	}

	inline FLinearColor AxisColor(int32 AxisIndex)
	{
		switch (AxisIndex)
		{
		case 0:  return FLinearColor(0.55f, 0.75f, 0.35f, 1.f); // valence
		case 1:  return FLinearColor(0.87f, 0.42f, 0.35f, 1.f); // tension
		case 2:  return FLinearColor(0.35f, 0.62f, 0.90f, 1.f); // agency
		case 3:  return FLinearColor(0.80f, 0.65f, 0.30f, 1.f); // information
		default: return FLinearColor(0.70f, 0.45f, 0.85f, 1.f); // stakes
		}
	}
}

namespace NCWidgets
{
	inline const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	inline FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	inline TSharedRef<SWidget> Chip(const FString& Label, const FLinearColor& Color)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(Color.R, Color.G, Color.B, 0.16f))
			.Padding(FMargin(7.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(Font(9, true))
				.ColorAndOpacity(Color)
			];
	}

	inline TSharedRef<SWidget> SectionHeader(const FString& Label)
	{
		return SNew(SBox)
			.Padding(FMargin(0.f, 10.f, 0.f, 4.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label.ToUpper()))
				.Font(Font(9, true))
				.ColorAndOpacity(NCPalette::TextMuted)
			];
	}

	inline TSharedRef<SWidget> BodyText(const FString& Label, const FLinearColor& Color = NCPalette::TextPrimary, int32 Size = 10)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Label))
			.Font(Font(Size))
			.ColorAndOpacity(Color)
			.AutoWrapText(true);
	}

	inline TSharedRef<SWidget> MonoLine(const FString& Label, const FLinearColor& Color = NCPalette::TextPrimary)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Label))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.ColorAndOpacity(Color);
	}

	inline TSharedRef<SWidget> PanelBox(const TSharedRef<SWidget>& Content, const FLinearColor& Fill = NCPalette::Panel)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(Fill)
			.Padding(FMargin(10.f))
			[
				Content
			];
	}
}

// Fired when a collapsible section toggles; true = now expanded. Used for
// RQ2-style "evidence panel opened" telemetry.
DECLARE_DELEGATE_OneParam(FOnCollapsibleToggled, bool /*bNowExpanded*/);

// A minimal collapsible section (style-safe: no dependence on editor
// styles). Header shows [+]/[-] and the title; the body toggles on click.
class SCollapsibleSection : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCollapsibleSection)
		: _InitiallyCollapsed(true)
	{}
		SLATE_ARGUMENT(FString, Title)
		SLATE_ARGUMENT(bool, InitiallyCollapsed)
		SLATE_EVENT(FOnCollapsibleToggled, OnToggled)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		bCollapsed = InArgs._InitiallyCollapsed;
		Title = InArgs._Title;
		OnToggled = InArgs._OnToggled;

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 2.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(4.f, 2.f))
				.ButtonColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.f))
				.OnClicked_Lambda([this]()
				{
					bCollapsed = !bCollapsed;
					OnToggled.ExecuteIfBound(!bCollapsed);
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(NCWidgets::Font(9, true))
					.ColorAndOpacity(NCPalette::TextMuted)
					.Text_Lambda([this]()
					{
						return FText::FromString(FString::Printf(TEXT("%s %s"),
							bCollapsed ? TEXT("[+]") : TEXT("[-]"), *Title.ToUpper()));
					})
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Visibility_Lambda([this]()
				{
					return bCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
				})
				[
					InArgs._Content.Widget
				]
			]
		];
	}

private:
	bool bCollapsed = true;
	FString Title;
	FOnCollapsibleToggled OnToggled;
};
