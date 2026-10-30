// Tests for GFC Exclusives: the node's exclusive/teaser fields, /play links, x_post, the upload field,
// and what the app concludes about who may see the full post.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiClient.h"
#include "GFCApiTypes.h"
#include "GFCJson.h"
#include "GFCShare.h"
#include "GFCUpload.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr EAutomationTestFlags ExclusiveFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCExclusiveSeedJsonTest, "GFC.Exclusive.SeedJson", ExclusiveFlags)
bool FGFCExclusiveSeedJsonTest::RunTest(const FString&)
{
	TArray<FGFCSeed> Seeds;
	TestTrue(TEXT("feed parses"), FGFCJson::ParseFeed(TEXT(R"({"items":[
		{"id":"v1","title":"Launch","content_type":"video","full":"/teasers/v1.mp4","stream":null,"exclusive":true,"teaser":"/teasers/v1.mp4"},
		{"id":"t1","title":"Secret plans","description":"","content_type":"other","full":null,"exclusive":true,"teaser":null},
		{"id":"p1","title":"Open","content_type":"video","full":"/originals/p1.mp4","stream":"/streams/p1.mp4","exclusive":false,"teaser":null},
		{"id":"o1","title":"Older node","content_type":"image","full":"/originals/o1.jpg"}]})"), Seeds));
	TestEqual(TEXT("all four"), Seeds.Num(), 4);
	TestTrue(TEXT("video is exclusive"), Seeds[0].bExclusive);
	TestEqual(TEXT("teaser"), Seeds[0].Teaser, FString(TEXT("/teasers/v1.mp4")));
	TestEqual(TEXT("still a video"), Seeds[0].GetMediaKind(), EGFCMediaKind::Video);
	TestTrue(TEXT("the public copy is the teaser"), Seeds[0].GetPlaybackPath() == Seeds[0].Teaser);
	TestTrue(TEXT("text is exclusive"), Seeds[1].bExclusive);
	TestEqual(TEXT("an exclusive text post with no file is still text"), Seeds[1].GetMediaKind(), EGFCMediaKind::Text);
	TestEqual(TEXT("title stays public"), Seeds[1].Title, FString(TEXT("Secret plans")));
	TestFalse(TEXT("not exclusive"), Seeds[2].bExclusive);
	TestFalse(TEXT("older nodes: not exclusive"), Seeds[3].bExclusive);
	TestTrue(TEXT("older nodes: no teaser"), Seeds[3].Teaser.IsEmpty());

	TestTrue(TEXT("videos can be exclusive"), FGFCSeed::CanBeExclusive(EGFCMediaKind::Video));
	TestTrue(TEXT("text can be exclusive"), FGFCSeed::CanBeExclusive(EGFCMediaKind::Text));
	TestFalse(TEXT("images can't"), FGFCSeed::CanBeExclusive(EGFCMediaKind::Image));
	TestFalse(TEXT("audio can't"), FGFCSeed::CanBeExclusive(EGFCMediaKind::Audio));
	TestFalse(TEXT("3D can't"), FGFCSeed::CanBeExclusive(EGFCMediaKind::Model3D));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCPlayLinksJsonTest, "GFC.Exclusive.PlayLinksJson", ExclusiveFlags)
bool FGFCPlayLinksJsonTest::RunTest(const FString&)
{
	FGFCPlayLinks L;
	TestTrue(TEXT("exclusive video"), FGFCJson::ParsePlayLinks(TEXT(R"({"ok":true,"id":"v1","exclusive":true,
		"full":"/originals/v1_clip.mp4?exp=1900000000&sig=abc","stream":"/streams/v1.mp4?exp=1900000000&sig=def",
		"teaser":"/teasers/v1.mp4","text":null,"expires_in":21600})"), L));
	TestTrue(TEXT("exclusive"), L.bExclusive);
	TestEqual(TEXT("plays the signed stream"), L.GetPlaybackPath(), FString(TEXT("/streams/v1.mp4?exp=1900000000&sig=def")));
	TestEqual(TEXT("expires"), L.ExpiresIn, 21600);
	TestTrue(TEXT("no text"), L.Text.IsEmpty());

	TestTrue(TEXT("exclusive text"), FGFCJson::ParsePlayLinks(TEXT(R"({"ok":true,"id":"t1","exclusive":true,
		"full":"/originals/t1_post.txt?exp=1&sig=x","stream":null,"teaser":null,"text":"The whole post.","expires_in":21600})"), L));
	TestEqual(TEXT("body"), L.Text, FString(TEXT("The whole post.")));
	TestEqual(TEXT("no stream: the full file"), L.GetPlaybackPath(), FString(TEXT("/originals/t1_post.txt?exp=1&sig=x")));

	TestTrue(TEXT("ordinary post"), FGFCJson::ParsePlayLinks(TEXT(R"({"ok":true,"id":"p1","exclusive":false,
		"full":"/originals/p1.mp4","stream":null,"expires_in":null})"), L));
	TestFalse(TEXT("not exclusive"), L.bExclusive);
	TestEqual(TEXT("doesn't expire"), L.ExpiresIn, 0);
	TestFalse(TEXT("an error body isn't links"), FGFCJson::ParsePlayLinks(TEXT(R"({"detail":"Verify your phone number to watch GFC Exclusives."})"), L));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCExclusiveUploadJsonTest, "GFC.Exclusive.UploadJson", ExclusiveFlags)
bool FGFCExclusiveUploadJsonTest::RunTest(const FString&)
{
	FGFCUploadResult R;
	TestTrue(TEXT("upload parses"), FGFCJson::ParseUploadResult(TEXT(R"({"id":"v1","title":"Launch","content_type":"video",
		"full":"/teasers/v1.mp4","exclusive":true,"teaser":"/teasers/v1.mp4","rewards_enabled":true,
		"x_post":{"text":"Launch: 5-second preview. The full clip is exclusive to the GFC Network","url":"https://galacticfederation.tech/p/v1",
		"intent":"https://x.com/intent/post?text=Launch&url=https%3A%2F%2Fgalacticfederation.tech%2Fp%2Fv1"}})"), R));
	TestTrue(TEXT("exclusive"), R.Seed.bExclusive);
	TestEqual(TEXT("teaser"), R.Seed.Teaser, FString(TEXT("/teasers/v1.mp4")));
	TestTrue(TEXT("x post"), R.XPost.IsSet());
	TestTrue(TEXT("intent"), R.XPost.Intent.StartsWith(TEXT("https://x.com/intent/post")));
	TestEqual(TEXT("share text is the text and the link"), R.XPost.ShareText(),
		FString(TEXT("Launch: 5-second preview. The full clip is exclusive to the GFC Network https://galacticfederation.tech/p/v1")));

	TestTrue(TEXT("plain upload parses"), FGFCJson::ParseUploadResult(TEXT(R"({"id":"p1","title":"Open"})"), R));
	TestFalse(TEXT("no x post"), R.XPost.IsSet());
	TestFalse(TEXT("not exclusive"), R.Seed.bExclusive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCExclusiveMultipartTest, "GFC.Exclusive.UploadField", ExclusiveFlags)
bool FGFCExclusiveMultipartTest::RunTest(const FString&)
{
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("exclusive-test.txt"));
	TestTrue(TEXT("test file"), FFileHelper::SaveStringToFile(TEXT("hello"), *Path));
	FGFCUploadRequest Req;
	Req.FilePath = Path;
	Req.FileName = TEXT("post.txt");
	Req.Title = TEXT("t");
	FGFCMultipart Body;
	TestTrue(TEXT("builds"), FGFCMultipart::Build(Req, Body));
	auto AsText = [](const TArray<uint8>& Bytes)
	{
		return FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num()));
	};
	TestFalse(TEXT("no exclusive field by default"), AsText(Body.Prefix).Contains(TEXT("name=\"exclusive\"")));
	Req.bExclusive = true;
	TestTrue(TEXT("builds exclusive"), FGFCMultipart::Build(Req, Body));
	TestTrue(TEXT("exclusive=true"), AsText(Body.Prefix).Contains(TEXT("name=\"exclusive\"\r\n\r\ntrue\r\n")));
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCExclusiveAccessTest, "GFC.Exclusive.Access", ExclusiveFlags)
bool FGFCExclusiveAccessTest::RunTest(const FString&)
{
	UGFCApiClient* C = NewObject<UGFCApiClient>();
	FGFCSeed Open;
	Open.Id = TEXT("p1");
	FGFCSeed Exclusive;
	Exclusive.Id = TEXT("v1");
	Exclusive.Owner = TEXT("alice");
	Exclusive.bExclusive = true;

	TestEqual(TEXT("ordinary posts are open to all"), C->GetExclusiveAccess(Open), EGFCExclusiveAccess::Full);
	TestEqual(TEXT("logged out: log in"), C->GetExclusiveAccess(Exclusive), EGFCExclusiveAccess::NeedsLogin);
	C->SetSession(TEXT("Alice"), TEXT("test-token"));
	TestEqual(TEXT("the poster sees their own"), C->GetExclusiveAccess(Exclusive), EGFCExclusiveAccess::Full);
	C->SetSession(TEXT("bob"), TEXT("test-token"));
	TestEqual(TEXT("someone else: ask the node"), C->GetExclusiveAccess(Exclusive), EGFCExclusiveAccess::Unknown);
	C->ClearSession();
	TestEqual(TEXT("logged out again"), C->GetExclusiveAccess(Exclusive), EGFCExclusiveAccess::NeedsLogin);

	TestEqual(TEXT("teaser file name"), FGFCShare::TeaserFileName(TEXT("My: launch?"), TEXT("abc")), FString(TEXT("My_ launch_-teaser.mp4")));
	TestEqual(TEXT("untitled teaser"), FGFCShare::TeaserFileName(TEXT("  "), TEXT("abcdef0123456789")), FString(TEXT("GFC-abcdef012345-teaser.mp4")));
	return true;
}

#endif
