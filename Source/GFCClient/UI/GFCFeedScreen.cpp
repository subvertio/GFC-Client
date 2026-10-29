#include "UI/GFCFeedScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "GFCMediaCapture.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCUpload.h"
#include "GFCApiSettings.h"
#include "GFCCacheSubsystem.h"
#include "UI/GFCActionSheet.h"
#include "UI/GFCCreateScreen.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCSeedCard.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCFeed"

using namespace GFCUI;

UGFCFeedScreen::UGFCFeedScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SeedCardClass = UGFCSeedCard::StaticClass();
}

void UGFCFeedScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;
	UVerticalBox* Col = MakeColumn(T);
	T->RootWidget = Col;

	UHorizontalBox* Header = MakeRow(T);
	PhoneTitle = MakeText(T, LOCTEXT("Title", "Feed"), 22, Text, true);
	PhoneTitle->SetAutoWrapText(false);
	PhoneTitle->SetClipping(EWidgetClipping::ClipToBounds);
	AddH(Header, PhoneTitle, 0.f, true);
	RefreshButton = MakeButton(T, LOCTEXT("Refresh", "Refresh"), PanelRaised);
	AddH(Header, RefreshButton, 0.f);
	AddV(Col, Header, 12.f);
	PhoneHeader = Header;

	BuildDesktopHeader(Col);

	EmptyText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	AddV(Col, EmptyText, 8.f);

	SeedList = T->ConstructWidget<UScrollBox>();
	AddV(Col, SeedList, 0.f, true);
}

void UGFCFeedScreen::BuildDesktopHeader(UVerticalBox* Col)
{
	UWidgetTree* T = WidgetTree;
	const FGFCTheme& Th = FGFCTheme::Get();
	UVerticalBox* Desk = MakeColumn(T);

	// For you: one tab, bold with an accent underline (the node has no following feed).
	UVerticalBox* Tabs = MakeColumn(T);
	ForYouTab = T->ConstructWidget<UButton>();
	ForYouTab->SetStyle(FlatButtonStyle());
	UVerticalBox* TabCol = MakeColumn(T);
	UTextBlock* TabLabel = MakeText(T, LOCTEXT("ForYou", "For you"), 15, Th.Text, true);
	TabLabel->SetAutoWrapText(false);
	AddV(TabCol, TabLabel, 12.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(TabLabel->Slot)) S->SetHorizontalAlignment(HAlign_Center);
	USizeBox* Underline = T->ConstructWidget<USizeBox>();
	Underline->SetWidthOverride(56.f);
	Underline->SetHeightOverride(4.f);
	UBorder* Bar = T->ConstructWidget<UBorder>();
	Bar->SetBrush(FSlateRoundedBoxBrush(Th.Accent, 2.f));
	Underline->AddChild(Bar);
	AddV(TabCol, Underline, 0.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Underline->Slot)) S->SetHorizontalAlignment(HAlign_Center);
	ForYouTab->AddChild(TabCol);
	if (UButtonSlot* S = Cast<UButtonSlot>(TabCol->Slot))
	{
		S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Fill);
	}
	ForYouTab->SetToolTipText(LOCTEXT("ForYouTip", "Refresh"));
	AddV(Tabs, ForYouTab, 0.f);
	AddV(Desk, Tabs, 0.f);
	TabBar = Tabs;

	// Other pages: a title (and Explore's search box).
	UBorder* TitlePad = MakePanel(T, Th.Background, FMargin(16.f, 10.f, 16.f, 12.f));
	UVerticalBox* TitleCol = MakeColumn(T);
	PageTitle = MakeText(T, FText::GetEmpty(), 20, Th.Text, true);
	PageSubtitle = MakeText(T, FText::GetEmpty(), 13, Th.TextSecondary);
	AddV(TitleCol, PageTitle, 2.f);
	AddV(TitleCol, PageSubtitle, 0.f);
	SearchBox = MakeSearchBox(T, LOCTEXT("SearchHint", "Search GFC"));
	AddV(TitleCol, SearchBox, 0.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(SearchBox->Slot)) S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	TitlePad->AddChild(TitleCol);
	AddV(Desk, TitlePad, 0.f);
	TitleBar = TitlePad;
	AddV(Desk, MakeDivider(T), 0.f);

	// The composer: avatar | "What's happening?" / photo, 3D ... Post.
	UVerticalBox* ComposeCol = MakeColumn(T);
	UBorder* ComposePad = MakePanel(T, Th.Background, FMargin(16.f, 12.f, 16.f, 12.f));
	UHorizontalBox* ComposeRow = MakeRow(T);
	ComposerAvatar = T->ConstructWidget<USizeBox>();
	ComposerAvatar->SetWidthOverride(40.f);
	ComposerAvatar->SetHeightOverride(40.f);
	AddH(ComposeRow, ComposerAvatar, 12.f);
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(ComposerAvatar->Slot)) S->SetVerticalAlignment(VAlign_Top);
	UVerticalBox* ComposeBody = MakeColumn(T);
	ComposerBox = MakeMultiLineInput(T, LOCTEXT("WhatsHappening", "What's happening?"));
	{
		FEditableTextBoxStyle Style = ComposerBox->WidgetStyle;
		FSlateFontInfo Font = Style.TextStyle.Font;
		Font.Size = 20;
		Style.SetFont(Font);
		Style.SetPadding(FMargin(0.f, 8.f));
		const FSlateRoundedBoxBrush Clear(FLinearColor::Transparent, 0.f);
		Style.SetBackgroundImageNormal(Clear);
		Style.SetBackgroundImageHovered(Clear);
		Style.SetBackgroundImageFocused(Clear);
		ComposerBox->WidgetStyle = Style;
	}
	AddV(ComposeBody, MinHeight(T, ComposerBox, 52.f), 8.f);
	AddV(ComposeBody, MakeDivider(T), 8.f);
	UHorizontalBox* Tools = MakeRow(T);
	auto Tool = [T, &Th](const TCHAR* Glyph, const FText& Label, const FText& Tip) -> UButton*
	{
		UButton* B = T->ConstructWidget<UButton>();
		B->SetStyle(GhostButtonStyle(Th.AccentTint, 17.f));
		UTextBlock* G = MakeText(T, FText::Format(LOCTEXT("ToolLabel", "{0}  {1}"), FText::FromString(Glyph), Label), 14, Th.Accent, true);
		G->SetAutoWrapText(false);
		B->AddChild(G);
		if (UButtonSlot* S = Cast<UButtonSlot>(G->Slot)) S->SetPadding(FMargin(10.f, 6.f));
		B->SetToolTipText(Tip);
		return B;
	};
	ComposerMediaButton = Tool(Icon::Media, LOCTEXT("MediaLabel", "Media"), LOCTEXT("MediaTip", "Photo or video"));
	ComposerFileButton = Tool(Icon::Model3D, LOCTEXT("FileLabel", "3D"), LOCTEXT("FileTip", "3D model, audio or another file"));
	AddH(Tools, ComposerMediaButton, 2.f);
	AddH(Tools, ComposerFileButton, 0.f);
	AddH(Tools, T->ConstructWidget<USpacer>(), 0.f, true);
	ComposerPostButton = MakeButton(T, LOCTEXT("Post", "Post"));
	if (UButtonSlot* S = Cast<UButtonSlot>(ComposerPostButton->GetChildAt(0)->Slot)) S->SetPadding(FMargin(18.f, 7.f));
	AddH(Tools, ComposerPostButton, 0.f);
	AddV(ComposeBody, Tools, 0.f);
	AddH(ComposeRow, ComposeBody, 0.f, true);
	ComposePad->AddChild(ComposeRow);
	AddV(ComposeCol, ComposePad, 0.f);
	AddV(ComposeCol, MakeDivider(T), 0.f);
	AddV(Desk, ComposeCol, 0.f);
	Composer = ComposeCol;

	AddV(Col, Desk, 0.f);
	DesktopHeader = Desk;
	DesktopHeader->SetVisibility(ESlateVisibility::Collapsed);
}

void UGFCFeedScreen::BindEvents()
{
	if (RefreshButton) RefreshButton->OnClicked.AddDynamic(this, &ThisClass::HandleRefresh);
	if (ForYouTab) ForYouTab->OnClicked.AddDynamic(this, &ThisClass::HandleRefresh);
	if (ComposerBox) ComposerBox->OnTextChanged.AddDynamic(this, &ThisClass::HandleComposerChanged);
	if (ComposerMediaButton) ComposerMediaButton->OnClicked.AddDynamic(this, &ThisClass::HandleComposerMedia);
	if (ComposerFileButton) ComposerFileButton->OnClicked.AddDynamic(this, &ThisClass::HandleComposerFile);
	if (ComposerPostButton) ComposerPostButton->OnClicked.AddDynamic(this, &ThisClass::HandleComposerPost);
	if (SearchBox) SearchBox->OnTextCommitted.AddDynamic(this, &ThisClass::HandleSearchCommitted);
	HandleComposerChanged(FText::GetEmpty());
}

void UGFCFeedScreen::SetSource(EGFCFeedSource InSource, const FString& InQuery)
{
	const FString NewQuery = InQuery.TrimStartAndEnd();
	const bool bChanged = InSource != Source || NewQuery != Query;
	if (bChanged)
	{
		++LoadSerial; // a load still running for the old page is ignored
		bLoading = false;
	}
	Source = InSource;
	Query = NewQuery;
	if (SearchBox) SearchBox->SetText(Source == EGFCFeedSource::Explore ? FText::FromString(Query) : FText::GetEmpty());
	if (Source == EGFCFeedSource::User && bChanged)
	{
		// Another account's page shows at once: their name now, an empty list saying it's loading.
		UserProfile = FGFCProfile();
		UserProfile.Username = Query;
		Populate({});
		if (EmptyText) EmptyText->SetText(FText::Format(LOCTEXT("LoadingUser", "Loading @{0}'s posts…"), FText::FromString(Query)));
		if (UGFCApiClient* C = Client())
		{
			TWeakObjectPtr<UGFCFeedScreen> WeakThis(this);
			const uint32 Serial = LoadSerial;
			C->GetUserProfile(Query, [WeakThis, Serial](const FGFCApiResult& R, const FGFCProfile& P)
			{
				if (R.bOk && WeakThis.IsValid() && WeakThis->LoadSerial == Serial)
				{
					WeakThis->UserProfile = P;
					WeakThis->UpdateHeader();
				}
			});
		}
	}
	UpdateHeader();
}

void UGFCFeedScreen::SetWideLayout(bool bInWide)
{
	bWide = bInWide;
	if (PhoneHeader) PhoneHeader->SetVisibility(bWide ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (DesktopHeader) DesktopHeader->SetVisibility(bWide ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (EmptyText)
	{
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(EmptyText->Slot))
		{
			S->SetPadding(bWide ? FMargin(16.f, 10.f, 16.f, 6.f) : FMargin(0.f, 0.f, 0.f, 8.f));
		}
	}
	if (SeedList)
	{
		for (UWidget* W : SeedList->GetAllChildren())
		{
			if (UGFCSeedCard* Card = Cast<UGFCSeedCard>(W)) Card->SetWideLayout(bWide);
		}
	}
	UpdateHeader();
}

void UGFCFeedScreen::UpdateHeader()
{
	const UGFCApiClient* C = Client();
	const bool bIn = C && C->IsLoggedIn();
	const bool bForYou = Source == EGFCFeedSource::ForYou;
	if (TabBar) TabBar->SetVisibility(bForYou ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (TitleBar) TitleBar->SetVisibility(bForYou ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (Composer) Composer->SetVisibility(bForYou && bIn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (SearchBox) SearchBox->SetVisibility(Source == EGFCFeedSource::Explore ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (ComposerAvatar && bIn)
	{
		ComposerAvatar->ClearChildren();
		ComposerAvatar->AddChild(MakeAvatar(WidgetTree, C->GetUsername(), 40.f));
	}
	if (PageTitle)
	{
		switch (Source)
		{
		case EGFCFeedSource::Explore: PageTitle->SetText(LOCTEXT("ExploreTitle", "Explore")); break;
		case EGFCFeedSource::Bookmarks: PageTitle->SetText(LOCTEXT("BookmarksTitle", "Bookmarks")); break;
		case EGFCFeedSource::Mine: PageTitle->SetText(bIn ? FText::FromString(C->GetUsername()) : LOCTEXT("ProfileTitle", "Profile")); break;
		case EGFCFeedSource::User: PageTitle->SetText(FText::FromString(UserProfile.Username.IsEmpty() ? Query : UserProfile.Username)); break;
		default: PageTitle->SetText(FText::GetEmpty()); break;
		}
	}
	if (PhoneTitle)
	{
		// Says whose page this is on phones too, so it's clear the Feed button goes back to everyone's posts
		switch (Source)
		{
		case EGFCFeedSource::Explore: PhoneTitle->SetText(LOCTEXT("ExploreTitle", "Explore")); break;
		case EGFCFeedSource::Bookmarks: PhoneTitle->SetText(LOCTEXT("BookmarksTitle", "Bookmarks")); break;
		case EGFCFeedSource::Mine: PhoneTitle->SetText(bIn ? FText::FromString(TEXT("@") + C->GetUsername()) : LOCTEXT("ProfileTitle", "Profile")); break;
		case EGFCFeedSource::User: PhoneTitle->SetText(FText::FromString(TEXT("@") + (UserProfile.Username.IsEmpty() ? Query : UserProfile.Username))); break;
		default: PhoneTitle->SetText(LOCTEXT("Title", "Feed")); break;
		}
	}
	if (PageSubtitle)
	{
		FText Sub;
		if (Source == EGFCFeedSource::Mine && bIn)
		{
			Sub = FText::FromString(TEXT("@") + C->GetUsername());
		}
		else if (Source == EGFCFeedSource::User)
		{
			const FString Handle = TEXT("@") + (UserProfile.Username.IsEmpty() ? Query : UserProfile.Username);
			Sub = FText::FromString(UserProfile.Bio.IsEmpty() ? Handle : Handle + TEXT(" · ") + UserProfile.Bio);
		}
		PageSubtitle->SetText(Sub);
		PageSubtitle->SetVisibility(PageSubtitle->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UGFCFeedScreen::HandleComposerChanged(const FText& NewText)
{
	const FString Body = NewText.ToString();
	if (Body.Len() > UGFCCreateScreen::MaxBodyChars && ComposerBox)
	{
		ComposerBox->SetText(FText::FromString(Body.Left(UGFCCreateScreen::MaxBodyChars)));
	}
	if (ComposerPostButton) ComposerPostButton->SetIsEnabled(!Body.TrimStartAndEnd().IsEmpty());
}

void UGFCFeedScreen::HandleComposerMedia() { Compose(uint8(EGFCCaptureSource::Gallery), false); }
void UGFCFeedScreen::HandleComposerFile() { Compose(uint8(EGFCCaptureSource::File), false); }
void UGFCFeedScreen::HandleComposerPost() { Compose(TOptional<uint8>(), true); }

void UGFCFeedScreen::Compose(TOptional<uint8> Pick, bool bPostNow)
{
	if (!Root.IsValid() || !ComposerBox)
	{
		return;
	}
	const FString Body = ComposerBox->GetText().ToString().TrimStartAndEnd();
	if (bPostNow && Body.IsEmpty())
	{
		return;
	}
	TOptional<EGFCCaptureSource> PickSource;
	if (Pick.IsSet()) PickSource = EGFCCaptureSource(Pick.GetValue());
	// The create screen does the rest (picker, preview, upload progress); the text goes along.
	Root->ComposeFromFeed(Body, PickSource, bPostNow);
	ComposerBox->SetText(FText::GetEmpty());
}

void UGFCFeedScreen::HandleSearchCommitted(const FText& InText, ETextCommit::Type Commit)
{
	if (Commit == ETextCommit::OnEnter)
	{
		SetSource(EGFCFeedSource::Explore, InText.ToString());
		Refresh();
	}
}

void UGFCFeedScreen::OnScreenShown()
{
	UpdateHeader(); // the composer follows the login
	Refresh();
	Super::OnScreenShown();
}

void UGFCFeedScreen::OnScreenHidden()
{
	// Leaving the feed: every card lets go of its content.
	if (SeedList)
	{
		for (UWidget* W : SeedList->GetAllChildren())
		{
			if (UGFCSeedCard* Card = Cast<UGFCSeedCard>(W))
			{
				Card->ReleaseContent();
			}
		}
	}
	OffscreenSeconds.Reset();
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Cache->SetVisibleThumbnails({});
	}
}

void UGFCFeedScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateCardVisibility(InDeltaTime);
}

void UGFCFeedScreen::UpdateCardVisibility(float DeltaTime)
{
	if (!SeedList || !IsVisible())
	{
		return;
	}
	const FGeometry& ListGeo = SeedList->GetCachedGeometry();
	const FVector2D ListTop = ListGeo.GetAbsolutePosition();
	const FVector2D ListSize = ListGeo.GetAbsoluteSize();
	if (ListSize.Y <= 0.0)
	{
		return; // not laid out yet
	}
	// A margin of a quarter screen above and below: cards about to scroll in are ready.
	const double Margin = ListSize.Y * 0.25;
	const double Top = ListTop.Y - Margin, Bottom = ListTop.Y + ListSize.Y + Margin;
	const float Grace = GetDefault<UGFCApiSettings>()->OffscreenGraceSeconds;

	TSet<FString> VisibleThumbs;
	for (UWidget* W : SeedList->GetAllChildren())
	{
		UGFCSeedCard* Card = Cast<UGFCSeedCard>(W);
		if (!Card)
		{
			continue;
		}
		const FGeometry& G = Card->GetCachedGeometry();
		const double CardTop = G.GetAbsolutePosition().Y, CardBottom = CardTop + G.GetAbsoluteSize().Y;
		const bool bOnScreen = G.GetAbsoluteSize().Y > 0.0 && CardBottom > Top && CardTop < Bottom;
		if (bOnScreen)
		{
			OffscreenSeconds.Remove(Card);
			Card->RestoreContent();
			VisibleThumbs.Add(Card->ThumbUrl());
		}
		else if (Card->IsContentLoaded())
		{
			float& Secs = OffscreenSeconds.FindOrAdd(Card);
			Secs += DeltaTime;
			if (Secs >= Grace)
			{
				Card->ReleaseContent(); // scrolled away: deleted, re-fetched if the user comes back
				OffscreenSeconds.Remove(Card);
			}
		}
	}
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Cache->SetVisibleThumbnails(VisibleThumbs);
	}
}

void UGFCFeedScreen::HandleRefresh()
{
	Refresh();
}

void UGFCFeedScreen::Refresh()
{
	UGFCApiClient* C = Client();
	if (!C || bLoading)
	{
		return;
	}
	if (!C->IsLoggedIn() && (Source == EGFCFeedSource::Bookmarks || Source == EGFCFeedSource::Mine))
	{
		Populate({});
		if (EmptyText) EmptyText->SetText(LOCTEXT("NeedLogin", "Log in to see this page."));
		return;
	}
	bLoading = true;
	if (EmptyText)
	{
		EmptyText->SetText(Source == EGFCFeedSource::User
			? FText::Format(LOCTEXT("LoadingUser", "Loading @{0}'s posts…"), FText::FromString(Query))
			: LOCTEXT("Loading", "Loading the feed…"));
	}
	TWeakObjectPtr<UGFCFeedScreen> WeakThis(this);
	const uint32 Serial = LoadSerial;
	const EGFCFeedSource For = Source;
	auto Done = [WeakThis, Serial, For](const FGFCApiResult& R, const TArray<FGFCSeed>& Seeds)
	{
		if (WeakThis.IsValid() && WeakThis->LoadSerial == Serial)
		{
			WeakThis->OnLoaded(R, Seeds, For);
		}
	};
	switch (Source)
	{
	case EGFCFeedSource::Explore:
		if (Query.IsEmpty()) C->GetFeed(Done); // no query: the most liked posts
		else C->SearchSeeds(Query, Done);
		break;
	case EGFCFeedSource::Bookmarks: C->GetBookmarks(Done); break;
	case EGFCFeedSource::Mine: C->GetMySeeds(Done); break;
	case EGFCFeedSource::User: C->GetUserSeeds(Query, Done); break;
	default: C->GetFeed(Done); break;
	}
}

void UGFCFeedScreen::OnLoaded(const FGFCApiResult& R, const TArray<FGFCSeed>& Seeds, EGFCFeedSource For)
{
	bLoading = false;
	if (!R.bOk)
	{
		if (EmptyText) EmptyText->SetText(LOCTEXT("Failed", "Couldn't load the feed."));
		ShowStatus(FText::FromString(R.Error), true);
		return;
	}
	TArray<FGFCSeed> List = Seeds;
	if (const UGFCApiClient* Api = Client())
	{
		List.RemoveAll([Api](const FGFCSeed& S) { return Api->IsHiddenForMe(S); });
	}
	if (For == EGFCFeedSource::ForYou)
	{
		if (OnForYouLoaded) OnForYouLoaded(List);
		// No creator takes over the feed: at most two posts in a row from the same person.
		FGFCFeedMixer::MixCreators(List, 2);
	}
	else if (For == EGFCFeedSource::Explore && Query.IsEmpty())
	{
		List.StableSort([](const FGFCSeed& A, const FGFCSeed& B) { return A.Likes != B.Likes ? A.Likes > B.Likes : A.Views > B.Views; });
		if (List.Num() > 50) List.SetNum(50);
	}
	Populate(List);
	if (!EmptyText)
	{
		return;
	}
	if (For == EGFCFeedSource::Explore)
	{
		EmptyText->SetText(Query.IsEmpty()
			? LOCTEXT("TopPosts", "Top posts on this node. Search by title, text, tag or creator.")
			: (List.Num() == 0 ? FText::Format(LOCTEXT("NoResults", "No results for \"{0}\"."), FText::FromString(Query))
				: FText::Format(LOCTEXT("Results", "{0} results for \"{1}\""), FText::AsNumber(List.Num()), FText::FromString(Query))));
	}
	else if (List.Num() == 0 && For == EGFCFeedSource::Bookmarks)
	{
		EmptyText->SetText(LOCTEXT("NoBookmarks", "Nothing bookmarked yet. Use the star on a post to keep it here."));
	}
	else if (List.Num() == 0 && For == EGFCFeedSource::Mine)
	{
		EmptyText->SetText(LOCTEXT("NoPosts", "You haven't posted yet."));
	}
	else if (For == EGFCFeedSource::User)
	{
		EmptyText->SetText(List.Num() == 0 ? FText::Format(LOCTEXT("NoUserPosts", "@{0} has no posts to show."), FText::FromString(Query))
			: FText::Format(LOCTEXT("UserPosts", "{0} posts"), FText::AsNumber(List.Num())));
	}
}

void UGFCFeedScreen::RemoveHidden()
{
	const UGFCApiClient* Api = Client();
	if (!SeedList || !Api)
	{
		return;
	}
	for (UWidget* W : SeedList->GetAllChildren())
	{
		UGFCSeedCard* Card = Cast<UGFCSeedCard>(W);
		if (Card && Api->IsHiddenForMe(Card->GetSeed()))
		{
			Card->ReleaseContent();
			Card->RemoveFromParent();
		}
	}
}

void UGFCFeedScreen::ApplyEdit(const FGFCSeed& Updated)
{
	if (!SeedList)
	{
		return;
	}
	for (UWidget* W : SeedList->GetAllChildren())
	{
		UGFCSeedCard* Card = Cast<UGFCSeedCard>(W);
		if (Card && Card->GetSeed().Id == Updated.Id)
		{
			// Only the editable fields: the card keeps its own like/bookmark state and counts.
			FGFCSeed S = Card->GetSeed();
			S.Title = Updated.Title;
			S.Description = Updated.Description;
			S.Tags = Updated.Tags;
			S.EditedAt = Updated.EditedAt;
			Card->SetSeed(S);
		}
	}
}

void UGFCFeedScreen::Populate(const TArray<FGFCSeed>& Seeds)
{
	if (EmptyText)
	{
		EmptyText->SetText(Seeds.Num() == 0
			? LOCTEXT("Empty", "Nothing here yet. Seeds appear as creators upload them.")
			: FText::Format(LOCTEXT("Count", "{0} seeds"), FText::AsNumber(Seeds.Num())));
	}
	if (!SeedList)
	{
		return;
	}
	for (UWidget* W : SeedList->GetAllChildren())
	{
		if (UGFCSeedCard* Old = Cast<UGFCSeedCard>(W))
		{
			Old->ReleaseContent();
		}
	}
	SeedList->ClearChildren();
	OffscreenSeconds.Reset();
	TWeakObjectPtr<UGFCFeedScreen> WeakThis(this);
	for (const FGFCSeed& Seed : Seeds)
	{
		UGFCSeedCard* Card = CreateWidget<UGFCSeedCard>(this, SeedCardClass);
		if (!Card)
		{
			continue;
		}
		Card->OnSelected.BindLambda([WeakThis](const FGFCSeed& Selected)
		{
			if (WeakThis.IsValid() && WeakThis->Root.IsValid())
			{
				WeakThis->Root->ShowSeed(Selected);
			}
		});
		Card->SetWideLayout(bWide);
		SeedList->AddChild(Card);
		SeedList->AddChild(MakeDivider(WidgetTree));
		Card->OnMore.BindLambda([WeakThis](const FGFCSeed& Selected)
		{
			if (!WeakThis.IsValid()) return;
			const FString Id = Selected.Id;
			GFCShowPostActions(WeakThis.Get(), Selected, [WeakThis, Id]()
			{
				if (WeakThis.IsValid() && WeakThis->Root.IsValid()) WeakThis->Root->OnPostHidden(Id);
			});
		});
		Card->SetSeed(Seed);
	}
}

#undef LOCTEXT_NAMESPACE
