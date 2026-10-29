#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCPostEditor.generated.h"

class UButton;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UTextBlock;
class UVerticalBox;

/**
 * A card over the dimmed screen for the poster's own post: edit its title, text and tags
 * (media can't change), or read its edit history. Tapping outside, Cancel/Close, or the
 * Android Back key closes it. One at a time.
 */
UCLASS()
class GFCCLIENT_API UGFCPostPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	using FOnSaved = TFunction<void(const FGFCSeed& Updated)>;

	/** The editor, prefilled from Seed. OnSaved gets the post as the node saved it. */
	static UGFCPostPanel* ShowEditor(UObject* WorldContext, const FGFCSeed& Seed, FOnSaved OnSaved);
	/** Every version of the post's title, text and tags, newest first (GET /seeds/{id}/edits). */
	static UGFCPostPanel* ShowHistory(UObject* WorldContext, const FGFCSeed& Seed);
	/** Closes the open panel. Returns false when none is open (for the Back key). */
	static bool CloseOpen();

	/**
	 * What to send for the editor's fields: only what changed. A text post's title follows its first
	 * line (UGFCCreateScreen::TitleFromBody) unless the poster typed a title of their own; an emptied
	 * title on a media post is left as it was. False when nothing changed.
	 */
	static bool BuildRequest(const FGFCSeed& Seed, const FString& Title, const FString& Body, const FString& TagsText,
		FGFCSeedEditRequest& Out);
	/** The post with the node's answer to an edit applied. */
	static FGFCSeed ApplyEdit(const FGFCSeed& Seed, const FGFCSeedEdit& Edit);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	static UGFCPostPanel* Open(UObject* WorldContext, const FGFCSeed& Seed);
	void BuildEditor();
	void BuildHistory();
	void ShowEdits(const TArray<FGFCSeedEdit>& Edits);
	void SetStatus(const FText& Message, bool bError);
	UFUNCTION() void HandleSave();
	UFUNCTION() void HandleClose();

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> List;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Versions;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> TitleBox;
	UPROPERTY(Transient) TObjectPtr<UMultiLineEditableTextBox> BodyBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> TagsBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> SaveButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SaveLabel;
	FGFCSeed Seed;
	FOnSaved OnSaved;
	bool bSaving = false;
};
