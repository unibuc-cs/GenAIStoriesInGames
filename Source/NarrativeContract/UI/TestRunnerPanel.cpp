#include "TestRunnerPanel.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "UiCommon.h"

void STestRunnerPanel::Construct(const FArguments& InArgs)
{
	OnClose = InArgs._OnClose;
	if (Scenarios::All().Num() > 0)
	{
		SelectedScenarioId = Scenarios::All()[0].Id;
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(NCWidgets::WhiteBrush())
		.BorderBackgroundColor(NCPalette::Panel)
		.Padding(12.f)
		[
			SNew(SVerticalBox)

			// Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("FUNCTIONAL SCENARIOS")))
					.Font(NCWidgets::Font(12, true))
					.ColorAndOpacity(NCPalette::TextPrimary)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.ContentPadding(FMargin(10.f, 4.f))
					.ButtonColorAndOpacity(NCPalette::Valid)
					.OnClicked_Lambda([this]()
					{
						RunAll();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Run all")))
						.Font(NCWidgets::Font(9, true))
						.ColorAndOpacity(FLinearColor::Black)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(10.f, 4.f))
					.ButtonColorAndOpacity(NCPalette::PanelLight)
					.OnClicked_Lambda([this]()
					{
						OnClose.ExecuteIfBound();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Close")))
						.Font(NCWidgets::Font(9, true))
						.ColorAndOpacity(NCPalette::TextMuted)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 8.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Each scenario runs on an isolated contract copy -- your session state and saved files are untouched. Generation uses the deterministic mock generator through the real ranking/validation path.")))
				.Font(NCWidgets::Font(8))
				.AutoWrapText(true)
				.ColorAndOpacity(NCPalette::TextMuted)
			]

			// Body: list | results
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				+ SSplitter::Slot()
				.Value(0.42f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(ListBox, SVerticalBox)
					]
				]

				+ SSplitter::Slot()
				.Value(0.58f)
				[
					SNew(SBox)
					.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
					[
						SNew(SBorder)
						.BorderImage(NCWidgets::WhiteBrush())
						.BorderBackgroundColor(NCPalette::PanelDark)
						.Padding(10.f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(ResultBox, SVerticalBox)
							]
						]
					]
				]
			]
		]
	];

	RefreshList();
	RefreshResults();
}

void STestRunnerPanel::RunOne(const FString& ScenarioId)
{
	for (const FScenario& Scenario : Scenarios::All())
	{
		if (Scenario.Id == ScenarioId)
		{
			Results.Add(ScenarioId, Scenarios::Run(Scenario));
			break;
		}
	}
	SelectedScenarioId = ScenarioId;
	RefreshList();
	RefreshResults();
}

void STestRunnerPanel::RunAll()
{
	for (const FScenario& Scenario : Scenarios::All())
	{
		Results.Add(Scenario.Id, Scenarios::Run(Scenario));
	}
	RefreshList();
	RefreshResults();
}

void STestRunnerPanel::RefreshList()
{
	if (!ListBox.IsValid())
	{
		return;
	}
	ListBox->ClearChildren();

	for (const FScenario& Scenario : Scenarios::All())
	{
		const FString ScenarioId = Scenario.Id;
		const FScenarioResult* Result = Results.Find(ScenarioId);
		const bool bSelected = (ScenarioId == SelectedScenarioId);

		FString StatusText = TEXT("not run");
		FLinearColor StatusColor = NCPalette::TextMuted;
		if (Result && Result->bRan)
		{
			StatusText = Result->bAllPassed
				? FString::Printf(TEXT("PASS %d/%d"), Result->NumPassed(), Result->Steps.Num())
				: FString::Printf(TEXT("FAIL %d/%d"), Result->NumPassed(), Result->Steps.Num());
			StatusColor = Result->bAllPassed ? NCPalette::Valid : NCPalette::Blocked;
		}

		ListBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8.f, 6.f))
			.ButtonColorAndOpacity(bSelected ? NCPalette::PanelLight : NCPalette::PanelDark)
			.OnClicked_Lambda([this, ScenarioId]()
			{
				RunOne(ScenarioId);
				return FReply::Handled();
			})
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
						.Text(FText::FromString(Scenario.Name))
						.Font(NCWidgets::Font(10, true))
						.ColorAndOpacity(NCPalette::TextPrimary)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						NCWidgets::Chip(StatusText, StatusColor)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Scenario.Description))
					.Font(NCWidgets::Font(8))
					.AutoWrapText(true)
					.ColorAndOpacity(NCPalette::TextMuted)
				]
			]
		];
	}
}

void STestRunnerPanel::RefreshResults()
{
	if (!ResultBox.IsValid())
	{
		return;
	}
	ResultBox->ClearChildren();

	const FScenarioResult* Result = Results.Find(SelectedScenarioId);
	if (!Result || !Result->bRan)
	{
		ResultBox->AddSlot().AutoHeight()
		[
			NCWidgets::BodyText(TEXT("Click a scenario to run it. Steps and details appear here."), NCPalette::TextMuted)
		];
		return;
	}

	// Summary banner
	ResultBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
	[
		SNew(SBorder)
		.BorderImage(NCWidgets::WhiteBrush())
		.BorderBackgroundColor(FLinearColor(
			(Result->bAllPassed ? NCPalette::Valid : NCPalette::Blocked).R,
			(Result->bAllPassed ? NCPalette::Valid : NCPalette::Blocked).G,
			(Result->bAllPassed ? NCPalette::Valid : NCPalette::Blocked).B, 0.18f))
		.Padding(8.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Result->bAllPassed
				? FString::Printf(TEXT("PASSED -- all %d steps"), Result->Steps.Num())
				: FString::Printf(TEXT("FAILED at step %d of %d executed"), Result->NumPassed() + 1, Result->Steps.Num())))
			.Font(NCWidgets::Font(10, true))
			.ColorAndOpacity(Result->bAllPassed ? NCPalette::Valid : NCPalette::Blocked)
		]
	];

	for (int32 i = 0; i < Result->Steps.Num(); ++i)
	{
		const FScenarioStepResult& Step = Result->Steps[i];
		ResultBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 5.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					NCWidgets::Chip(Step.bPassed ? TEXT("PASS") : TEXT("FAIL"),
						Step.bPassed ? NCPalette::Valid : NCPalette::Blocked)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(6.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d. %s"), i + 1, *Step.Description)))
					.Font(NCWidgets::Font(9, true))
					.AutoWrapText(true)
					.ColorAndOpacity(NCPalette::TextPrimary)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(30.f, 1.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Step.Detail))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
				.AutoWrapText(true)
				.ColorAndOpacity(Step.bPassed ? NCPalette::TextMuted : NCPalette::Blocked)
				.Visibility(Step.Detail.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			]
		];
	}
}
