#include "UI/GFCCreateScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Containers/Ticker.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "GFCApiSettings.h"
#include "GFCModelParser.h"
#include "Viewer/GFCModelView.h"
#include "Viewer/GFCModelViewer.h"
#include "Engine/Texture2D.h"
#include "GFCApiClient.h"
#include "GFCCacheSubsystem.h"
#include "GFCUpload.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "GFCApiSubsystem.h"
#include "UI/GFCRewardBurst.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCCreate"

using namespace GFCUI;

namespace
{
	FText Size(int64 Bytes)
	{
		FNumberFormattingOptions O;
		O.MaximumFractionalDigits = 1;
		return Bytes < 1024 * 1024 ? FText::Format(LOCTEXT("KB", "{0} KB"), FText::AsNumber(FMath::Max<int64>(1, Bytes / 1024)))
			: FText::Format(LOCTEXT("MB", "{0} MB"), FText::AsNumber(Bytes / 1048576.0, &O));
	}
}

void UGFCCreateScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	T->RootWidget = Scroll;
	FormScroll = Scroll;
	UVerticalBox* Col = MakeColumn(T);
	Scroll->AddChild(Col);

	// Top to bottom, in the order you do it: write, add media (optional), title, post.
	// Header: the screen's name and a Post button that's always in reach, however long the form gets.
	UHorizontalBox* Header = MakeRow(T);
	AddH(Header, MakeText(T, LOCTEXT("Title", "New post"), 22, Text, true), 0.f, true);
	HeaderPostButton = MakeButton(T, LOCTEXT("PostTop", "Post"));
	AddH(Header, HeaderPostButton, 0.f);
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(HeaderPostButton->Slot)) S->SetVerticalAlignment(VAlign_Center);
	AddV(Col, Header, 12.f);

	BodyBox = MakeMultiLineInput(T, LOCTEXT("BodyHint", "What's on your mind?"));
	USizeBox* BodySize = T->ConstructWidget<USizeBox>();
	BodySize->SetMinDesiredHeight(120.f);
	BodySize->SetMaxDesiredHeight(260.f);
	BodySize->AddChild(BodyBox);
	AddV(Col, BodySize, 14.f);

	AddV(Col, MakeText(T, LOCTEXT("AddL", "Add to your post (optional). What you capture here is saved to your phone's gallery and stays yours."), 12, TextDim), 8.f);
	PhotoButton = MakeButton(T, LOCTEXT("Photo", "Take a photo"), PanelRaised);
	VideoButton = MakeButton(T, LOCTEXT("Video", "Record a video"), PanelRaised);
	GalleryButton = MakeButton(T, LOCTEXT("Gallery", "Choose from gallery"), PanelRaised);
	FileButton = MakeButton(T, LOCTEXT("File", "3D model or other file"), PanelRaised);
	AddV(Col, PhotoButton, 8.f);
	AddV(Col, VideoButton, 8.f);
	AddV(Col, GalleryButton, 8.f);
	AddV(Col, FileButton, 14.f);

	UBorder* Form = MakeCard(T, FMargin(16.f));
	UVerticalBox* F = MakeColumn(T);
	Form->AddChild(F);

	UVerticalBox* M = MakeColumn(T);

	// Preview: the image or video frame, or for a 3D model the live model on its turntable.
	PreviewSize = T->ConstructWidget<USizeBox>();
	PreviewSize->SetHeightOverride(200.f);
	PreviewSize->SetClipping(EWidgetClipping::ClipToBounds);
	USizeBox* PreviewBox = PreviewSize;
	UBorder* PreviewBg = MakePanel(T, Background, FMargin(0.f));
	UOverlay* PreviewOver = T->ConstructWidget<UOverlay>();
	UScaleBox* Fit = T->ConstructWidget<UScaleBox>();
	Fit->SetStretch(EStretch::ScaleToFit);
	PreviewImage = T->ConstructWidget<UImage>();
	Fit->AddChild(PreviewImage);
	PreviewModelView = CreateWidget<UGFCModelView>(this, UGFCModelView::StaticClass());
	PreviewModelView->SetVisibility(ESlateVisibility::Collapsed);
	for (UWidget* W : TArray<UWidget*>{Fit, PreviewModelView})
	{
		if (UOverlaySlot* S = PreviewOver->AddChildToOverlay(W))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}
	PreviewBg->AddChild(PreviewOver);
	PreviewBox->AddChild(PreviewBg);
	AddV(M, PreviewBox, 8.f);
	FileText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	AddV(M, FileText, 8.f);
	RemoveMediaButton = MakeButton(T, LOCTEXT("Remove", "Remove"), PanelRaised);
	AddV(M, RemoveMediaButton, 14.f);
	MediaPanel = M;
	AddV(F, M, 0.f);

	TitleBox = MakeInput(T, LOCTEXT("TitleHint", "Title (optional for text)"));
	AddV(F, MakeField(T, LOCTEXT("TitleL", "Title"), TitleBox), 14.f);

	PostButton = MakeButton(T, LOCTEXT("Post", "Post"));
	AddV(F, PostButton, 10.f);
	UploadProgress = T->ConstructWidget<UProgressBar>();
	UploadProgress->SetFillColorAndOpacity(Accent);
	UploadProgress->SetPercent(0.f);
	AddV(F, UploadProgress, 6.f);
	ProgressText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	AddV(F, ProgressText, 8.f);
	CancelButton = MakeButton(T, LOCTEXT("Cancel", "Cancel upload"), PanelRaised);
	AddV(F, CancelButton, 8.f);
	ResultText = MakeText(T, FText::GetEmpty(), 14, Text);
	AddV(F, ResultText, 8.f);
	ViewPostButton = MakeButton(T, LOCTEXT("ViewPost", "View your post"), PanelRaised);
	AddV(F, ViewPostButton, 0.f);

	AddV(Col, Form, 0.f);
}

void UGFCCreateScreen::BindEvents()
{
	if (PhotoButton) PhotoButton->OnClicked.AddDynamic(this, &ThisClass::HandlePhoto);
	if (VideoButton) VideoButton->OnClicked.AddDynamic(this, &ThisClass::HandleVideo);
	if (GalleryButton) GalleryButton->OnClicked.AddDynamic(this, &ThisClass::HandleGallery);
	if (FileButton) FileButton->OnClicked.AddDynamic(this, &ThisClass::HandleFile);
	if (RemoveMediaButton) RemoveMediaButton->OnClicked.AddDynamic(this, &ThisClass::HandleRemoveMedia);
	if (BodyBox) BodyBox->OnTextChanged.AddDynamic(this, &ThisClass::HandleBodyChanged);
	if (PostButton) PostButton->OnClicked.AddDynamic(this, &ThisClass::HandlePost);
	if (HeaderPostButton) HeaderPostButton->OnClicked.AddDynamic(this, &ThisClass::HandlePost);
	if (CancelButton) CancelButton->OnClicked.AddDynamic(this, &ThisClass::HandleCancel);
	if (ViewPostButton) ViewPostButton->OnClicked.AddDynamic(this, &ThisClass::HandleViewPost);
}

void UGFCCreateScreen::OnScreenShown()
{
	// The camera lives in the phone app; on PC only the file picker is offered.
	if (PhotoButton) PhotoButton->SetIsEnabled(FGFCMediaCapture::IsSupported(EGFCCaptureSource::Photo));
	if (VideoButton) VideoButton->SetIsEnabled(FGFCMediaCapture::IsSupported(EGFCCaptureSource::Video));
	if (FileButton) FileButton->SetIsEnabled(FGFCMediaCapture::IsSupported(EGFCCaptureSource::File));
	if (PreviewViewer) PreviewViewer->SetActorTickEnabled(true);
	if (!Job.IsValid())
	{
		ShowMediaPanel();
		SetUploading(false);
	}
	Super::OnScreenShown();
}

void UGFCCreateScreen::OnScreenHidden()
{
	// Off screen, the preview stops turning and rendering; it carries on when the screen comes back.
	if (PreviewViewer) PreviewViewer->SetActorTickEnabled(false);
	Super::OnScreenHidden();
}

void UGFCCreateScreen::NativeDestruct()
{
	if (Job.IsValid()) Job->Cancel();
	DiscardMedia();
	Super::NativeDestruct();
}

void UGFCCreateScreen::HandlePhoto() { Choose(EGFCCaptureSource::Photo); }
void UGFCCreateScreen::HandleVideo() { Choose(EGFCCaptureSource::Video); }
void UGFCCreateScreen::HandleGallery() { Choose(EGFCCaptureSource::Gallery); }
void UGFCCreateScreen::HandleFile() { Choose(EGFCCaptureSource::File); }

void UGFCCreateScreen::HandleRemoveMedia()
{
	if (Job.IsValid()) return;
	DiscardMedia();
	PreviewTexture = nullptr;
	ShowMediaPanel();
	UpdatePostEnabled();
}

void UGFCCreateScreen::HandleBodyChanged(const FText& NewText)
{
	if (NewText.ToString().Len() > MaxBodyChars && BodyBox)
	{
		BodyBox->SetText(FText::FromString(NewText.ToString().Left(MaxBodyChars)));
	}
	UpdatePostEnabled();
}

void UGFCCreateScreen::ShowMediaPanel()
{
	if (MediaPanel) MediaPanel->SetVisibility(Chosen.Path.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UGFCCreateScreen::UpdatePostEnabled()
{
	const bool bHasText = BodyBox && !BodyBox->GetText().ToString().TrimStartAndEnd().IsEmpty();
	const bool bTooBig = !Chosen.Path.IsEmpty() && Chosen.Size + 64 * 1024 > UGFCApiClient::MaxUploadBytes;
	const bool bCanPost = !bTooBig && (bHasText || !Chosen.Path.IsEmpty());
	if (PostButton) PostButton->SetIsEnabled(bCanPost);
	if (HeaderPostButton) HeaderPostButton->SetIsEnabled(bCanPost && !Job.IsValid());
}

FString UGFCCreateScreen::TitleFromBody(const FString& Body)
{
	FString Line = Body.TrimStartAndEnd();
	int32 Nl;
	if (Line.FindChar(TEXT('\n'), Nl)) Line.LeftInline(Nl);
	Line.TrimEndInline();
	if (Line.Len() > 80) Line = Line.Left(79).TrimEnd() + TEXT("\u2026");
	return Line;
}

FString UGFCCreateScreen::WriteTextSeed(const FString& Body)
{
	const FString Dir = FPaths::Combine(UGFCCacheSubsystem::GetCacheRoot(), TEXT("outbox"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = FPaths::Combine(Dir, FString::Printf(TEXT("text-%s.txt"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower()));
	return FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ? Path : FString();
}

void UGFCCreateScreen::ComposeFrom(const FString& Body, TOptional<EGFCCaptureSource> Pick, bool bPostNow)
{
	if (Job.IsValid())
	{
		ShowStatus(LOCTEXT("Busy", "Wait for the current upload to finish, or cancel it."), true);
		return;
	}
	if (BodyBox) BodyBox->SetText(FText::FromString(Body.Left(MaxBodyChars)));
	if (!Chosen.Path.IsEmpty())
	{
		HandleRemoveMedia();
	}
	UpdatePostEnabled();
	if (Pick.IsSet())
	{
		Choose(Pick.GetValue());
	}
	else if (bPostNow)
	{
		HandlePost();
	}
}

void UGFCCreateScreen::Choose(EGFCCaptureSource Source)
{
	if (Job.IsValid())
	{
		ShowStatus(LOCTEXT("Busy", "Wait for the current upload to finish, or cancel it."), true);
		return;
	}
	TWeakObjectPtr<UGFCCreateScreen> WeakThis(this);
	FGFCMediaCapture::Start(Source, [WeakThis](bool bOk, const FGFCCapturedMedia& Media, const FString& Error)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		if (bOk)
		{
			WeakThis->OnChosen(Media);
		}
		else if (Error != TEXT("cancelled"))
		{
			WeakThis->ShowStatus(FText::FromString(Error), true);
		}
	});
}

void UGFCCreateScreen::UseFile(const FString& Path)
{
	FGFCCapturedMedia Media;
	Media.Path = FPaths::ConvertRelativePathToFull(Path);
	Media.FileName = FPaths::GetCleanFilename(Path);
	Media.MimeType = FGFCMediaCapture::MimeForFile(Media.FileName);
	Media.Size = IFileManager::Get().FileSize(*Media.Path);
	if (Media.Size >= 0)
	{
		OnChosen(Media);
	}
}

void UGFCCreateScreen::OnChosen(const FGFCCapturedMedia& Media)
{
	DiscardMedia();
	Chosen = Media;
	ShowMediaPanel();
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	if (ViewPostButton) ViewPostButton->SetVisibility(ESlateVisibility::Collapsed);
	if (UploadProgress) UploadProgress->SetPercent(0.f);
	if (ProgressText) ProgressText->SetText(FText::GetEmpty());
	if (TitleBox && TitleBox->GetText().IsEmpty() && (!BodyBox || BodyBox->GetText().IsEmpty()))
	{
		TitleBox->SetText(FText::FromString(FPaths::GetBaseFilename(Media.FileName)));
	}

	const bool bTooBig = Media.Size + 64 * 1024 > UGFCApiClient::MaxUploadBytes;
	if (FileText)
	{
		FText Info = FText::Format(LOCTEXT("FileInfo", "{0} · {1}"), FText::FromString(Media.FileName), Size(Media.Size));
		if (Media.bSavedToGallery)
		{
			Info = FText::Format(LOCTEXT("Saved", "{0}\nSaved to your gallery."), Info);
		}
		if (bTooBig)
		{
			Info = FText::Format(LOCTEXT("TooBig", "{0}\nToo big: uploads are limited to 200 MB. Trim the video and try again."), Info);
		}
		FileText->SetText(Info);
		FileText->SetColorAndOpacity(FSlateColor(bTooBig ? Danger : TextDim));
	}
	UpdatePostEnabled();

	// Preview: the device's video thumbnail, or the image itself (downscaled).
	PreviewTexture = nullptr;
	TArray<uint8> Bytes;
	if (!Media.ThumbnailPath.IsEmpty())
	{
		FFileHelper::LoadFileToArray(Bytes, *Media.ThumbnailPath);
	}
	else if (Media.MimeType.StartsWith(TEXT("image/")))
	{
		Bytes = FGFCMediaCapture::MakeImageThumbnail(Media.Path, 512);
	}
	PreviewTexture = Bytes.Num() ? UGFCCacheSubsystem::DecodeTexture(Bytes, 512) : nullptr;
	if (PreviewImage)
	{
		if (PreviewTexture)
		{
			PreviewImage->SetBrushFromTexture(PreviewTexture, true);
			PreviewImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			PreviewImage->SetColorAndOpacity(FLinearColor::Transparent);
		}
	}
	if (Media.MimeType.StartsWith(TEXT("model/")))
	{
		StartModelPreview(Media.Path);
	}
	SetUploading(false);
	// Next step is the title and Post: bring them into view under the preview.
	if (FormScroll && PostButton) FormScroll->ScrollWidgetIntoView(PostButton, true, EDescendantScrollDestination::BottomOrRight);
}

void UGFCCreateScreen::StartModelPreview(const FString& Path)
{
	// The same parse and budgets as the seed screen, off the game thread: what you see here is what phones will show.
	if (PreviewSize) PreviewSize->SetHeightOverride(280.f);
	if (FileText) FileText->SetText(FText::Format(LOCTEXT("Building", "{0}\nBuilding the 3D preview…"), FileText->GetText()));
	const uint32 Serial = ++PreviewSerial;
	const FGFCModelBudget Budget = FGFCModelBudget::FromSettings();
	TWeakObjectPtr<UGFCCreateScreen> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, Serial, Path, Budget]()
	{
		TArray<uint8> Bytes;
		TSharedPtr<FGFCModelData> Model = MakeShared<FGFCModelData>();
		FString Error;
		const bool bOk = FFileHelper::LoadFileToArray(Bytes, *Path) && FGFCModelParser::Parse(Bytes, Budget, *Model, Error);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Serial, Model, bOk, Error]()
		{
			if (WeakThis.IsValid() && WeakThis->PreviewSerial == Serial)
			{
				WeakThis->OnModelParsed(Model, bOk, Error);
			}
		});
	});
}

void UGFCCreateScreen::OnModelParsed(TSharedPtr<FGFCModelData> Model, bool bOk, const FString& Error)
{
	const FText Base = FText::Format(LOCTEXT("FileInfo", "{0} · {1}"), FText::FromString(Chosen.FileName), Size(Chosen.Size));
	UWorld* World = GetWorld();
	if (!bOk || !World)
	{
		// Still postable (the original is kept), but phones won't be able to show it.
		if (FileText)
		{
			FileText->SetText(FText::Format(LOCTEXT("ModelBad", "{0}\nThe app's 3D viewer can't show this model: {1}"), Base,
				FText::FromString(Error.IsEmpty() ? FString(TEXT("the file couldn't be read.")) : Error)));
			FileText->SetColorAndOpacity(FSlateColor(Warning));
		}
		if (PreviewSize) PreviewSize->SetHeightOverride(200.f);
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	PreviewViewer = World->SpawnActor<AGFCModelViewer>(AGFCModelViewer::StaticClass(), FTransform::Identity, Params);
	if (!PreviewViewer)
	{
		return;
	}
	PreviewViewer->ShowModel(*Model, GetDefault<UGFCApiSettings>()->MaxTextureSize, 768);
	if (PreviewModelView)
	{
		PreviewModelView->SetViewer(PreviewViewer);
		PreviewModelView->SetVisibility(ESlateVisibility::Visible);
	}
	if (PreviewImage) PreviewImage->SetColorAndOpacity(FLinearColor::Transparent);
	if (FileText)
	{
		FNumberFormattingOptions O;
		O.UseGrouping = true;
		FText Info = FText::Format(LOCTEXT("ModelInfo", "{0}\n{1} triangles{2}. Drag to turn it; the thumbnail is the view when you post."),
			Base, FText::AsNumber(Model->Triangles, &O),
			Model->IsAnimated() ? FText::Format(LOCTEXT("Anim", ", animated ({0})"), FText::FromString(Model->Animations[0].Name)) : FText::GetEmpty());
		FileText->SetText(Info);
		FileText->SetColorAndOpacity(FSlateColor(TextDim));
	}
}

void UGFCCreateScreen::ReleaseModelPreview()
{
	++PreviewSerial; // a parse still running is ignored
	if (PreviewModelView)
	{
		PreviewModelView->SetViewer(nullptr);
		PreviewModelView->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PreviewViewer)
	{
		PreviewViewer->Release();
		PreviewViewer = nullptr;
	}
	if (PreviewSize) PreviewSize->SetHeightOverride(200.f);
}

void UGFCCreateScreen::SetUploading(bool bUploading)
{
	if (PostButton) PostButton->SetVisibility(bUploading ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (CancelButton) CancelButton->SetVisibility(bUploading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UploadProgress) UploadProgress->SetVisibility(bUploading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const TPair<UButton*, EGFCCaptureSource> Sources[] = {{PhotoButton.Get(), EGFCCaptureSource::Photo},
		{VideoButton.Get(), EGFCCaptureSource::Video}, {GalleryButton.Get(), EGFCCaptureSource::Gallery}, {FileButton.Get(), EGFCCaptureSource::File}};
	for (const auto& S : Sources)
	{
		if (S.Key) S.Key->SetIsEnabled(!bUploading && FGFCMediaCapture::IsSupported(S.Value));
	}
	if (RemoveMediaButton) RemoveMediaButton->SetIsEnabled(!bUploading);
	if (HeaderPostButton && bUploading) HeaderPostButton->SetIsEnabled(false);
	if (BodyBox) BodyBox->SetIsReadOnly(bUploading);
	if (!bUploading) UpdatePostEnabled();
}

void UGFCCreateScreen::HandlePost()
{
	UGFCApiClient* C = Client();
	const FString Body = BodyBox ? BodyBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (!C || Job.IsValid() || (Chosen.Path.IsEmpty() && Body.IsEmpty()))
	{
		return;
	}
	if (Chosen.Path.IsEmpty())
	{
		// Just text: the post is a small .txt seed; the text itself travels as the description.
		FGFCCapturedMedia TextSeed;
		TextSeed.Path = WriteTextSeed(Body);
		if (TextSeed.Path.IsEmpty())
		{
			ShowStatus(LOCTEXT("TextWriteFailed", "Couldn't prepare the post. Check the phone has free space."), true);
			return;
		}
		TextSeed.FileName = TEXT("post.txt");
		TextSeed.MimeType = TEXT("text/plain");
		TextSeed.Size = IFileManager::Get().FileSize(*TextSeed.Path);
		TextSeed.bIsAppCopy = true;
		Chosen = TextSeed;
		bTextFile = true;
	}
	FGFCUploadRequest Req;
	Req.FilePath = Chosen.Path;
	Req.FileName = Chosen.FileName;
	Req.MimeType = Chosen.MimeType;
	Req.Title = TitleBox ? TitleBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (Req.Title.IsEmpty() && bTextFile)
	{
		Req.Title = TitleFromBody(Body);
	}
	Req.Description = Body;
	if (!Chosen.ThumbnailPath.IsEmpty())
	{
		FFileHelper::LoadFileToArray(Req.ThumbnailJpeg, *Chosen.ThumbnailPath);
	}
	else if (Chosen.MimeType.StartsWith(TEXT("image/")))
	{
		Req.ThumbnailJpeg = FGFCMediaCapture::MakeImageThumbnail(Chosen.Path, 512);
	}
	else if (PreviewViewer)
	{
		// A 3D post's thumbnail is the preview as the user left it.
		Req.ThumbnailJpeg = PreviewViewer->SnapshotJpeg();
	}

	SetUploading(true);
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	TWeakObjectPtr<UGFCCreateScreen> WeakThis(this);
	Job = C->Upload(Req,
		[WeakThis](int64 Sent, int64 Total)
		{
			if (!WeakThis.IsValid() || Total <= 0) return;
			const float P = float(double(Sent) / double(Total));
			if (WeakThis->UploadProgress) WeakThis->UploadProgress->SetPercent(P);
			if (WeakThis->ProgressText)
			{
				WeakThis->ProgressText->SetText(FText::Format(LOCTEXT("Progress", "Uploading {0} of {1}"), Size(Sent), Size(Total)));
			}
		},
		[WeakThis](const FGFCApiResult& R, const FGFCUploadResult& Result)
		{
			if (!WeakThis.IsValid()) return;
			const int32 Attempts = WeakThis->Job.IsValid() ? WeakThis->Job->GetAttempt() : 1;
			WeakThis->Job.Reset();
			if (!R.bOk && WeakThis->bTextFile)
			{
				WeakThis->DiscardMedia(); // a retry writes a fresh text seed from the box
				}
			WeakThis->SetUploading(false);
			if (!R.bOk)
			{
				if (WeakThis->ResultText)
				{
					WeakThis->ResultText->SetText(FText::FromString(R.Error));
					WeakThis->ResultText->SetColorAndOpacity(FSlateColor(R.Error.Contains(TEXT("cancelled")) ? TextDim : Danger));
				}
				// The real network wants a verified phone first: offer it right here (the post stays written).
				if (R.StatusCode == 403 && R.Error.Contains(TEXT("phone"), ESearchCase::IgnoreCase) && WeakThis->Root.IsValid())
				{
					WeakThis->Root->OfferPhoneVerification(FText::FromString(R.Error));
				}
				return;
			}
			WeakThis->Posted = Result.Seed;
			FText Message = Attempts > 1
				? FText::Format(LOCTEXT("PostedRetry", "Posted (after {0} tries)."), FText::AsNumber(Attempts))
				: LOCTEXT("Posted", "Posted!");
			if (Result.RepostOf.IsSet())
			{
				Message = FText::Format(LOCTEXT("Repost", "{0}\nThis looks like a repost of @{1}. They'll get the credit."),
					Message, FText::FromString(Result.RepostOf.Owner));
			}
			else if (Result.FirstPostBonus > 0.0)
			{
				Message = FText::Format(LOCTEXT("Bonus", "{0}\nYou earned {1} GFC for your first post!"), Message, FText::AsNumber(Result.FirstPostBonus));
				UGFCRewardBurst::Play(WeakThis.Get(), Result.FirstPostBonus, LOCTEXT("BonusCaption", "First post bonus!"));
				// The bonus is celebrated now: count the new balance as seen so login doesn't repeat it.
				WeakThis->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->CheckEarnings([](int64) {});
			}
			else if (Result.bFirstPostBonusPending)
			{
				Message = FText::Format(LOCTEXT("BonusPending",
					"{0}\nYour first post bonus is saved. Verify your phone and have a wallet (both in Wallet) to collect it."), Message);
			}
			else if (!Result.bRewardsEnabled)
			{
				Message = FText::Format(LOCTEXT("NoRewards", "{0}\nCreate a wallet so your posts can earn GFC."), Message);
			}
			if (WeakThis->ResultText)
			{
				WeakThis->ResultText->SetText(Message);
				WeakThis->ResultText->SetColorAndOpacity(FSlateColor(Result.RepostOf.IsSet() ? Warning : Success));
			}
			if (WeakThis->ViewPostButton) WeakThis->ViewPostButton->SetVisibility(ESlateVisibility::Visible);
			if (WeakThis->PostButton) WeakThis->PostButton->SetVisibility(ESlateVisibility::Collapsed);
			WeakThis->DiscardMedia(); // the app's copy goes; the gallery copy stays the user's
			WeakThis->ShowMediaPanel();
				if (WeakThis->BodyBox) WeakThis->BodyBox->SetText(FText::GetEmpty());
				if (WeakThis->TitleBox) WeakThis->TitleBox->SetText(FText::GetEmpty());
		});
}

void UGFCCreateScreen::HandleCancel()
{
	if (Job.IsValid())
	{
		Job->Cancel();
	}
}

void UGFCCreateScreen::HandleViewPost()
{
	if (Root.IsValid() && !Posted.Id.IsEmpty())
	{
		Root->ShowSeed(Posted);
	}
}

void UGFCCreateScreen::DiscardMedia()
{
	ReleaseModelPreview();
	// Only temporary copies the app made are deleted; files the user picked on PC are theirs.
	if (Chosen.bIsAppCopy)
	{
		// A moment later: the finished HTTP request may still hold the file open until it's destroyed.
		const FString Path = Chosen.Path, Thumb = Chosen.ThumbnailPath;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Path, Thumb](float)
		{
			IFileManager::Get().Delete(*Path, false, true, true);
			if (!Thumb.IsEmpty())
			{
				IFileManager::Get().Delete(*Thumb, false, true, true);
			}
			return false;
		}), 1.f);
	}
	Chosen = FGFCCapturedMedia();
	bTextFile = false;
}

#undef LOCTEXT_NAMESPACE
