// Story-brief picker: built-in worlds plus user JSON briefs from
// <Project>/Briefs/, with "save current state as a brief".

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FContractModel;
class SVerticalBox;

class SBriefPickerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBriefPickerPanel)
		: _Model(nullptr)
	{}
		SLATE_ARGUMENT(FContractModel*, Model)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FContractModel* Model = nullptr;
	FSimpleDelegate OnClose;
	TSharedPtr<SVerticalBox> ListBox;

	void RefreshList();
	void LoadBuiltIn(int32 Index);
	void LoadJson(const FString& FileName);
};
