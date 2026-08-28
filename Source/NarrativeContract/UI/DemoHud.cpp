#include "DemoHud.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "UiCommon.h"
#include "../Demo/DemoDirector.h"

void SDemoHud::Construct(const FArguments& InArgs)
{
	Director = InArgs._Director;

	const FLinearColor PanelBg(0.03f, 0.035f, 0.05f, 0.78f);

	auto Panel = [&PanelBg](const TSharedRef<SWidget>& Content)
	{
		return SNew(SBorder)
			.BorderImage(NCWidgets::WhiteBrush())
			.BorderBackgroundColor(PanelBg)
			.Padding(10.f)
			[
				Content
			];
	};

	ChildSlot
	[
		SNew(SOverlay)

		// Top: objective + message
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.Padding(16.f)
		[
			Panel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font(NCWidgets::Font(11, true))
					.ColorAndOpacity(NCPalette::Accent)
					.Text_Lambda([this]()
					{
						return FText::FromString(Director.IsValid()
							? Director->ObjectiveText() : FString());
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(NCWidgets::Font(10))
					.AutoWrapText(true)
					.ColorAndOpacity_Lambda([this]()
					{
						return (Director.IsValid() && Director->LastMessage.StartsWith(TEXT("RUNTIME GATE")))
							? NCPalette::Blocked : NCPalette::TextPrimary;
					})
					.Text_Lambda([this]()
					{
						return FText::FromString(Director.IsValid() ? Director->LastMessage : FString());
					})
				]
			)
		]

		// Left: facts
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(16.f)
		[
			SNew(SBox)
			.WidthOverride(330.f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("BRANCH STATE (runtime facts)")))
						.Font(NCWidgets::Font(9, true))
						.ColorAndOpacity(NCPalette::TextMuted)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity(NCPalette::Valid)
						.Text_Lambda([this]()
						{
							return FText::FromString(Director.IsValid() ? Director->FactsText() : FString());
						})
					]
				)
			]
		]

		// Right: runtime evidence + axis trace
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(16.f)
		[
			SNew(SBox)
			.WidthOverride(380.f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("RUNTIME EVIDENCE")))
						.Font(NCWidgets::Font(9, true))
						.ColorAndOpacity(NCPalette::TextMuted)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 8.f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity(NCPalette::TextPrimary)
						.Text_Lambda([this]()
						{
							return FText::FromString(Director.IsValid() ? Director->EvidenceText() : FString());
						})
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity(NCPalette::Accent)
						.Text_Lambda([this]()
						{
							return FText::FromString(Director.IsValid() ? Director->AxisReadout() : FString());
						})
					]
				)
			]
		]

		// Bottom-center: dialogue choice prompt
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.f, 0.f, 0.f, 90.f)
		[
			SNew(SBox)
			.WidthOverride(560.f)
			.Visibility_Lambda([this]()
			{
				return (Director.IsValid() && Director->HasPendingChoice())
					? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("CHOOSE  (press the number)")))
						.Font(NCWidgets::Font(10, true))
						.ColorAndOpacity(NCPalette::Warn)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(NCWidgets::Font(11))
						.ColorAndOpacity(NCPalette::TextPrimary)
						.Text_Lambda([this]()
						{
							return FText::FromString(Director.IsValid() ? Director->ChoicePromptText() : FString());
						})
					]
				)
			]
		]

		// Center: ending summary card
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(640.f)
			.Visibility_Lambda([this]()
			{
				return (Director.IsValid() && Director->bEndingReached)
					? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(SBorder)
				.BorderImage(NCWidgets::WhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.02f, 0.025f, 0.04f, 0.92f))
				.Padding(24.f)
				[
					SNew(STextBlock)
					.Font(NCWidgets::Font(12))
					.AutoWrapText(true)
					.ColorAndOpacity(NCPalette::TextPrimary)
					.Text_Lambda([this]()
					{
						return FText::FromString(Director.IsValid() ? Director->EndingSummaryText() : FString());
					})
				]
			]
		]

		// Bottom: controls hint (hidden in capture mode)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(16.f)
		[
			SNew(SBox)
			.Visibility_Lambda([]() { return NCPalette::HintVisibility(); })
			[
				Panel(
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("WASD + mouse: move  ·  E: execute station  ·  1/2: choose  ·  Tab: back to authoring  ·  F9: screenshot  ·  F10: capture mode")))
					.Font(NCWidgets::Font(9))
					.ColorAndOpacity(NCPalette::TextMuted)
				)
			]
		]
	];
}
