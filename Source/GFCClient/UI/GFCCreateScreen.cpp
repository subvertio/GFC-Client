#include "UI/GFCCreateScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Containers/Ticker.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "GFCApiClient.h"
#include "GFCCacheSubsystem.h"
#include "GFCUpload.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
	UVerticalBox* Col = MakeColumn(T);
	Scroll->AddChild(Col);

	AddV(Col, MakeText(T, LOCTEXT("Title", "Post"), 22, Text, true), 4.f);
	AddV(Col, MakeText(T, LOCTEXT("Sub", "Share a photo, a video, audio or a 3D model. What you capture here is saved to your phone's gallery and stays yours."), 13, TextDim), 14.f);

	PhotoButton = MakeButton(T, LOCTEXT("Photo", "Take a photo"), PanelRaised);
	VideoButton = MakeButton(T, LOCTEXT("Video", "Record a video"), PanelRaised);
	GalleryButton = MakeButton(T, LOCTEXT("Gallery", "Choose from gallery"), PanelRaised);
	AddV(Col, PhotoButton, 8.f);
	AddV(Col, VideoButton, 8.f);
	AddV(Col, GalleryButton, 18.f);

	UBorder* Form = MakeCard(T, FMargin(16.f));
	UVerticalBox* F = MakeColumn(T);
	Form->AddChild(F);

	USizeBox* PreviewBox = T->ConstructWidget<USizeBox>();
	PreviewBox->SetHeightOverride(200.f);
	PreviewBox->SetClipping(EWidgetClipping::ClipToBounds);
	UBorder* PreviewBg = MakePanel(T, Background, FMargin(0.f));
	UScaleBox* Fit = T->ConstructWidget<UScaleBox>();
	Fit->SetStretch(EStretch::ScaleToFit);
	PreviewImage = T->ConstructWidget<UImage>();
	Fit->AddChild(PreviewImage);
	PreviewBg->AddChild(Fit);
	PreviewBox->AddChild(PreviewBg);
	AddV(F, PreviewBox, 8.f);
	FileText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	AddV(F, FileText, 12.f);

	TitleBox = MakeInput(T, LOCTEXT("TitleHint", "Give it a title"));
	DescriptionBox = MakeInput(T, LOCTEXT("DescHint", "Say something about it (optional)"));
	AddV(F, MakeField(T, LOCTEXT("TitleL", "Title"), TitleBox), 10.f);
	AddV(F, MakeField(T, LOCTEXT("DescL", "Description"), DescriptionBox), 14.f);

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

	FormPanel = Form;
	AddV(Col, Form, 0.f);
}

void UGFCCreateScreen::BindEvents()
{
	if (PhotoButton) PhotoButton->OnClicked.AddDynamic(this, &ThisClass::HandlePhoto);
	if (VideoButton) VideoButton->OnClicked.AddDynamic(this, &ThisClass::HandleVideo);
	if (GalleryButton) GalleryButton->OnClicked.AddDynamic(this, &ThisClass::HandleGallery);
	if (PostButton) PostButton->OnClicked.AddDynamic(this, &ThisClass::HandlePost);
	if (CancelButton) CancelButton->OnClicked.AddDynamic(this, &ThisClass::HandleCancel);
	if (ViewPostButton) ViewPostButton->OnClicked.AddDynamic(this, &ThisClass::HandleViewPost);
}

void UGFCCreateScreen::OnScreenShown()
{
	// The camera lives in the phone app; on PC only the file picker is offered.
	if (PhotoButton) PhotoButton->SetIsEnabled(FGFCMediaCapture::IsSupported(EGFCCaptureSource::Photo));
	if (VideoButton) VideoButton->SetIsEnabled(FGFCMediaCapture::IsSupported(EGFCCaptureSource::Video));
	if (!Job.IsValid() && Chosen.Path.IsEmpty())
	{
		if (FormPanel) FormPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	Super::OnScreenShown();
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
	if (FormPanel) FormPanel->SetVisibility(ESlateVisibility::Visible);
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	if (ViewPostButton) ViewPostButton->SetVisibility(ESlateVisibility::Collapsed);
	if (UploadProgress) UploadProgress->SetPercent(0.f);
	if (ProgressText) ProgressText->SetText(FText::GetEmpty());
	if (TitleBox && TitleBox->GetText().IsEmpty())
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
	if (PostButton) PostButton->SetIsEnabled(!bTooBig);

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
	SetUploading(false);
}

void UGFCCreateScreen::SetUploading(bool bUploading)
{
	if (PostButton) PostButton->SetVisibility(bUploading ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (CancelButton) CancelButton->SetVisibility(bUploading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UploadProgress) UploadProgress->SetVisibility(bUploading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (UButton* B : {PhotoButton.Get(), VideoButton.Get(), GalleryButton.Get()})
	{
		if (B) B->SetIsEnabled(!bUploading && (B == GalleryButton.Get() || FGFCMediaCapture::IsSupported(EGFCCaptureSource::Photo)));
	}
}

void UGFCCreateScreen::HandlePost()
{
	UGFCApiClient* C = Client();
	if (!C || Chosen.Path.IsEmpty() || Job.IsValid())
	{
		return;
	}
	FGFCUploadRequest Req;
	Req.FilePath = Chosen.Path;
	Req.FileName = Chosen.FileName;
	Req.MimeType = Chosen.MimeType;
	Req.Title = TitleBox ? TitleBox->GetText().ToString().TrimStartAndEnd() : FString();
	Req.Description = DescriptionBox ? DescriptionBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (!Chosen.ThumbnailPath.IsEmpty())
	{
		FFileHelper::LoadFileToArray(Req.ThumbnailJpeg, *Chosen.ThumbnailPath);
	}
	else if (Chosen.MimeType.StartsWith(TEXT("image/")))
	{
		Req.ThumbnailJpeg = FGFCMediaCapture::MakeImageThumbnail(Chosen.Path, 512);
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
			WeakThis->SetUploading(false);
			if (!R.bOk)
			{
				if (WeakThis->ResultText)
				{
					WeakThis->ResultText->SetText(FText::FromString(R.Error));
					WeakThis->ResultText->SetColorAndOpacity(FSlateColor(R.Error.Contains(TEXT("cancelled")) ? TextDim : Danger));
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
}

#undef LOCTEXT_NAMESPACE
