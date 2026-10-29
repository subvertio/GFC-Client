#include "UI/GFCActionSheet.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "UI/GFCPostEditor.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCActionSheet"

using namespace GFCUI;

namespace
{
	TWeakObjectPtr<UGFCActionSheet> GOpenSheet;
}

UGFCActionSheet* UGFCActionSheet::Show(UObject* WorldContext, const FText& Title, const TArray<FText>& Options, FOnPick InOnPick,
	bool bLastIsDanger)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !World->GetGameInstance())
	{
		return nullptr;
	}
	CloseOpen();
	UGFCActionSheet* Sheet = CreateWidget<UGFCActionSheet>(World->GetGameInstance(), UGFCActionSheet::StaticClass());
	Sheet->OnPick = MoveTemp(InOnPick);
	Sheet->Build(Title, Options, bLastIsDanger);
	Sheet->AddToViewport(900); // over the screens, under the earnings burst
	GOpenSheet = Sheet;
	return Sheet;
}

bool UGFCActionSheet::CloseOpen()
{
	if (UGFCActionSheet* Sheet = GOpenSheet.Get())
	{
		Sheet->Finish(-1);
		return true;
	}
	return false;
}

void UGFCActionSheet::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	// The dimmed screen behind: tapping it cancels.
	Dim = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle DimStyle = FlatButtonStyle();
	DimStyle.Normal.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	DimStyle.Hovered.TintColor = DimStyle.Normal.TintColor;
	DimStyle.Pressed.TintColor = DimStyle.Normal.TintColor;
	Dim->SetStyle(DimStyle);
	Dim->OnClicked.AddDynamic(this, &ThisClass::HandleCancel);
	if (UOverlaySlot* S = Root->AddChildToOverlay(Dim))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UBorder* Sheet = MakeCard(WidgetTree, FMargin(16.f, 16.f, 16.f, 28.f));
	List = MakeColumn(WidgetTree);
	Sheet->AddChild(List);
	if (UOverlaySlot* S = Root->AddChildToOverlay(Sheet))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(8.f, 0.f, 8.f, 8.f));
	}
}

void UGFCActionSheet::Build(const FText& Title, const TArray<FText>& Options, bool bLastIsDanger)
{
	if (!Title.IsEmpty())
	{
		UTextBlock* T = MakeText(WidgetTree, Title, 14, TextDim);
		T->SetJustification(ETextJustify::Center);
		AddV(List, T, 12.f);
	}
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		const bool bDanger = bLastIsDanger && i == Options.Num() - 1;
		UTextBlock* Label = nullptr;
		UButton* B = MakeButton(WidgetTree, Options[i], PanelRaised, &Label);
		if (bDanger && Label) Label->SetColorAndOpacity(FSlateColor(Danger));
		UGFCSheetChoice* Choice = NewObject<UGFCSheetChoice>(this);
		Choice->Index = i;
		Choice->Sheet = this;
		B->OnClicked.AddDynamic(Choice, &UGFCSheetChoice::Clicked);
		Choices.Add(Choice);
		AddV(List, B, 8.f);
	}
	UButton* Cancel = MakeButton(WidgetTree, LOCTEXT("Cancel", "Cancel"), Background);
	Cancel->OnClicked.AddDynamic(this, &ThisClass::HandleCancel);
	AddV(List, Cancel, 0.f);
}

void UGFCSheetChoice::Clicked()
{
	if (Sheet.IsValid())
	{
		Sheet->Finish(Index);
	}
}

void UGFCActionSheet::HandleCancel()
{
	Finish(-1);
}

void UGFCActionSheet::Finish(int32 Index)
{
	if (bDone)
	{
		return;
	}
	bDone = true;
	FOnPick Pick = MoveTemp(OnPick);
	if (GOpenSheet.Get() == this)
	{
		GOpenSheet.Reset();
	}
	RemoveFromParent();
	if (Pick)
	{
		Pick(Index); // may open the next sheet
	}
}

void UGFCActionSheet::NativeDestruct()
{
	if (GOpenSheet.Get() == this)
	{
		GOpenSheet.Reset();
	}
	Super::NativeDestruct();
}

// ---------------------------------------------------------------- report / block

namespace
{
	void Toast(UObject* Ctx, const FText& Message, bool bError = false)
	{
		if (UGFCRootWidget* Root = UGFCRootWidget::Find(Ctx))
		{
			Root->ShowStatus(Message, bError);
		}
	}

	struct FReason { const TCHAR* Key; FText Label; };

	const TArray<FReason>& Reasons()
	{
		static const TArray<FReason> R = {
			{TEXT("spam"), LOCTEXT("Spam", "Spam")},
			{TEXT("nudity"), LOCTEXT("Nudity", "Nudity or sexual content")},
			{TEXT("violence"), LOCTEXT("Violence", "Violence or gore")},
			{TEXT("harassment"), LOCTEXT("Harassment", "Harassment or bullying")},
			{TEXT("hate"), LOCTEXT("Hate", "Hate speech")},
			{TEXT("scam"), LOCTEXT("Scam", "Scam or fraud")},
			{TEXT("copyright"), LOCTEXT("Copyright", "Stolen or copyrighted work")},
			{TEXT("other"), LOCTEXT("Other", "Something else")},
		};
		return R;
	}
}

void GFCShowOwnPostActions(UObject* WorldContext, const FGFCSeed& Seed)
{
	TArray<FText> Options = {LOCTEXT("Edit", "Edit post")};
	if (!Seed.EditedAt.IsEmpty()) Options.Add(LOCTEXT("History", "Edit history"));
	TWeakObjectPtr<UObject> WeakCtx(WorldContext);
	UGFCActionSheet::Show(WorldContext, FText::GetEmpty(), Options, [WeakCtx, Seed](int32 Choice)
	{
		if (!WeakCtx.IsValid()) return;
		if (Choice == 0)
		{
			UGFCPostPanel::ShowEditor(WeakCtx.Get(), Seed, [WeakCtx](const FGFCSeed& Updated)
			{
				if (!WeakCtx.IsValid()) return;
				if (UGFCRootWidget* Root = UGFCRootWidget::Find(WeakCtx.Get())) Root->OnPostEdited(Updated);
				Toast(WeakCtx.Get(), LOCTEXT("Saved", "Post updated."));
			});
		}
		else if (Choice == 1)
		{
			UGFCPostPanel::ShowHistory(WeakCtx.Get(), Seed);
		}
	});
}

void GFCShowPostActions(UObject* WorldContext, const FGFCSeed& Seed, TFunction<void()> OnHidden)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGFCApiClient* C = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient() : nullptr;
	if (!C)
	{
		return;
	}
	if (!C->IsLoggedIn())
	{
		Toast(WorldContext, LOCTEXT("LogIn", "Log in to report a post or block an account."), true);
		return;
	}
	if (Seed.Owner.Equals(C->GetUsername(), ESearchCase::IgnoreCase))
	{
		GFCShowOwnPostActions(WorldContext, Seed);
		return;
	}

	TWeakObjectPtr<UObject> WeakCtx(WorldContext);
	TWeakObjectPtr<UGFCApiClient> WeakClient(C);
	const FString SeedId = Seed.Id, Owner = Seed.Owner;
	UGFCActionSheet::Show(WorldContext, FText::GetEmpty(),
		{LOCTEXT("Report", "Report post"), FText::Format(LOCTEXT("Block", "Block @{0}"), FText::FromString(Owner))},
		[WeakCtx, WeakClient, SeedId, Owner, OnHidden](int32 Choice)
		{
			if (!WeakCtx.IsValid() || !WeakClient.IsValid())
			{
				return;
			}
			if (Choice == 0)
			{
				TArray<FText> Labels;
				for (const FReason& R : Reasons()) Labels.Add(R.Label);
				UGFCActionSheet::Show(WeakCtx.Get(), LOCTEXT("Why", "Why are you reporting this post?"), Labels,
					[WeakCtx, WeakClient, SeedId, OnHidden](int32 Reason)
					{
						if (Reason < 0 || !WeakCtx.IsValid() || !WeakClient.IsValid())
						{
							return;
						}
						WeakClient->ReportSeed(SeedId, Reasons()[Reason].Key, [WeakCtx, OnHidden](const FGFCApiResult& R)
						{
							if (!WeakCtx.IsValid()) return;
							if (!R.bOk)
							{
								Toast(WeakCtx.Get(), FText::FromString(R.Error), true);
								return;
							}
							if (OnHidden) OnHidden();
							Toast(WeakCtx.Get(), LOCTEXT("Reported", "Thanks for reporting. We've hidden this post for you."));
						});
					});
			}
			else if (Choice == 1)
			{
				UGFCActionSheet::Show(WeakCtx.Get(),
					FText::Format(LOCTEXT("BlockWhat", "Block @{0}? You won't see their posts. They aren't told."), FText::FromString(Owner)),
					{FText::Format(LOCTEXT("BlockConfirm", "Block @{0}"), FText::FromString(Owner))},
					[WeakCtx, WeakClient, Owner, OnHidden](int32 Confirm)
					{
						if (Confirm != 0 || !WeakCtx.IsValid() || !WeakClient.IsValid())
						{
							return;
						}
						WeakClient->BlockUser(Owner, [WeakCtx, Owner, OnHidden](const FGFCApiResult& R)
						{
							if (!WeakCtx.IsValid()) return;
							if (!R.bOk)
							{
								Toast(WeakCtx.Get(), FText::FromString(R.Error), true);
								return;
							}
							if (OnHidden) OnHidden();
							Toast(WeakCtx.Get(), FText::Format(LOCTEXT("Blocked", "Blocked @{0}."), FText::FromString(Owner)));
						});
					}, true);
			}
		});
}

#undef LOCTEXT_NAMESPACE
