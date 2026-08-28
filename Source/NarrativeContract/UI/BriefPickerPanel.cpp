#include "BriefPickerPanel.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "UiCommon.h"
#include "../Core/ContractModel.h"
#include "../Core/Briefs.h"

void SBriefPickerPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	OnClose = InArgs._OnClose;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(NCWidgets::WhiteBrush())
		.BorderBackgroundColor(NCPalette::Panel)
		.Padding(12.f)
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
					.Text(FText::FromString(TEXT("STORY BRIEFS")))
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
						if (Model)
						{
							Model->LiveStatus = Briefs::SaveCurrentAsBrief(*Model);
							Model->LogEvent(TEXT("brief_saved"), Model->StoryTitle);
							Model->OnChanged.Broadcast();
							RefreshList();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Save current as brief")))
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
						.Font(NCWidgets::Font(9))
						.ColorAndOpacity(NCPalette::TextMuted)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 8.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Switching briefs replaces the working state (undoable). JSON briefs use the same format as Save files -- drop any saved state into the Briefs folder to make it a brief.")))
				.Font(NCWidgets::Font(8))
				.AutoWrapText(true)
				.ColorAndOpacity(NCPalette::TextMuted)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(ListBox, SVerticalBox)
				]
			]
		]
	];

	RefreshList();
}

void SBriefPickerPanel::LoadBuiltIn(int32 Index)
{
	const TArray<FBriefDescriptor>& Registry = Briefs::BuiltIn();
	if (!Model || !Registry.IsValidIndex(Index) || !Registry[Index].Build)
	{
		return;
	}
	Model->PushUndoSnapshot(FString(), FString::Printf(TEXT("Switch to brief '%s'"), *Registry[Index].Name));
	Registry[Index].Build(*Model);
	Model->LogEvent(TEXT("brief_loaded"), Registry[Index].Id);
	Model->OnChanged.Broadcast();
	OnClose.ExecuteIfBound();
}

void SBriefPickerPanel::LoadJson(const FString& FileName)
{
	if (!Model)
	{
		return;
	}
	Model->PushUndoSnapshot(FString(), FString::Printf(TEXT("Switch to brief file '%s'"), *FileName));
	Model->LiveStatus = Briefs::LoadJsonBrief(*Model, FileName);
	Model->LogEvent(TEXT("brief_loaded"), FileName);
	Model->OnChanged.Broadcast();
	OnClose.ExecuteIfBound();
}

void SBriefPickerPanel::RefreshList()
{
	if (!ListBox.IsValid())
	{
		return;
	}
	ListBox->ClearChildren();

	ListBox->AddSlot().AutoHeight()[NCWidgets::SectionHeader(TEXT("Built-in briefs"))];

	const TArray<FBriefDescriptor>& Registry = Briefs::BuiltIn();
	for (int32 i = 0; i < Registry.Num(); ++i)
	{
		const FBriefDescriptor& Brief = Registry[i];
		const bool bActive = Model && Model->StoryTitle == Brief.Name;

		ListBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8.f, 6.f))
			.ButtonColorAndOpacity(bActive ? NCPalette::PanelLight : NCPalette::PanelDark)
			.OnClicked_Lambda([this, i]()
			{
				LoadBuiltIn(i);
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
						.Text(FText::FromString(Brief.Name))
						.Font(NCWidgets::Font(10, true))
						.ColorAndOpacity(NCPalette::TextPrimary)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						NCWidgets::Chip(Brief.Genre, bActive ? NCPalette::Valid : NCPalette::Accent)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Brief.Description))
					.Font(NCWidgets::Font(8))
					.AutoWrapText(true)
					.ColorAndOpacity(NCPalette::TextMuted)
				]
			]
		];
	}

	const TArray<FString> JsonBriefs = Briefs::DiscoverJsonBriefs();
	ListBox->AddSlot().AutoHeight()
	[
		NCWidgets::SectionHeader(FString::Printf(TEXT("Brief files in Briefs/ (%d)"), JsonBriefs.Num()))
	];
	if (JsonBriefs.Num() == 0)
	{
		ListBox->AddSlot().AutoHeight()
		[
			NCWidgets::BodyText(TEXT("None yet. 'Save current as brief' writes one here."), NCPalette::TextMuted, 9)
		];
	}
	for (const FString& FileName : JsonBriefs)
	{
		ListBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8.f, 5.f))
			.ButtonColorAndOpacity(NCPalette::PanelDark)
			.OnClicked_Lambda([this, FileName]()
			{
				LoadJson(FileName);
				return FReply::Handled();
			})
			[
				NCWidgets::MonoLine(FileName, NCPalette::TextPrimary)
			]
		];
	}
}
