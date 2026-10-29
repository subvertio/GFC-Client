#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "Types/SlateEnums.h"
#include "GFCDesktopRails.generated.h"

class UButton;
class UEditableTextBox;
class UGFCRootWidget;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

/** The left rail's destinations. */
UENUM(BlueprintType)
enum class EGFCNavItem : uint8
{
	None,
	Home,
	Explore,
	Bookmarks,
	Wallet,
	Profile,
};

/** One clickable row in a rail (dynamic delegates can't carry the row's index). */
UCLASS()
class UGFCRailLink : public UObject
{
	GENERATED_BODY()
public:
	int32 Index = -1;
	TFunction<void(int32)> OnClick;
	UFUNCTION() void Clicked() { if (OnClick) OnClick(Index); }
};

/**
 * Desktop left rail: the GFC logo, the destinations (icon and label), a big Post button,
 * and the account at the bottom (its menu logs out). Logged out, it offers Log in instead.
 * See docs/ui-spec.md#desktop-layout.
 */
UCLASS()
class GFCCLIENT_API UGFCLeftRail : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRoot(UGFCRootWidget* InRoot);
	/** Bold label for the page on screen (None for pages that aren't in the rail). */
	void SetActive(EGFCNavItem Item);
	/** Follows the login: which items show, and the account at the bottom. */
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;

private:
	void Build();
	UFUNCTION() void HandlePost();
	UFUNCTION() void HandleLogin();
	UFUNCTION() void HandleAccount();

	struct FItem
	{
		EGFCNavItem Item = EGFCNavItem::None;
		UButton* Button = nullptr;
		UTextBlock* Glyph = nullptr;
		UTextBlock* Label = nullptr;
		bool bNeedsLogin = false;
	};
	TArray<FItem> Items;
	EGFCNavItem Active = EGFCNavItem::None;

	TWeakObjectPtr<UGFCRootWidget> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UGFCRailLink>> Links;
	UPROPERTY(Transient) TObjectPtr<UButton> PostButton;
	UPROPERTY(Transient) TObjectPtr<UButton> LoginButton;
	UPROPERTY(Transient) TObjectPtr<UButton> AccountButton;
	UPROPERTY(Transient) TObjectPtr<USizeBox> AccountAvatar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AccountName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AccountHandle;
};

/**
 * Desktop right rail, all from the node: a search box (Explore), the user's GFC balance,
 * trending posts (the For you feed ranked by likes, views and GFC earned) and the network's
 * top earners (GET /podt/stats). See docs/ui-spec.md#desktop-layout.
 */
UCLASS()
class GFCCLIENT_API UGFCRightRail : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRoot(UGFCRootWidget* InRoot);
	/** Reloads the balance and the top earners. */
	void Refresh();
	/** Ranks the For you feed into the trending card. */
	void SetTrending(const TArray<FGFCSeed>& Feed);

	/** The trending order: likes, then GFC earned, then views (ties keep the feed's order). At most Max. */
	static TArray<FGFCSeed> RankTrending(const TArray<FGFCSeed>& Feed, int32 Max = 5);
	/** Top earners: "@owner" when the node confirmed the account, else the shortened wallet ID. */
	static FString EarnerName(const FGFCTopEarner& Earner);

protected:
	virtual void NativeOnInitialized() override;

private:
	void Build();
	void RefreshBalance();
	void RefreshEarners();
	UFUNCTION() void HandleSearch(const FText& InText, ETextCommit::Type Commit);
	UFUNCTION() void HandleOpenWallet();
	UWidget* MakeCardTitle(const FText& Title);

	TWeakObjectPtr<UGFCRootWidget> Root;
	TArray<FGFCSeed> Trending;
	uint32 BalanceSerial = 0;

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> SearchBox;
	UPROPERTY(Transient) TObjectPtr<UWidget> WalletCard;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BalanceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BalanceNote;
	UPROPERTY(Transient) TObjectPtr<UButton> WalletButton;
	UPROPERTY(Transient) TObjectPtr<UWidget> TrendingCard;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TrendingList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EarnersNote;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EarnersList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NodeText;
	UPROPERTY(Transient) TArray<TObjectPtr<UGFCRailLink>> TrendingLinks;
};
