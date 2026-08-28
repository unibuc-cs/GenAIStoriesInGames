// In-demo HUD overlay: objective line, branch-local facts, runtime evidence,
// axis trace vs approved targets, and the last gate message.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/WeakObjectPtrTemplates.h"

class ADemoDirector;

class SDemoHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDemoHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ADemoDirector>, Director)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TWeakObjectPtr<ADemoDirector> Director;
};
