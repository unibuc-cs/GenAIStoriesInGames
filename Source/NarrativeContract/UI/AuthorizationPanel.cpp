#include "AuthorizationPanel.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "UiCommon.h"
#include "../Core/ContractModel.h"

namespace
{
	FString RouteLabel(const FAuthorizationEpisode& Episode)
	{
		switch (Episode.Route)
		{
		case EPolicyRoute::Auto:     return TEXT("Auto");
		case EPolicyRoute::Review:   return TEXT("Review");
		case EPolicyRoute::Applied:  return Episode.ResolvedAction == EProposalAction::Reject ? TEXT("Rejected") : TEXT("Applied");
		case EPolicyRoute::Deferred: return TEXT("Deferred");
		default:                     return TEXT("?");
		}
	}

	FLinearColor RouteColor(const FAuthorizationEpisode& Episode)
	{
		switch (Episode.Route)
		{
		case EPolicyRoute::Auto:     return NCPalette::Valid;
		case EPolicyRoute::Review:   return NCPalette::Warn;
		case EPolicyRoute::Applied:  return Episode.ResolvedAction == EProposalAction::Reject ? NCPalette::Blocked : NCPalette::Valid;
		case EPolicyRoute::Deferred: return NCPalette::TextMuted;
		default:                     return NCPalette::TextMuted;
		}
	}
}

void SAuthorizationPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	SelectedEpisodeId = TEXT("E1");

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
				.Text(FText::FromString(TEXT("AUTHORIZATION EPISODES")))
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
					return FText::FromString(FString::Printf(TEXT("%d pending"),
						Model ? Model->NumPendingEpisodes() : 0));
				})
			]
		]

		// --- Proposal source row (v3): Curated | Live + Generate ----------
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(8.f, 3.f))
				.ButtonColorAndOpacity_Lambda([this]()
				{
					return (Model && Model->ProposalMode == EProposalSourceMode::Curated)
						? NCPalette::Accent : NCPalette::PanelLight;
				})
				.OnClicked_Lambda([this]()
				{
					if (Model)
					{
						Model->ProposalMode = EProposalSourceMode::Curated;
						Model->LiveStatus = TEXT("Curated queue (reproducible fixtures).");
						Model->OnChanged.Broadcast();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Curated")))
					.Font(NCWidgets::Font(9, true))
					.ColorAndOpacity_Lambda([this]()
					{
						return (Model && Model->ProposalMode == EProposalSourceMode::Curated)
							? FLinearColor::Black : NCPalette::TextMuted;
					})
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(8.f, 3.f))
				.ButtonColorAndOpacity_Lambda([this]()
				{
					return (Model && Model->ProposalMode == EProposalSourceMode::Live)
						? NCPalette::Accent : NCPalette::PanelLight;
				})
				.OnClicked_Lambda([this]()
				{
					if (Model)
					{
						Model->ProposalMode = EProposalSourceMode::Live;
						Model->LiveStatus = TEXT("Live mode: select a node, then Generate.");
						Model->OnChanged.Broadcast();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Live LLM")))
					.Font(NCWidgets::Font(9, true))
					.ColorAndOpacity_Lambda([this]()
					{
						return (Model && Model->ProposalMode == EProposalSourceMode::Live)
							? FLinearColor::Black : NCPalette::TextMuted;
					})
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(8.f, 3.f))
				.ButtonColorAndOpacity(NCPalette::Valid)
				.Visibility_Lambda([this]()
				{
					return (Model && Model->ProposalMode == EProposalSourceMode::Live)
						? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([this]()
				{
					if (Model)
					{
						Model->RequestLiveProposal();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(NCWidgets::Font(9, true))
					.ColorAndOpacity(FLinearColor::Black)
					.Text_Lambda([this]()
					{
						return FText::FromString(FString::Printf(TEXT("Generate proposal under %s"),
							Model && !Model->SelectedNodeId.IsEmpty() ? *Model->SelectedNodeId : TEXT("root")));
					})
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(8.f, 3.f))
				.ButtonColorAndOpacity(NCPalette::Reval)
				.Visibility_Lambda([this]()
				{
					return (Model && Model->ProposalMode == EProposalSourceMode::Live)
						? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([this]()
				{
					if (Model)
					{
						Model->RequestFrontierExpansion();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Expand frontier")))
					.Font(NCWidgets::Font(9, true))
					.ColorAndOpacity(FLinearColor::Black)
				]
			]
		]

		// Live-source status line
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Font(NCWidgets::Font(8))
			.ColorAndOpacity(NCPalette::TextMuted)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				return FText::FromString(Model ? Model->LiveStatus : FString());
			})
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			SAssignNew(QueueBox, SVerticalBox)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(NCWidgets::WhiteBrush())
			.BorderBackgroundColor(NCPalette::Panel)
			.Padding(10.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(DetailBox, SVerticalBox)
				]
			]
		]
	];

	if (Model)
	{
		ModelChangedHandle = Model->OnChanged.AddSP(this, &SAuthorizationPanel::Refresh);
	}
	Refresh();
}

SAuthorizationPanel::~SAuthorizationPanel()
{
	if (Model && ModelChangedHandle.IsValid())
	{
		Model->OnChanged.Remove(ModelChangedHandle);
	}
}

void SAuthorizationPanel::Refresh()
{
	RefreshQueue();
	RefreshDetail();
}

void SAuthorizationPanel::RefreshQueue()
{
	if (!QueueBox.IsValid() || !Model)
	{
		return;
	}
	QueueBox->ClearChildren();

	for (const FAuthorizationEpisode& Episode : Model->Episodes)
	{
		const FString EpisodeId = Episode.EpisodeId;
		const bool bSelected = (EpisodeId == SelectedEpisodeId);

		QueueBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 3.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8.f, 5.f))
			.ButtonColorAndOpacity(bSelected ? NCPalette::PanelLight : NCPalette::PanelDark)
			.OnClicked_Lambda([this, EpisodeId]()
			{
				if (Model && SelectedEpisodeId != EpisodeId)
				{
					Model->LogEvent(TEXT("episode_selected"), EpisodeId);
				}
				SelectedEpisodeId = EpisodeId;
				Refresh();
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					NCWidgets::Chip(RouteLabel(Episode), RouteColor(Episode))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					NCWidgets::Chip(Episode.ProposalClass, NCPalette::Accent)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(8.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s  %s"), *Episode.EpisodeId, *Episode.Title)))
					.Font(NCWidgets::Font(9, bSelected))
					.ColorAndOpacity(bSelected ? NCPalette::TextPrimary : NCPalette::TextMuted)
				]
			]
		];
	}
}

void SAuthorizationPanel::RefreshDetail()
{
	if (!DetailBox.IsValid() || !Model)
	{
		return;
	}
	DetailBox->ClearChildren();

	// After a brief switch the selection may be stale; adopt the queue head.
	if (!Model->FindEpisode(SelectedEpisodeId) && Model->Episodes.Num() > 0)
	{
		SelectedEpisodeId = Model->Episodes[0].EpisodeId;
		RefreshQueue();
	}

	FAuthorizationEpisode* Episode = Model->FindEpisode(SelectedEpisodeId);
	if (!Episode)
	{
		DetailBox->AddSlot().AutoHeight()
		[
			NCWidgets::BodyText(TEXT("Select an episode."), NCPalette::TextMuted)
		];
		return;
	}

	auto AddStage = [this](int32 Number, const FString& Title)
	{
		DetailBox->AddSlot().AutoHeight().Padding(0.f, Number == 1 ? 0.f : 12.f, 0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(NCWidgets::WhiteBrush())
				.BorderBackgroundColor(NCPalette::Accent)
				.Padding(FMargin(6.f, 1.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d"), Number)))
					.Font(NCWidgets::Font(9, true))
					.ColorAndOpacity(FLinearColor::Black)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Title.ToUpper()))
				.Font(NCWidgets::Font(9, true))
				.ColorAndOpacity(NCPalette::TextMuted)
			]
		];
	};

	auto AddLines = [this](const FString& Label, const TArray<FString>& Lines, const FLinearColor& Color)
	{
		if (Lines.Num() == 0)
		{
			return;
		}
		DetailBox->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
		[
			NCWidgets::BodyText(Label, NCPalette::TextMuted, 9)
		];
		for (const FString& Line : Lines)
		{
			DetailBox->AddSlot().AutoHeight().Padding(8.f, 1.f, 0.f, 0.f)
			[
				NCWidgets::MonoLine(Line, Color)
			];
		}
	};

	// --- Stage 1: AI proposal ------------------------------------------
	AddStage(1, TEXT("AI proposal"));
	DetailBox->AddSlot().AutoHeight()
	[
		NCWidgets::BodyText(Episode->ProposalText, NCPalette::TextPrimary)
	];
	if (!Episode->Diagnostic.IsEmpty())
	{
		DetailBox->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(NCWidgets::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(NCPalette::Blocked.R, NCPalette::Blocked.G, NCPalette::Blocked.B, 0.12f))
			.Padding(6.f)
			[
				NCWidgets::BodyText(FString::Printf(TEXT("Diagnostic: %s"), *Episode->Diagnostic),
					Episode->bGateValid ? NCPalette::Warn : NCPalette::Blocked, 9)
			]
		];
	}

	// --- Stage 2: Commitment profile -----------------------------------
	AddStage(2, TEXT("Commitment profile"));
	{
		TSharedRef<SWrapBox> Chips = SNew(SWrapBox).UseAllottedSize(true);
		auto AddChip = [&Chips](const FString& Text, const FLinearColor& Color)
		{
			Chips->AddSlot().Padding(0.f, 0.f, 4.f, 4.f)
			[
				NCWidgets::Chip(Text, Color)
			];
		};
		const FCommitmentProfile& P = Episode->Profile;
		AddChip(P.Reversibility == EReversibility::Reversible ? TEXT("Reversible") : TEXT("Irreversible"),
			P.Reversibility == EReversibility::Reversible ? NCPalette::Valid : NCPalette::Blocked);
		AddChip(P.Scope == EDependencyScope::Local ? TEXT("Local scope")
			: P.Scope == EDependencyScope::Branch ? TEXT("Branch scope") : TEXT("Global scope"),
			P.Scope == EDependencyScope::Local ? NCPalette::Valid : NCPalette::Warn);
		if (P.bChangesNarrativeMeaning) { AddChip(TEXT("Narrative meaning"), NCPalette::Warn); }
		if (P.bChangesReachability)     { AddChip(TEXT("Reachability"), NCPalette::Warn); }
		if (P.bChangesPersistentState)  { AddChip(TEXT("Persistent state"), NCPalette::Warn); }
		if (P.bIntroducesNewLabel)      { AddChip(TEXT("New label"), NCPalette::Warn); }
		AddChip(ImplConsequenceDisplayName(P.ImplConsequence),
			P.ImplConsequence == EImplConsequence::None ? NCPalette::TextMuted :
			P.ImplConsequence == EImplConsequence::RegisteredMapping ? NCPalette::Accent : NCPalette::Warn);
		AddChip(Episode->bGateValid ? TEXT("Gate: valid") : TEXT("Gate: failed"),
			Episode->bGateValid ? NCPalette::Valid : NCPalette::Blocked);

		DetailBox->AddSlot().AutoHeight()[Chips];
	}

	// --- Stage 3: Evidence package (collapsible to cut scrolling) --------
	auto AddCollapsibleLines = [this](const FString& Label, const TArray<FString>& Lines,
		const FLinearColor& Color, bool bInitiallyCollapsed)
	{
		if (Lines.Num() == 0)
		{
			return;
		}
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		for (const FString& Line : Lines)
		{
			Body->AddSlot().AutoHeight().Padding(14.f, 1.f, 0.f, 0.f)
			[
				NCWidgets::MonoLine(Line, Color)
			];
		}
		const FString PanelLabel = Label;
		DetailBox->AddSlot().AutoHeight()
		[
			SNew(SCollapsibleSection)
			.Title(FString::Printf(TEXT("%s (%d)"), *Label, Lines.Num()))
			.InitiallyCollapsed(bInitiallyCollapsed)
			.OnToggled(FOnCollapsibleToggled::CreateLambda([this, PanelLabel](bool bNowExpanded)
			{
				if (Model && bNowExpanded)
				{
					// RQ2-style: which evidence views were opened.
					Model->LogEvent(TEXT("evidence_panel_opened"), PanelLabel, SelectedEpisodeId);
				}
			}))
			[
				Body
			]
		];
	};

	AddStage(3, TEXT("Evidence package"));
	AddCollapsibleLines(TEXT("Branch state at the affected location"), Episode->Evidence.BranchState, NCPalette::TextPrimary, true);
	AddCollapsibleLines(TEXT("Affected region (dependency closure)"), Episode->Evidence.AffectedRegion, NCPalette::Reval, false);
	AddCollapsibleLines(TEXT("Implementation mapping"), Episode->Evidence.MappingLines, NCPalette::Accent, true);
	AddCollapsibleLines(TEXT("Provenance"), Episode->Evidence.ProvenanceLines, NCPalette::TextMuted, true);
	AddCollapsibleLines(TEXT("Bounded alternatives"), Episode->Evidence.AlternativeLines, NCPalette::TextPrimary, true);
	AddCollapsibleLines(TEXT("Expected revalidation if accepted"), Episode->Evidence.RevalidationChecks, NCPalette::Reval, true);

	// --- Stage 4: Authorization ----------------------------------------
	AddStage(4, TEXT("Authorization"));
	DetailBox->AddSlot().AutoHeight()
	[
		NCWidgets::BodyText(FString::Printf(TEXT("Routed under %s policy: %s"),
			PolicyDisplayName(Model->Policy), *RouteLabel(*Episode)),
			RouteColor(*Episode), 9)
	];

	const bool bResolved = Episode->ResolvedAction != EProposalAction::None;
	if (!bResolved)
	{
		for (const FEpisodeOption& Option : Episode->Options)
		{
			const FString EpisodeId = Episode->EpisodeId;
			const FString OptionId = Option.OptionId;
			const EProposalAction ActionKind = Option.ActionKind;
			const FLinearColor Col =
				Option.ActionKind == EProposalAction::Reject ? NCPalette::Blocked :
				Option.bRecommended ? NCPalette::Valid : NCPalette::PanelLight;

			DetailBox->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(8.f, 5.f))
				.ButtonColorAndOpacity(Col)
				.OnClicked_Lambda([this, EpisodeId, OptionId, ActionKind]()
				{
					if (Model)
					{
						Model->ResolveEpisode(EpisodeId, OptionId, ActionKind, TEXT("designer"));
					}
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Option.Label + (Option.bRecommended ? TEXT("   (recommended)") : TEXT(""))))
						.Font(NCWidgets::Font(9, true))
						.ColorAndOpacity(Option.ActionKind == EProposalAction::Reject || Option.bRecommended
							? FLinearColor::Black : NCPalette::TextPrimary)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Option.Description))
						.Font(NCWidgets::Font(8))
						.AutoWrapText(true)
						.ColorAndOpacity(Option.ActionKind == EProposalAction::Reject || Option.bRecommended
							? FLinearColor(0.f, 0.f, 0.f, 0.7f) : NCPalette::TextMuted)
					]
				]
			];
		}

		if (Episode->Route != EPolicyRoute::Deferred)
		{
			const FString EpisodeId = Episode->EpisodeId;
			DetailBox->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(8.f, 5.f))
				.ButtonColorAndOpacity(NCPalette::PanelDark)
				.OnClicked_Lambda([this, EpisodeId]()
				{
					if (Model)
					{
						Model->ResolveEpisode(EpisodeId, FString(), EProposalAction::Defer, TEXT("designer"));
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Defer (keep pending, record the deferral)")))
					.Font(NCWidgets::Font(9))
					.ColorAndOpacity(NCPalette::TextMuted)
				]
			];
		}
	}

	// --- Stage 5: Accountable outcome ----------------------------------
	AddStage(5, TEXT("Accountable outcome"));
	if (bResolved)
	{
		TArray<FString> OutcomeLines;
		OutcomeLines.Add(FString::Printf(TEXT("action: %s"), ActionDisplayName(Episode->ResolvedAction)));
		if (!Episode->ResolvedOptionId.IsEmpty())
		{
			OutcomeLines.Add(FString::Printf(TEXT("option: %s"), *Episode->ResolvedOptionId));
		}
		OutcomeLines.Add(FString::Printf(TEXT("actor: %s"), *Episode->ResolvedBy));
		OutcomeLines.Add(FString::Printf(TEXT("graph version after: v%d"), Episode->GraphVersionAfter));
		AddLines(TEXT("Versioned record"), OutcomeLines, NCPalette::Valid);

		if (Model->LastRevalidationBoundary.Num() > 0)
		{
			AddLines(TEXT("Scoped revalidation (last accepted edit)"), Model->LastRevalidationBoundary, NCPalette::Reval);
		}
	}
	else
	{
		DetailBox->AddSlot().AutoHeight()
		[
			NCWidgets::BodyText(TEXT("Awaiting a recorded action. Records outside the affected set will retain their earlier results."),
				NCPalette::TextMuted, 9)
		];
	}
}
