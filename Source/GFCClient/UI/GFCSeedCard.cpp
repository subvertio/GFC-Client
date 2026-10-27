#include "UI/GFCSeedCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "UI/GFCImageCache.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCSeedCard"

using namespace GFCUI;

FText GFCMediaKindLabel(EGFCMediaKind Kind)
{
	switch (Kind)
	{
	case EGFCMediaKind::Model3D: return LOCTEXT("Model", "3D model");
	case EGFCMediaKind::Image: return LOCTEXT("Image", "Image");
	case EGFCMediaKind::Video: return LOCTEXT("Video", "Video");
	case EGFCMediaKind::Audio: return LOCTEXT("Audio", "Audio");
	default: return LOCTEXT("Other", "File");
	}
}

void GFCLoadImageInto(UUserWidget* Owner, UImage* Image, const FString& Url)
{
	if (!Owner || !Image)
	{
		return;
	}
	// Which URL each image is currently meant to show, so a late download for a
	// previous seed doesn't overwrite a reused widget.
	static TMap<TWeakObjectPtr<UImage>, FString> Wanted;
	for (auto It = Wanted.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	Wanted.Add(Image, Url);

	Image->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f)); // hidden until loaded
	UGFCImageCache* Cache = Owner->GetGameInstance() ? Owner->GetGameInstance()->GetSubsystem<UGFCImageCache>() : nullptr;
	if (!Cache || Url.IsEmpty())
	{
		return;
	}
	TWeakObjectPtr<UImage> WeakImage(Image);
	Cache->Request(Url, [WeakImage, Url](UTexture2D* Tex)
	{
		const FString* Expected = Wanted.Find(WeakImage);
		if (WeakImage.IsValid() && Tex && Expected && *Expected == Url)
		{
			WeakImage->SetBrushFromTexture(Tex);
			WeakImage->SetColorAndOpacity(FLinearColor::White);
		}
	});
}

void UGFCSeedCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	if (CardButton)
	{
		CardButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}
}

void UGFCSeedCard::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;

	CardButton = T->ConstructWidget<UButton>();
	CardButton->SetBackgroundColor(PanelRaised);
	T->RootWidget = CardButton;

	UHorizontalBox* Row = MakeRow(T);
	CardButton->AddChild(Row);
	if (UButtonSlot* S = Cast<UButtonSlot>(Row->Slot))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(4.f));
	}

	USizeBox* ThumbBox = T->ConstructWidget<USizeBox>();
	ThumbBox->SetWidthOverride(112.f);
	ThumbBox->SetHeightOverride(112.f);
	UBorder* ThumbBg = MakePanel(T, Background, FMargin(0.f));
	ThumbImage = T->ConstructWidget<UImage>();
	ThumbBg->AddChild(ThumbImage);
	ThumbBox->AddChild(ThumbBg);
	AddH(Row, ThumbBox, 16.f);

	UVerticalBox* Col = MakeColumn(T);
	TitleText = MakeText(T, FText::GetEmpty(), 16, Text, true);
	OwnerText = MakeText(T, FText::GetEmpty(), 12, Accent);
	KindText = MakeText(T, FText::GetEmpty(), 11, TextDim);
	MetaText = MakeText(T, FText::GetEmpty(), 11, TextDim);
	AddV(Col, TitleText, 2.f);
	AddV(Col, OwnerText, 6.f);
	AddV(Col, KindText, 2.f);
	AddV(Col, MetaText, 0.f);
	AddH(Row, Col, 0.f, true);
}

void UGFCSeedCard::SetSeed(const FGFCSeed& InSeed)
{
	Seed = InSeed;
	if (TitleText) TitleText->SetText(FText::FromString(Seed.Title));
	if (OwnerText) OwnerText->SetText(FText::Format(LOCTEXT("By", "by {0}"), FText::FromString(Seed.Owner.IsEmpty() ? TEXT("unknown") : Seed.Owner)));
	if (KindText) KindText->SetText(GFCMediaKindLabel(Seed.GetMediaKind()));
	if (MetaText)
	{
		MetaText->SetText(FText::Format(LOCTEXT("Meta", "{0} views · {1} likes · {2} comments"),
			FText::AsNumber(Seed.Views), FText::AsNumber(Seed.Likes), FText::AsNumber(Seed.Comments)));
	}
	if (ThumbImage)
	{
		const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
		GFCLoadImageInto(this, ThumbImage, C->ResolveMediaUrl(Seed.Thumb));
	}
}

void UGFCSeedCard::HandleClicked()
{
	OnSelected.ExecuteIfBound(Seed);
}

#undef LOCTEXT_NAMESPACE
