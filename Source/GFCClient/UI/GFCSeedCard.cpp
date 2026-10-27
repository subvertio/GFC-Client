#include "UI/GFCSeedCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "GFCCacheSubsystem.h"
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

	Image->SetBrushResourceObject(nullptr);
	Image->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f)); // hidden until loaded
	UGFCCacheSubsystem* Cache = Owner->GetGameInstance() ? Owner->GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>() : nullptr;
	if (!Cache || Url.IsEmpty())
	{
		return;
	}
	TWeakObjectPtr<UImage> WeakImage(Image);
	Cache->RequestThumbnail(Url, [WeakImage, Url](UTexture2D* Tex)
	{
		const FString* Expected = Wanted.Find(WeakImage);
		if (WeakImage.IsValid() && Tex && Expected && *Expected == Url)
		{
			WeakImage->SetBrushFromTexture(Tex, true);
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
	CardButton->SetTouchMethod(EButtonTouchMethod::PreciseTap); // swipes scroll the feed
	CardButton->SetClickMethod(EButtonClickMethod::PreciseClick);
	T->RootWidget = CardButton;

	UVerticalBox* Col = MakeColumn(T);
	CardButton->AddChild(Col);
	if (UButtonSlot* S = Cast<UButtonSlot>(Col->Slot))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	// Thumbnail on top, full width, cropped to fill.
	USizeBox* ThumbBox = T->ConstructWidget<USizeBox>();
	ThumbBox->SetHeightOverride(200.f);
	ThumbBox->SetClipping(EWidgetClipping::ClipToBounds);
	UBorder* ThumbBg = MakePanel(T, Background, FMargin(0.f));
	UScaleBox* Scale = T->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFill);
	ThumbImage = T->ConstructWidget<UImage>();
	Scale->AddChild(ThumbImage);
	ThumbBg->AddChild(Scale);
	ThumbBox->AddChild(ThumbBg);
	AddV(Col, ThumbBox, 8.f);

	UVerticalBox* TextCol = MakeColumn(T);
	TitleText = MakeText(T, FText::GetEmpty(), 17, GFCUI::Text, true);
	OwnerText = MakeText(T, FText::GetEmpty(), 13, Accent);
	KindText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	MetaText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	AddV(TextCol, TitleText, 2.f);
	AddV(TextCol, OwnerText, 4.f);
	UHorizontalBox* Row = MakeRow(T);
	AddH(Row, KindText, 10.f);
	AddH(Row, MetaText, 0.f, true);
	AddV(TextCol, Row, 0.f);
	UBorder* Pad = MakePanel(T, FLinearColor::Transparent, FMargin(12.f, 0.f));
	Pad->AddChild(TextCol);
	AddV(Col, Pad, 0.f);
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
	RestoreContent();
}

FString UGFCSeedCard::ThumbUrl() const
{
	const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	return C->ResolveMediaUrl(Seed.Thumb);
}

void UGFCSeedCard::ReleaseContent()
{
	if (!bContentLoaded)
	{
		return;
	}
	bContentLoaded = false;
	if (ThumbImage)
	{
		ThumbImage->SetBrushResourceObject(nullptr);
		ThumbImage->SetColorAndOpacity(FLinearColor::Transparent);
	}
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Cache->ReleaseOwner(OwnerKey());
	}
}

void UGFCSeedCard::RestoreContent()
{
	if (bContentLoaded)
	{
		return;
	}
	bContentLoaded = true;
	if (ThumbImage)
	{
		GFCLoadImageInto(this, ThumbImage, ThumbUrl());
	}
}

void UGFCSeedCard::HandleClicked()
{
	OnSelected.ExecuteIfBound(Seed);
}

#undef LOCTEXT_NAMESPACE
