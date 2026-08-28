#include "GraphPanel.h"

#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#include "UiCommon.h"
#include "../Core/ContractModel.h"

const FVector2D SStoryGraphCanvas::NodeSize(126.0, 52.0);

// ===========================================================================
// SStoryGraphCanvas
// ===========================================================================

void SStoryGraphCanvas::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	SelectedNodeId = InArgs._SelectedNodeId;
	OnNodeClicked = InArgs._OnNodeClicked;
}

FVector2D SStoryGraphCanvas::NodePosition(const FStoryNode& Node) const
{
	return FVector2D(24.0 + Node.Lane * 150.0, 24.0 + Node.Depth * 96.0);
}

FVector2D SStoryGraphCanvas::ComputeDesiredSize(float) const
{
	double MaxX = 400.0, MaxY = 300.0;
	if (Model)
	{
		for (const FStoryNode& Node : Model->Nodes)
		{
			if (Node.Status == ENodeStatus::Removed)
			{
				continue;
			}
			const FVector2D P = NodePosition(Node);
			MaxX = FMath::Max(MaxX, P.X + NodeSize.X + 40.0);
			MaxY = FMath::Max(MaxY, P.Y + NodeSize.Y + 40.0);
		}
	}
	return FVector2D(MaxX, MaxY);
}

int32 SStoryGraphCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* Brush = NCWidgets::WhiteBrush();

	auto DrawBoxAt = [&](const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(static_cast<float>(Size.X), static_cast<float>(Size.Y)),
				FSlateLayoutTransform(FVector2f(static_cast<float>(Pos.X), static_cast<float>(Pos.Y)))),
			Brush, ESlateDrawEffect::None, Color);
	};

	auto DrawLineSeg = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness, int32 Layer)
	{
		TArray<FVector2f> Pts;
		Pts.Add(FVector2f(static_cast<float>(A.X), static_cast<float>(A.Y)));
		Pts.Add(FVector2f(static_cast<float>(B.X), static_cast<float>(B.Y)));
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(),
			Pts, ESlateDrawEffect::None, Color, true, Thickness);
	};

	auto DrawDashedLine = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness, int32 Layer)
	{
		const double Len = FVector2D::Distance(A, B);
		if (Len < 1.0)
		{
			return;
		}
		const FVector2D Dir = (B - A) / Len;
		const double Dash = 6.0, Gap = 5.0;
		double D = 0.0;
		while (D < Len)
		{
			const double E = FMath::Min(D + Dash, Len);
			DrawLineSeg(A + Dir * D, A + Dir * E, Color, Thickness, Layer);
			D = E + Gap;
		}
	};

	auto DrawDashedRect = [&](const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, int32 Layer)
	{
		DrawDashedLine(Pos, Pos + FVector2D(Size.X, 0), Color, 1.4f, Layer);
		DrawDashedLine(Pos + FVector2D(Size.X, 0), Pos + Size, Color, 1.4f, Layer);
		DrawDashedLine(Pos + Size, Pos + FVector2D(0, Size.Y), Color, 1.4f, Layer);
		DrawDashedLine(Pos + FVector2D(0, Size.Y), Pos, Color, 1.4f, Layer);
	};

	if (!Model)
	{
		return LayerId;
	}

	const FString Selected = SelectedNodeId.Get();
	const int32 EdgeLayer = LayerId + 1;
	const int32 NodeLayer = LayerId + 3;
	const int32 TextLayer = LayerId + 5;
	const FSlateFontInfo TitleFont = NCWidgets::Font(9, true);
	const FSlateFontInfo IdFont = NCWidgets::Font(8);

	// Edges
	for (const FStoryNode& Node : Model->Nodes)
	{
		if (Node.Status == ENodeStatus::Removed || Node.Status == ENodeStatus::Proposed)
		{
			continue;
		}
		const FVector2D From = NodePosition(Node) + FVector2D(NodeSize.X * 0.5, NodeSize.Y);
		for (const FStoryChoice& Choice : Node.Choices)
		{
			const FStoryNode* Target = Model->FindNode(Choice.TargetNodeId);
			if (!Target || Target->Status == ENodeStatus::Removed || Target->Status == ENodeStatus::Proposed)
			{
				continue;
			}
			const FVector2D To = NodePosition(*Target) + FVector2D(NodeSize.X * 0.5, 0.0);
			const FVector2D Mid(From.X, From.Y + (To.Y - From.Y) * 0.5);
			const FVector2D Mid2(To.X, Mid.Y);

			if (Choice.bBlocked)
			{
				DrawDashedLine(From, Mid, NCPalette::Blocked, 1.6f, EdgeLayer);
				DrawDashedLine(Mid, Mid2, NCPalette::Blocked, 1.6f, EdgeLayer);
				DrawDashedLine(Mid2, To, NCPalette::Blocked, 1.6f, EdgeLayer);
				FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
					AllottedGeometry.ToPaintGeometry(FVector2f(70.f, 12.f),
						FSlateLayoutTransform(FVector2f(static_cast<float>(Mid2.X) + 4.f, static_cast<float>(Mid.Y) - 12.f))),
					TEXT("blocked"), IdFont, ESlateDrawEffect::None, NCPalette::Blocked);
			}
			else
			{
				const FLinearColor EdgeCol = NCPalette::EdgeLine;
				DrawLineSeg(From, Mid, EdgeCol, 1.5f, EdgeLayer);
				DrawLineSeg(Mid, Mid2, EdgeCol, 1.5f, EdgeLayer);
				DrawLineSeg(Mid2, To, EdgeCol, 1.5f, EdgeLayer);
				// Arrow head
				DrawLineSeg(To, To + FVector2D(-4.0, -6.0), EdgeCol, 1.5f, EdgeLayer);
				DrawLineSeg(To, To + FVector2D(4.0, -6.0), EdgeCol, 1.5f, EdgeLayer);
			}
		}
	}

	// Nodes
	for (const FStoryNode& Node : Model->Nodes)
	{
		if (Node.Status == ENodeStatus::Removed || Node.Status == ENodeStatus::Proposed)
		{
			continue;
		}
		const FVector2D Pos = NodePosition(Node);
		const FLinearColor StatusCol = NCPalette::StatusColor(Node.Status);
		const bool bSelected = (Node.NodeId == Selected);

		// Outline, fill, status stripe
		DrawBoxAt(Pos - FVector2D(1.5, 1.5), NodeSize + FVector2D(3.0, 3.0),
			bSelected ? NCPalette::Accent : NCPalette::Outline, NodeLayer);
		DrawBoxAt(Pos, NodeSize, NCPalette::PanelLight, NodeLayer + 1);
		DrawBoxAt(Pos, FVector2D(4.0, NodeSize.Y), StatusCol, NodeLayer + 1);

		if (Node.bEnding)
		{
			DrawBoxAt(Pos + FVector2D(0.0, NodeSize.Y - 3.0), FVector2D(NodeSize.X, 3.0), NCPalette::Accent, NodeLayer + 2);
		}
		if (Node.Grounding == EGroundingStatus::ApprovedPlaceholder)
		{
			DrawBoxAt(Pos + FVector2D(NodeSize.X - 14.0, 4.0), FVector2D(10.0, 10.0), NCPalette::Warn, NodeLayer + 2);
		}

		// Revalidation boundary from the last accepted edit
		if (Model->LastRevalidationBoundary.Contains(Node.NodeId))
		{
			DrawDashedRect(Pos - FVector2D(5.0, 5.0), NodeSize + FVector2D(10.0, 10.0), NCPalette::Reval, NodeLayer + 2);
		}

		FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(static_cast<float>(NodeSize.X) - 14.f, 12.f),
				FSlateLayoutTransform(FVector2f(static_cast<float>(Pos.X) + 10.f, static_cast<float>(Pos.Y) + 8.f))),
			Node.NodeId, IdFont, ESlateDrawEffect::None, NCPalette::TextMuted);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(static_cast<float>(NodeSize.X) - 14.f, 14.f),
				FSlateLayoutTransform(FVector2f(static_cast<float>(Pos.X) + 10.f, static_cast<float>(Pos.Y) + 22.f))),
			Node.Title, TitleFont, ESlateDrawEffect::None, NCPalette::TextPrimary);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(static_cast<float>(NodeSize.X) - 14.f, 12.f),
				FSlateLayoutTransform(FVector2f(static_cast<float>(Pos.X) + 10.f, static_cast<float>(Pos.Y) + 37.f))),
			NodeStatusDisplayName(Node.Status), IdFont, ESlateDrawEffect::None, StatusCol);
	}

	return LayerId + 8;
}

FReply SStoryGraphCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!Model)
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	for (const FStoryNode& Node : Model->Nodes)
	{
		if (Node.Status == ENodeStatus::Removed || Node.Status == ENodeStatus::Proposed)
		{
			continue;
		}
		const FVector2D Pos = NodePosition(Node);
		if (Local.X >= Pos.X && Local.X <= Pos.X + NodeSize.X && Local.Y >= Pos.Y && Local.Y <= Pos.Y + NodeSize.Y)
		{
			OnNodeClicked.ExecuteIfBound(Node.NodeId);
			return FReply::Handled();
		}
	}
	return FReply::Handled();
}

// ===========================================================================
// SStoryGraphPanel
// ===========================================================================

void SStoryGraphPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	SelectedNodeId = (Model && !Model->SelectedNodeId.IsEmpty()) ? Model->SelectedNodeId : TEXT("N7");

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
				.Text_Lambda([this]()
				{
					return FText::FromString(FString::Printf(TEXT("STORY GRAPH  ·  %s  ·  graph v%d"),
						Model ? *Model->StoryTitle : TEXT(""), Model ? Model->Versions.GraphVersion : 0));
				})
				.Font(NCWidgets::Font(10, true))
				.ColorAndOpacity(NCPalette::TextPrimary)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Visibility_Lambda([]() { return NCPalette::HintVisibility(); })
				.Text(FText::FromString(TEXT("green: valid · red: needs review · amber: placeholder/unsupported · dashed blue: scoped revalidation")))
				.Font(NCWidgets::Font(8))
				.ColorAndOpacity(NCPalette::TextMuted)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(0.62f)
		.Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(NCWidgets::WhiteBrush())
			.BorderBackgroundColor(NCPalette::PanelDark)
			.Padding(2.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()
					[
						SNew(SStoryGraphCanvas)
						.Model(Model)
						.SelectedNodeId_Lambda([this]() { return SelectedNodeId; })
						.OnNodeClicked(FOnNodeClicked::CreateLambda([this](const FString& NodeId)
						{
							if (Model && SelectedNodeId != NodeId)
							{
								Model->LogEvent(TEXT("node_selected"), NodeId);
							}
							SelectedNodeId = NodeId;
							if (Model)
							{
								// Shared selection: the Live proposal source
								// expands under the selected node.
								Model->SelectedNodeId = NodeId;
							}
							RefreshDetails();
						}))
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(0.38f)
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
					SAssignNew(DetailsBox, SVerticalBox)
				]
			]
		]
	];

	if (Model)
	{
		ModelChangedHandle = Model->OnChanged.AddSP(this, &SStoryGraphPanel::RefreshDetails);
	}
	RefreshDetails();
}

SStoryGraphPanel::~SStoryGraphPanel()
{
	if (Model && ModelChangedHandle.IsValid())
	{
		Model->OnChanged.Remove(ModelChangedHandle);
	}
}

void SStoryGraphPanel::RefreshDetails()
{
	if (!DetailsBox.IsValid() || !Model)
	{
		return;
	}
	DetailsBox->ClearChildren();

	// After a brief switch the local selection may be stale; adopt the
	// model's selection (set by the brief builder).
	if (!Model->FindNode(SelectedNodeId) && Model->FindNode(Model->SelectedNodeId))
	{
		SelectedNodeId = Model->SelectedNodeId;
	}

	const FStoryNode* Node = Model->FindNode(SelectedNodeId);
	if (!Node)
	{
		DetailsBox->AddSlot().AutoHeight()
		[
			NCWidgets::BodyText(TEXT("Select a node to inspect its contract record."), NCPalette::TextMuted)
		];
		return;
	}

	// Header: id, title, chips
	{
		TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox);
		Header->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%s  %s"), *Node->NodeId, *Node->Title)))
			.Font(NCWidgets::Font(11, true))
			.ColorAndOpacity(NCPalette::TextPrimary)
		];
		Header->AddSlot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
		[
			NCWidgets::Chip(NodeStatusDisplayName(Node->Status), NCPalette::StatusColor(Node->Status))
		];
		Header->AddSlot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
		[
			NCWidgets::Chip(GroundingDisplayName(Node->Grounding),
				Node->Grounding == EGroundingStatus::ApprovedPlaceholder ? NCPalette::Warn :
				Node->Grounding == EGroundingStatus::Unresolved ? NCPalette::Blocked : NCPalette::TextMuted)
		];
		if (Node->bEnding)
		{
			Header->AddSlot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
			[
				NCWidgets::Chip(TEXT("Ending"), NCPalette::Accent)
			];
		}
		DetailsBox->AddSlot().AutoHeight()[Header];
	}

	DetailsBox->AddSlot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
	[
		NCWidgets::BodyText(Node->Description, NCPalette::TextPrimary)
	];

	auto AddListSection = [this](const FString& Title, const TArray<FString>& Lines, const FLinearColor& Color)
	{
		if (Lines.Num() == 0)
		{
			return;
		}
		DetailsBox->AddSlot().AutoHeight()[NCWidgets::SectionHeader(Title)];
		for (const FString& Line : Lines)
		{
			DetailsBox->AddSlot().AutoHeight().Padding(6.f, 1.f, 0.f, 0.f)
			[
				NCWidgets::MonoLine(Line, Color)
			];
		}
	};

	// Longer sections collapse by default to keep the record scannable.
	auto AddCollapsedSection = [this](const FString& Title, const TArray<FString>& Lines, const FLinearColor& Color)
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
		const FString PanelLabel = Title;
		DetailsBox->AddSlot().AutoHeight()
		[
			SNew(SCollapsibleSection)
			.Title(FString::Printf(TEXT("%s (%d)"), *Title, Lines.Num()))
			.InitiallyCollapsed(true)
			.OnToggled(FOnCollapsibleToggled::CreateLambda([this, PanelLabel](bool bNowExpanded)
			{
				if (Model && bNowExpanded)
				{
					Model->LogEvent(TEXT("evidence_panel_opened"), PanelLabel, SelectedNodeId);
				}
			}))
			[
				Body
			]
		];
	};

	AddListSection(TEXT("Preconditions p(v)"), Node->Preconditions, NCPalette::TextPrimary);
	AddListSection(TEXT("Add effects q+"), Node->AddEffects, NCPalette::Valid);
	AddListSection(TEXT("Delete effects q-"), Node->DelEffects, NCPalette::Blocked);
	AddCollapsedSection(TEXT("Gameplay bindings b(v)"), Node->GameplayBindings, NCPalette::TextPrimary);

	if (Node->RequiredCapabilities.Num() > 0)
	{
		TArray<FString> MappingLines;
		for (const FString& Cap : Node->RequiredCapabilities)
		{
			const FString* Impl = Node->SelectedMapping.Find(Cap);
			MappingLines.Add(FString::Printf(TEXT("%s -> %s"), *Cap, Impl ? **Impl : TEXT("(unmapped)")));
		}
		AddListSection(TEXT("Capability mapping m(v)"), MappingLines, NCPalette::Accent);
	}

	// Choices with block diagnostics
	if (Node->Choices.Num() > 0)
	{
		DetailsBox->AddSlot().AutoHeight()[NCWidgets::SectionHeader(TEXT("Choices"))];
		for (const FStoryChoice& Choice : Node->Choices)
		{
			const FString Line = FString::Printf(TEXT("%s -> %s%s"), *Choice.Label, *Choice.TargetNodeId,
				Choice.bBlocked ? TEXT("   [BLOCKED]") : TEXT(""));
			DetailsBox->AddSlot().AutoHeight().Padding(6.f, 1.f, 0.f, 0.f)
			[
				NCWidgets::MonoLine(Line, Choice.bBlocked ? NCPalette::Blocked : NCPalette::TextPrimary)
			];
			if (Choice.bBlocked && !Choice.BlockDiagnostic.IsEmpty())
			{
				DetailsBox->AddSlot().AutoHeight().Padding(14.f, 1.f, 0.f, 0.f)
				[
					NCWidgets::BodyText(Choice.BlockDiagnostic, NCPalette::Blocked, 9)
				];
			}
		}
	}

	AddCollapsedSection(TEXT("Provenance rho (append-only)"), Node->Provenance, NCPalette::TextMuted);

	// Branch state + adherence
	{
		const TSet<FString> State = Model->ComputeBranchStateAfterNode(Node->NodeId);
		TArray<FString> StateLines = State.Array();
		StateLines.Sort();
		AddCollapsedSection(TEXT("Branch state S_t after node (primary path)"), StateLines, NCPalette::TextPrimary);

		DetailsBox->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
		[
			NCWidgets::BodyText(FString::Printf(TEXT("Path adherence to approved curves: %.3f"),
				Model->ComputePathAdherence(Node->NodeId)), NCPalette::Accent, 10)
		];
	}
}
