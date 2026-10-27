#include "UI/GFCSeedCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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

	// A social-feed post: avatar | name @handle · time / title / media / stats.
	CardButton = T->ConstructWidget<UButton>();
	CardButton->SetStyle(FlatButtonStyle());
	CardButton->SetTouchMethod(EButtonTouchMethod::PreciseTap); // swipes scroll the feed
	CardButton->SetClickMethod(EButtonClickMethod::PreciseClick);
	T->RootWidget = CardButton;

	UHorizontalBox* Post = MakeRow(T);
	CardButton->AddChild(Post);
	if (UButtonSlot* S = Cast<UButtonSlot>(Post->Slot))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(4.f, 12.f));
	}

	AvatarSlot = T->ConstructWidget<USizeBox>();
	AvatarSlot->SetWidthOverride(40.f);
	AddH(Post, AvatarSlot, 12.f);
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(AvatarSlot->Slot))
	{
		S->SetVerticalAlignment(VAlign_Top);
	}

	UVerticalBox* Body = MakeColumn(T);
	AddH(Post, Body, 0.f, true);

	UHorizontalBox* Header = MakeRow(T);
	OwnerText = MakeText(T, FText::GetEmpty(), 15, GFCUI::Text, true);
	OwnerText->SetAutoWrapText(false);
	HandleText = MakeText(T, FText::GetEmpty(), 14, TextDim);
	HandleText->SetAutoWrapText(false);
	AddH(Header, OwnerText, 6.f);
	AddH(Header, HandleText, 0.f, true);
	AddV(Body, Header, 2.f);

	TitleText = MakeText(T, FText::GetEmpty(), 15, GFCUI::Text);
	AddV(Body, TitleText, 10.f);

	USizeBox* ThumbBox = T->ConstructWidget<USizeBox>();
	ThumbBox->SetHeightOverride(220.f);
	ThumbBox->SetClipping(EWidgetClipping::ClipToBounds);
	UBorder* ThumbBg = MakeCard(T, FMargin(0.f));
	UScaleBox* Scale = T->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFill);
	ThumbImage = T->ConstructWidget<UImage>();
	Scale->AddChild(ThumbImage);
	ThumbBg->AddChild(Scale);
	ThumbBox->AddChild(ThumbBg);
	AddV(Body, ThumbBox, 10.f);

	UHorizontalBox* Stats = MakeRow(T);
	KindText = MakeText(T, FText::GetEmpty(), 13, AccentText, true);
	KindText->SetAutoWrapText(false);
	MetaText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	AddH(Stats, KindText, 14.f);
	AddH(Stats, MetaText, 0.f, true);
	AddV(Body, Stats, 0.f);
}

void UGFCSeedCard::SetSeed(const FGFCSeed& InSeed)
{
	Seed = InSeed;
	if (TitleText) TitleText->SetText(FText::FromString(Seed.Title));
	const FString Owner = Seed.Owner.IsEmpty() ? FString(TEXT("unknown")) : Seed.Owner;
	if (OwnerText) OwnerText->SetText(FText::FromString(Owner));
	if (HandleText)
	{
		const FText When = RelativeTime(Seed.CreatedAt);
		HandleText->SetText(When.IsEmpty() ? FText::FromString(TEXT("@") + Owner)
			: FText::Format(LOCTEXT("HandleTime", "@{0} · {1}"), FText::FromString(Owner), When));
	}
	if (AvatarSlot)
	{
		AvatarSlot->ClearChildren();
		AvatarSlot->AddChild(MakeAvatar(WidgetTree, Owner, 40.f));
	}
	if (KindText) KindText->SetText(GFCMediaKindLabel(Seed.GetMediaKind()));
	if (MetaText)
	{
		MetaText->SetText(FText::Format(LOCTEXT("Meta", "{0} views    {1} likes    {2} comments"),
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
