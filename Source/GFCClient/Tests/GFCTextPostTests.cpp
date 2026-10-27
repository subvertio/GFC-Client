// Tests for text-only posts: titles, the .txt seed, and how the feed shortens the text.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiTypes.h"
#include "GFCMediaCapture.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "UI/GFCCreateScreen.h"
#include "UI/GFCSeedCard.h"

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCTextTitleTest, "GFC.Posting.TextTitleFromBody", Flags)
bool FGFCTextTitleTest::RunTest(const FString&)
{
	TestEqual(TEXT("first line"), UGFCCreateScreen::TitleFromBody(TEXT("  Hello galaxy\r\nSecond line")), FString(TEXT("Hello galaxy")));
	TestEqual(TEXT("one line"), UGFCCreateScreen::TitleFromBody(TEXT("gm")), FString(TEXT("gm")));
	TestEqual(TEXT("empty"), UGFCCreateScreen::TitleFromBody(TEXT("   ")), FString());
	const FString Long = UGFCCreateScreen::TitleFromBody(FString::ChrN(200, TEXT('a')));
	TestEqual(TEXT("long line capped at 80"), Long.Len(), 80);
	TestTrue(TEXT("with an ellipsis"), Long.EndsWith(TEXT("…")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCTextSeedFileTest, "GFC.Posting.TextSeedFile", Flags)
bool FGFCTextSeedFileTest::RunTest(const FString&)
{
	const FString Body = TEXT("Welcome to the Federation ✨\nLine two");
	const FString Path = UGFCCreateScreen::WriteTextSeed(Body);
	if (!TestFalse(TEXT("written"), Path.IsEmpty()))
	{
		return false;
	}
	FString Back;
	TestTrue(TEXT("readable"), FFileHelper::LoadFileToString(Back, *Path));
	TestEqual(TEXT("same text (UTF-8)"), Back, Body);
	TestEqual(TEXT("uploads as text/plain"), FGFCMediaCapture::MimeForFile(Path), FString(TEXT("text/plain")));
	TestTrue(TEXT("outside the cache's own files"), Path.Contains(TEXT("/outbox/")));
	IFileManager::Get().Delete(*Path);

	// The node stores .txt as "other"; the extension makes it a text post.
	FGFCSeed S;
	S.ContentType = TEXT("other");
	S.Full = TEXT("/originals/abc_post.txt");
	TestEqual(TEXT("text kind"), S.GetMediaKind(), EGFCMediaKind::Text);
	S.ContentType = TEXT("text/plain");
	S.Full.Reset();
	TestEqual(TEXT("text MIME"), S.GetMediaKind(), EGFCMediaKind::Text);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCFeedBodyTest, "GFC.Posting.FeedBodyShortened", Flags)
bool FGFCFeedBodyTest::RunTest(const FString&)
{
	TestEqual(TEXT("short kept"), UGFCSeedCard::FeedBody(TEXT("  hi \n"), 200), FString(TEXT("hi")));
	const FString Cut = UGFCSeedCard::FeedBody(FString::ChrN(300, TEXT('x')), 200);
	TestEqual(TEXT("cut to the limit"), Cut.Len(), 200);
	TestTrue(TEXT("ellipsis"), Cut.EndsWith(TEXT("…")));
	return true;
}

#endif
