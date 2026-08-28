#include "DecisionLogPanel.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#include "UiCommon.h"
#include "../Core/ContractModel.h"

void SDecisionLogPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("INSPECTABLE HISTORY")))
				.Font(NCWidgets::Font(10, true))
				.ColorAndOpacity(NCPalette::TextPrimary)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				// Live count via lambda: LogEvent deliberately does not
				// broadcast, so this row must not rely on rebuilds.
				SNew(STextBlock)
				.Font(NCWidgets::Font(8))
				.ColorAndOpacity(NCPalette::TextMuted)
				.Text_Lambda([this]()
				{
					if (!Model || Model->Telemetry.Num() == 0)
					{
						return FText::GetEmpty();
					}
					const FTelemetryEvent& Last = Model->Telemetry.Last();
					return FText::FromString(FString::Printf(TEXT("telemetry: %d events (last: %s) -> Saved/SessionTelemetry.json"),
						Model->Telemetry.Num(), *Last.Type));
				})
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(NCWidgets::WhiteBrush())
			.BorderBackgroundColor(NCPalette::Panel)
			.Padding(8.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(ContentBox, SVerticalBox)
				]
			]
		]
	];

	if (Model)
	{
		ModelChangedHandle = Model->OnChanged.AddSP(this, &SDecisionLogPanel::Refresh);
	}
	Refresh();
}

SDecisionLogPanel::~SDecisionLogPanel()
{
	if (Model && ModelChangedHandle.IsValid())
	{
		Model->OnChanged.Remove(ModelChangedHandle);
	}
}

void SDecisionLogPanel::Refresh()
{
	if (!ContentBox.IsValid() || !Model)
	{
		return;
	}
	ContentBox->ClearChildren();

	// --- Decision log (latest first) -----------------------------------
	ContentBox->AddSlot().AutoHeight()
	[
		NCWidgets::SectionHeader(FString::Printf(TEXT("Decisions (%d) -> Saved/DecisionLog.json"), Model->DecisionLog.Num()))
	];
	if (Model->DecisionLog.Num() == 0)
	{
		ContentBox->AddSlot().AutoHeight()
		[
			NCWidgets::BodyText(TEXT("No decisions recorded yet."), NCPalette::TextMuted, 9)
		];
	}
	for (int32 i = Model->DecisionLog.Num() - 1; i >= 0; --i)
	{
		const FDecisionRecord& R = Model->DecisionLog[i];
		const FLinearColor ActionCol =
			R.Action == EProposalAction::Reject ? NCPalette::Blocked :
			R.Action == EProposalAction::AutoApplied ? NCPalette::Accent :
			R.Action == EProposalAction::Defer ? NCPalette::TextMuted : NCPalette::Valid;

		ContentBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SBorder)
			.BorderImage(NCWidgets::WhiteBrush())
			.BorderBackgroundColor(NCPalette::PanelDark)
			.Padding(6.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						NCWidgets::Chip(ActionDisplayName(R.Action), ActionCol)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s  %s"), *R.EpisodeId, *R.EpisodeTitle)))
						.Font(NCWidgets::Font(9, true))
						.ColorAndOpacity(NCPalette::TextPrimary)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					NCWidgets::BodyText(FString::Printf(TEXT("%s · %s policy · v%d -> v%d%s · %s"),
						*R.Actor, PolicyDisplayName(R.PolicyAtDecision),
						R.GraphVersionBefore, R.GraphVersionAfter,
						R.RevalidatedNodes.Num() > 0
							? *FString::Printf(TEXT(" · revalidated {%s}"), *FString::Join(R.RevalidatedNodes, TEXT(", ")))
							: TEXT(""),
						*R.Timestamp), NCPalette::TextMuted, 8)
				]
			]
		];
	}

	// --- Curve edits ----------------------------------------------------
	if (Model->CurveEdits.Num() > 0)
	{
		ContentBox->AddSlot().AutoHeight()
		[
			NCWidgets::SectionHeader(FString::Printf(TEXT("Curve edits (%d)"), Model->CurveEdits.Num()))
		];
		const int32 First = FMath::Max(0, Model->CurveEdits.Num() - 6);
		for (int32 i = Model->CurveEdits.Num() - 1; i >= First; --i)
		{
			const FCurveEditRecord& E = Model->CurveEdits[i];
			ContentBox->AddSlot().AutoHeight().Padding(6.f, 1.f, 0.f, 0.f)
			[
				NCWidgets::MonoLine(FString::Printf(TEXT("%s@%.2f: %.2f -> %.2f (%s)"),
					AxisDisplayName(E.Axis), FTargetCurve::ControlX(E.ControlIndex),
					E.OldValue, E.NewValue, *E.Actor), NCPalette::TextMuted)
			];
		}
	}

	// --- Implementation needs ------------------------------------------
	if (Model->ImplementationNeeds.Num() > 0)
	{
		ContentBox->AddSlot().AutoHeight()
		[
			NCWidgets::SectionHeader(TEXT("Recorded implementation needs"))
		];
		for (const FString& Need : Model->ImplementationNeeds)
		{
			ContentBox->AddSlot().AutoHeight().Padding(6.f, 1.f, 0.f, 0.f)
			[
				NCWidgets::MonoLine(Need, NCPalette::Warn)
			];
		}
	}

	// --- Manifest (collapsible) ----------------------------------------
	{
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		for (const FCapabilityRecord& Rec : Model->Manifest)
		{
			Body->AddSlot().AutoHeight().Padding(6.f, 1.f, 0.f, 0.f)
			[
				NCWidgets::MonoLine(FString::Printf(TEXT("%s -> %s"), *Rec.CapabilityId, *Rec.ImplementationClass),
					Rec.bRegistered ? NCPalette::TextPrimary : NCPalette::Blocked)
			];
			if (Rec.bRegistered)
			{
				Body->AddSlot().AutoHeight().Padding(14.f, 0.f, 0.f, 2.f)
				[
					NCWidgets::BodyText(FString::Printf(TEXT("entities: %s · params: %s · evidence: %s"),
						*Rec.EntityTypes, *Rec.Parameters, *Rec.EvidenceFields), NCPalette::TextMuted, 8)
				];
			}
		}
		ContentBox->AddSlot().AutoHeight()
		[
			SNew(SCollapsibleSection)
			.Title(FString::Printf(TEXT("Engine Capability Manifest (M v%d, %d entries)"),
				Model->Versions.EngineCapabilityManifest, Model->Manifest.Num()))
			.InitiallyCollapsed(true)
			[
				Body
			]
		];
	}
}
