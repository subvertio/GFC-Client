// Tests for text-only posts: titles, the .txt seed, and how the feed shortens the text.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiTypes.h"
#include "GFCMediaCapture.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "UI/GFCCreateScreen.h"
#include "UI/GFCPostEditor.h"
#include "UI/GFCSeedCard.h"

namespace
{
	constexpr EAutomationTestFlags TextPostFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCTextTitleTest, "GFC.Posting.TextTitleFromBody", TextPostFlags)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCTextSeedFileTest, "GFC.Posting.TextSeedFile", TextPostFlags)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCFeedBodyTest, "GFC.Posting.FeedBodyShortened", TextPostFlags)
bool FGFCFeedBodyTest::RunTest(const FString&)
{
	TestEqual(TEXT("short kept"), UGFCSeedCard::FeedBody(TEXT("  hi \n"), 200), FString(TEXT("hi")));
	const FString Cut = UGFCSeedCard::FeedBody(FString::ChrN(300, TEXT('x')), 200);
	TestEqual(TEXT("cut to the limit"), Cut.Len(), 200);
	TestTrue(TEXT("ellipsis"), Cut.EndsWith(TEXT("…")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCPostEditRulesTest, "GFC.Posting.EditRequest", TextPostFlags)
bool FGFCPostEditRulesTest::RunTest(const FString&)
{
	FGFCSeedEditRequest R;

	// A text post titled from its first line: fixing the typo moves the title with it.
	FGFCSeed Text;
	Text.ContentType = TEXT("text/plain");
	Text.Description = TEXT("Helo galaxy\nmore");
	Text.Title = TEXT("Helo galaxy");
	TestFalse(TEXT("unchanged: nothing to send"), UGFCPostPanel::BuildRequest(Text, Text.Title, Text.Description, TEXT(""), R));
	TestTrue(TEXT("typo fixed"), UGFCPostPanel::BuildRequest(Text, Text.Title, TEXT("Hello galaxy\nmore"), TEXT(""), R));
	TestEqual(TEXT("title follows the first line"), R.Title.Get(FString()), FString(TEXT("Hello galaxy")));
	TestEqual(TEXT("new text"), R.Description.Get(FString()), FString(TEXT("Hello galaxy\nmore")));
	TestFalse(TEXT("tags untouched"), R.Tags.IsSet());
	TestTrue(TEXT("own title kept"), UGFCPostPanel::BuildRequest(Text, TEXT("My title"), TEXT("Hello galaxy\nmore"), TEXT(""), R));
	TestEqual(TEXT("typed title"), R.Title.Get(FString()), FString(TEXT("My title")));
	TestTrue(TEXT("emptied title"), UGFCPostPanel::BuildRequest(Text, TEXT(""), TEXT("Bye\nnow"), TEXT(""), R));
	TestEqual(TEXT("emptied title is the first line"), R.Title.Get(FString()), FString(TEXT("Bye")));

	// A media post: only what changed is sent; an emptied title stays as it was.
	FGFCSeed Media;
	Media.ContentType = TEXT("image/png");
	Media.Title = TEXT("My ship");
	Media.Description = TEXT("render");
	Media.Tags = {TEXT("ships"), TEXT("3d")};
	TestFalse(TEXT("same tags, other spacing"), UGFCPostPanel::BuildRequest(Media, TEXT("My ship "), TEXT("render"), TEXT(" ships ,3d,"), R));
	TestTrue(TEXT("title fixed"), UGFCPostPanel::BuildRequest(Media, TEXT("My ship (fixed)"), TEXT("render"), TEXT("ships, 3d"), R));
	TestTrue(TEXT("title only"), R.Title.IsSet() && !R.Description.IsSet() && !R.Tags.IsSet());
	TestFalse(TEXT("emptied media title isn't sent"), UGFCPostPanel::BuildRequest(Media, TEXT(""), TEXT("render"), TEXT("ships, 3d"), R));
	TestTrue(TEXT("tags changed"), UGFCPostPanel::BuildRequest(Media, TEXT("My ship"), TEXT("render"), TEXT("ships"), R));
	TestEqual(TEXT("one tag"), R.Tags.Get(TArray<FString>()).Num(), 1);

	FGFCSeedEdit Saved;
	Saved.Title = TEXT("T");
	Saved.Description = TEXT("D");
	Saved.Tags = {TEXT("x")};
	Saved.EditedAt = TEXT("2026-10-29T10:00:00Z");
	Media.Likes = 4;
	const FGFCSeed After = UGFCPostPanel::ApplyEdit(Media, Saved);
	TestEqual(TEXT("applied title"), After.Title, FString(TEXT("T")));
	TestEqual(TEXT("edited_at set"), After.EditedAt, Saved.EditedAt);
	TestEqual(TEXT("counts kept"), After.Likes, 4);
	return true;
}

#endif
