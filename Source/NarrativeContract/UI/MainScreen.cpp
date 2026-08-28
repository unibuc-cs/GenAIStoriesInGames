#include "MainScreen.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "TestRunnerPanel.h"
#include "BriefPickerPanel.h"

#include "UiCommon.h"
#include "../Core/ContractSerialization.h"
#include "CurvePanel.h"
#include "GraphPanel.h"
#include "AuthorizationPanel.h"
#include "DecisionLogPanel.h"
#include "../Core/ContractModel.h"

void SMainScreen::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	OnEnterDemo = InArgs._OnEnterDemo;
	OnRequestRebuild = InArgs._OnRequestRebuild;
	OnScreenshot = InArgs._OnScreenshot;

	// Small utility buttons (theme / capture / screenshot); the cluster
	// hides itself in capture mode so figures stay clean (F10 exits).
	auto UtilityButton = [this](const FString& LabelText, FSimpleDelegate OnPress) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ContentPadding(FMargin(8.f, 4.f))
			.ButtonColorAndOpacity(NCPalette::PanelLight)
			.OnClicked_Lambda([OnPress]()
			{
				OnPress.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(LabelText))
				.Font(NCWidgets::Font(9, true))
				.ColorAndOpacity(NCPalette::TextMuted)
			];
	};

	// Policy selector
	TSharedRef<SHorizontalBox> PolicyRow = SNew(SHorizontalBox);
	const EAuthorizationPolicy Policies[3] = {
		EAuthorizationPolicy::Automatic, EAuthorizationPolicy::Assisted, EAuthorizationPolicy::Strict};
	for (EAuthorizationPolicy P : Policies)
	{
		PolicyRow->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(10.f, 4.f))
			.ButtonColorAndOpacity_Lambda([this, P]()
			{
				return (Model && Model->Policy == P) ? NCPalette::Warn : NCPalette::PanelLight;
			})
			.OnClicked_Lambda([this, P]()
			{
				if (Model)
				{
					Model->SetPolicy(P, TEXT("designer"));
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(PolicyDisplayName(P)))
				.Font(NCWidgets::Font(9, true))
				.ColorAndOpacity_Lambda([this, P]()
				{
					return (Model && Model->Policy == P) ? FLinearColor::Black : NCPalette::TextMuted;
				})
			]
		];
	}

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
		SNew(SBorder)
		.BorderImage(NCWidgets::WhiteBrush())
		.BorderBackgroundColor(NCPalette::Background)
		.Padding(10.f)
		[
			SNew(SVerticalBox)

			// ----------------------------------------------------------- Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(NCWidgets::WhiteBrush())
				.BorderBackgroundColor(NCPalette::Panel)
				.Padding(FMargin(12.f, 8.f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("NARRATIVE CAPABILITY CONTRACT")))
							.Font(NCWidgets::Font(13, true))
							.ColorAndOpacity(NCPalette::TextPrimary)
						]
						+ SVerticalBox::Slot().AutoHeight()
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
								return FText::FromString(FString::Printf(
									TEXT("%s (%s)  ·  active contract C(L%d, D%d, M%d, B%d)  ·  graph v%d"),
									*Model->StoryTitle, *Model->GenreLabel,
									Model->Versions.CoreNarrativeLibrary,
									Model->Versions.DomainNarrativeProfile,
									Model->Versions.EngineCapabilityManifest,
									Model->Versions.StoryBible,
									Model->Versions.GraphVersion));
							})
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SBox)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 12.f, 0.f)
					[
						SNew(SHorizontalBox)
						.Visibility_Lambda([]() { return NCPalette::HintVisibility(); })

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							UtilityButton(NCPalette::bLightTheme ? TEXT("Dark theme") : TEXT("Light theme"),
								FSimpleDelegate::CreateLambda([this]()
								{
									NCPalette::ApplyTheme(!NCPalette::bLightTheme);
									OnRequestRebuild.ExecuteIfBound();
								}))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							UtilityButton(TEXT("Capture mode [F10]"),
								FSimpleDelegate::CreateLambda([]()
								{
									NCPalette::bCaptureMode = !NCPalette::bCaptureMode;
								}))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							UtilityButton(TEXT("Screenshot [F9]"),
								FSimpleDelegate::CreateLambda([this]()
								{
									OnScreenshot.ExecuteIfBound();
								}))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SButton)
							.ContentPadding(FMargin(8.f, 4.f))
							.ButtonColorAndOpacity(NCPalette::Warn)
							.Visibility_Lambda([this]()
							{
								return (Model && Model->CanUndo()) ? EVisibility::Visible : EVisibility::Collapsed;
							})
							.ToolTipText_Lambda([this]()
							{
								return FText::FromString(Model
									? FString::Printf(TEXT("Roll back: %s"), *Model->LastUndoLabel()) : FString());
							})
							.OnClicked_Lambda([this]()
							{
								if (Model)
								{
									Model->UndoLastDecision(TEXT("designer"));
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Font(NCWidgets::Font(9, true))
								.ColorAndOpacity(FLinearColor::Black)
								.Text_Lambda([this]()
								{
									return FText::FromString(FString::Printf(TEXT("Undo (%d)"),
										Model ? Model->UndoStack.Num() : 0));
								})
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							UtilityButton(TEXT("Briefs"),
								FSimpleDelegate::CreateLambda([this]()
								{
									bShowBriefs = !bShowBriefs;
									bShowTests = false;
								}))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							UtilityButton(TEXT("Tests"),
								FSimpleDelegate::CreateLambda([this]()
								{
									bShowTests = !bShowTests;
									bShowBriefs = false;
								}))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 4.f, 0.f)
						[
							UtilityButton(TEXT("Save"),
								FSimpleDelegate::CreateLambda([this]()
								{
									if (Model)
									{
										Model->LiveStatus = ContractSerialization::SaveToFile(*Model);
										Model->LogEvent(TEXT("save"), TEXT("ContractState.json"));
										Model->OnChanged.Broadcast();
									}
								}))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							UtilityButton(TEXT("Load"),
								FSimpleDelegate::CreateLambda([this]()
								{
									if (Model)
									{
										// Loading is destructive, so it is undoable too.
										Model->PushUndoSnapshot(FString(), TEXT("Load from file"));
										// LoadFromFile revalidates and broadcasts on success.
										Model->LiveStatus = ContractSerialization::LoadFromFile(*Model);
										Model->LogEvent(TEXT("load"), TEXT("ContractState.json"));
										Model->OnChanged.Broadcast();
									}
								}))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("AUTHORIZATION POLICY")))
							.Font(NCWidgets::Font(8, true))
							.ColorAndOpacity(NCPalette::TextMuted)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							PolicyRow
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ContentPadding(FMargin(12.f, 8.f))
						.ButtonColorAndOpacity(NCPalette::Accent)
						.OnClicked_Lambda([this]()
						{
							OnEnterDemo.ExecuteIfBound();
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Play accepted graph  [Tab]")))
							.Font(NCWidgets::Font(10, true))
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]
			]

			// ------------------------------------------------------------- Body
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				+ SSplitter::Slot()
				.Value(0.27f)
				[
					SNew(SSplitter)
					.Orientation(Orient_Vertical)
					+ SSplitter::Slot()
					.Value(0.55f)
					[
						SNew(SBorder)
						.BorderImage(NCWidgets::WhiteBrush())
						.BorderBackgroundColor(NCPalette::Panel)
						.Padding(8.f)
						[
							SNew(SCurvePanel).Model(Model)
						]
					]
					+ SSplitter::Slot()
					.Value(0.45f)
					[
						SNew(SBox)
						.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
						[
							SNew(SDecisionLogPanel).Model(Model)
						]
					]
				]

				+ SSplitter::Slot()
				.Value(0.45f)
				[
					SNew(SBox)
					.Padding(FMargin(8.f, 0.f, 8.f, 0.f))
					[
						SNew(SStoryGraphPanel).Model(Model)
					]
				]

				+ SSplitter::Slot()
				.Value(0.28f)
				[
					SNew(SAuthorizationPanel).Model(Model)
				]
			]
		]
		]

		// Scenario runner overlay (Tests button toggles it)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(140.f, 70.f))
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				return bShowTests ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(STestRunnerPanel)
				.OnClose(FSimpleDelegate::CreateLambda([this]()
				{
					bShowTests = false;
				}))
			]
		]

		// Brief picker overlay (Briefs button toggles it)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(640.f)
			.HeightOverride(560.f)
			.Visibility_Lambda([this]()
			{
				return bShowBriefs ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SBriefPickerPanel)
				.Model(Model)
				.OnClose(FSimpleDelegate::CreateLambda([this]()
				{
					bShowBriefs = false;
				}))
			]
		]
	];
}
