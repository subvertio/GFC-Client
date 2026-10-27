#include "UI/GFCScreenBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "GFCApiSubsystem.h"
#include "UI/GFCRootWidget.h"

void UGFCScreenBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	BindEvents();
}

void UGFCScreenBase::OnScreenShown()
{
	// Screens open at the top, and don't jump around when an input takes focus.
	if (UScrollBox* Scroll = WidgetTree ? Cast<UScrollBox>(WidgetTree->RootWidget) : nullptr)
	{
		Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::NoScroll);
		Scroll->ScrollToStart();
	}
	K2_OnScreenShown();
}

UGFCApiSubsystem* UGFCScreenBase::Api() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UGFCApiSubsystem>() : nullptr;
}

UGFCApiClient* UGFCScreenBase::Client() const
{
	const UGFCApiSubsystem* A = Api();
	return A ? A->GetClient() : nullptr;
}

void UGFCScreenBase::ShowStatus(const FText& Message, bool bError) const
{
	if (Root.IsValid())
	{
		Root->ShowStatus(Message, bError);
	}
}

void UGFCScreenBase::SetRoot(UGFCRootWidget* InRoot)
{
	Root = InRoot;
}
