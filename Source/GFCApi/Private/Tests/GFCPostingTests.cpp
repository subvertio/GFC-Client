// Tests for posting and the feed's fairness rules.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiClient.h"
#include "GFCJson.h"
#include "GFCMediaCapture.h"
#include "GFCUpload.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;

	TArray<FGFCSeed> SeedsBy(const TArray<FString>& Owners)
	{
		TArray<FGFCSeed> Out;
		for (int32 i = 0; i < Owners.Num(); ++i)
		{
			FGFCSeed& S = Out.AddDefaulted_GetRef();
			S.Id = FString::Printf(TEXT("s%d"), i);
			S.Owner = Owners[i];
		}
		return Out;
	}

	FString Owners(const TArray<FGFCSeed>& Seeds)
	{
		TArray<FString> O;
		for (const FGFCSeed& S : Seeds) O.Add(S.Owner);
		return FString::Join(O, TEXT(""));
	}

	int32 LongestRun(const TArray<FGFCSeed>& Seeds)
	{
		int32 Best = 0, Run = 0;
		for (int32 i = 0; i < Seeds.Num(); ++i)
		{
			Run = (i > 0 && Seeds[i].Owner == Seeds[i - 1].Owner) ? Run + 1 : 1;
			Best = FMath::Max(Best, Run);
		}
		return Best;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCFeedMixerTest, "GFC.Posting.FeedMixesCreators", Flags)
bool FGFCFeedMixerTest::RunTest(const FString&)
{
	TArray<FGFCSeed> A = SeedsBy({TEXT("a"), TEXT("a"), TEXT("a"), TEXT("a"), TEXT("b"), TEXT("c"), TEXT("a")});
	FGFCFeedMixer::MixCreators(A, 2);
	TestEqual(TEXT("one creator can't take over"), Owners(A), TEXT("aabaaca"));
	TestTrue(TEXT("max 2 in a row where others exist"), LongestRun(A) <= 2);
	TestEqual(TEXT("nothing dropped"), A.Num(), 7);

	TArray<FGFCSeed> B = SeedsBy({TEXT("a"), TEXT("b"), TEXT("a"), TEXT("b")});
	FGFCFeedMixer::MixCreators(B, 2);
	TestEqual(TEXT("already mixed: order kept"), Owners(B), TEXT("abab"));

	TArray<FGFCSeed> C = SeedsBy({TEXT("a"), TEXT("a"), TEXT("a")});
	FGFCFeedMixer::MixCreators(C, 2);
	TestEqual(TEXT("only one creator: posts kept, not hidden"), C.Num(), 3);

	TArray<FGFCSeed> D = SeedsBy({TEXT("x"), TEXT("x"), TEXT("x"), TEXT("x"), TEXT("x"), TEXT("y"), TEXT("z")});
	FGFCFeedMixer::MixCreators(D, 2);
	TestEqual(TEXT("others spread through a heavy poster"), Owners(D), TEXT("xxyxxzx"));
	TestEqual(TEXT("relative order of the heavy poster's posts kept"), D[0].Id + D[1].Id + D[3].Id, TEXT("s0s1s2"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCMultipartTest, "GFC.Posting.Multipart", Flags)
bool FGFCMultipartTest::RunTest(const FString&)
{
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/GFCPostTest"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString File = Dir / TEXT("clip.mp4");
	TArray<uint8> Payload;
	Payload.SetNumUninitialized(300000);
	for (int32 i = 0; i < Payload.Num(); ++i) Payload[i] = uint8((i * 31) ^ (i >> 7));
	FFileHelper::SaveArrayToFile(Payload, *File);

	FGFCUploadRequest R;
	R.FilePath = File;
	R.FileName = TEXT("my \"clip\".mp4");
	R.MimeType = FGFCMediaCapture::MimeForFile(TEXT("x.MP4"));
	R.Title = TEXT("Sunset run");
	R.Description = TEXT("line one\nline two");
	R.ThumbnailJpeg = {0xFF, 0xD8, 0xFF, 0xD9};

	FGFCMultipart Body;
	TestTrue(TEXT("builds"), FGFCMultipart::Build(R, Body));
	TestEqual(TEXT("mime from extension"), R.MimeType, TEXT("video/mp4"));
	TestEqual(TEXT("total size"), Body.TotalSize(), int64(Body.Prefix.Num()) + 300000 + Body.Suffix.Num());
	TestTrue(TEXT("content type carries the boundary"), Body.ContentType().EndsWith(Body.Boundary));

	const TArray<uint8> Whole = Body.ToBytes();
	const FUTF8ToTCHAR Conv((const ANSICHAR*)Body.Prefix.GetData(), Body.Prefix.Num());
	const FString Head(Conv.Length(), Conv.Get());
	TestTrue(TEXT("title field"), Head.Contains(TEXT("name=\"title\"\r\n\r\nSunset run\r\n")));
	TestTrue(TEXT("thumbnail part"), Head.Contains(TEXT("name=\"thumbnail\"; filename=\"thumb.jpg\"")));
	TestTrue(TEXT("quotes removed from the file name"), Head.Contains(TEXT("filename=\"my clip.mp4\"")));
	TestTrue(TEXT("file part last, typed"), Head.EndsWith(TEXT("Content-Type: video/mp4\r\n\r\n")));

	// The streaming archive yields exactly the same bytes, in odd-sized reads, and after a rewind.
	FGFCMultipartArchive Ar(Body);
	TestEqual(TEXT("archive size"), Ar.TotalSize(), int64(Whole.Num()));
	TArray<uint8> Streamed;
	Streamed.SetNumUninitialized(Whole.Num());
	int64 Pos = 0;
	while (Pos < Whole.Num())
	{
		const int64 Chunk = FMath::Min<int64>(7777, Whole.Num() - Pos);
		Ar.Serialize(Streamed.GetData() + Pos, Chunk);
		Pos += Chunk;
	}
	TestFalse(TEXT("no read errors"), Ar.IsError());
	TestTrue(TEXT("streamed body equals the in-memory body"), Streamed == Whole);
	Ar.Seek(0);
	uint8 First[16];
	Ar.Serialize(First, 16);
	TestTrue(TEXT("rewind for a retry"), FMemory::Memcmp(First, Whole.GetData(), 16) == 0);

	FGFCMultipart Missing;
	R.FilePath = Dir / TEXT("nope.mp4");
	TestFalse(TEXT("missing file"), FGFCMultipart::Build(R, Missing));
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCUploadJsonTest, "GFC.Posting.UploadAndRepostJson", Flags)
bool FGFCUploadJsonTest::RunTest(const FString&)
{
	FGFCUploadResult R;
	TestTrue(TEXT("upload response"), FGFCJson::ParseUploadResult(TEXT(R"({"ok": true, "id": "abc", "title": "Mine", "desc": "d",
		"tags": [], "thumb": "/thumbs/abc_thumb.jpg", "full": "/originals/x.mp4", "owner": "me", "content_type": "video",
		"file_size": 10, "rewards_enabled": true, "repost_of": null, "repost_credit_gfc": 0.0})"), R));
	TestEqual(TEXT("id"), R.Seed.Id, TEXT("abc"));
	TestEqual(TEXT("desc mapped"), R.Seed.Description, TEXT("d"));
	TestTrue(TEXT("rewards"), R.bRewardsEnabled);
	TestFalse(TEXT("not a repost"), R.RepostOf.IsSet());

	FGFCUploadResult Repost;
	TestTrue(TEXT("repost response"), FGFCJson::ParseUploadResult(TEXT(R"({"ok": true, "id": "def", "title": "Copy", "owner": "me",
		"full": "/originals/y.jpg", "rewards_enabled": true,
		"repost_of": {"seed_id": "orig1", "owner": "alice", "label": "Repost of @alice",
		              "url": "https://node.galacticfederation.tech/p/orig1", "matched_by": "phash"}})"), Repost));
	TestTrue(TEXT("repost recognised"), Repost.RepostOf.IsSet());
	TestEqual(TEXT("original creator"), Repost.RepostOf.Owner, TEXT("alice"));
	TestEqual(TEXT("original seed"), Repost.RepostOf.SeedId, TEXT("orig1"));

	TArray<FGFCSeed> Feed;
	TestTrue(TEXT("feed items carry repost_of"), FGFCJson::ParseFeed(TEXT(R"({"items": [
		{"id": "a", "owner": "bob", "repost_of": {"seed_id": "o", "owner": "carol"}},
		{"id": "b", "owner": "bob", "repost_of": null}]})"), Feed));
	TestTrue(TEXT("first is a repost"), Feed.Num() == 2 && Feed[0].RepostOf.Owner == TEXT("carol") && !Feed[1].RepostOf.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCApiTraceTest, "GFC.Posting.ApiTrace", Flags)
bool FGFCApiTraceTest::RunTest(const FString&)
{
	UGFCApiClient* C = NewObject<UGFCApiClient>();
	C->AddToRoot();
	C->SetBaseUrl(TEXT("http://127.0.0.1:9"));
	C->TimeoutSeconds = 3.f;
	C->SetSession(TEXT("me"), TEXT("super-secret-token"));
	C->GetHistory(TEXT("wallet with spaces"), [](const FGFCApiResult&, const TArray<FGFCTransaction>&) {});
	const TArray<FGFCApiTrace>& Calls = C->GetRecentCalls();
	if (TestEqual(TEXT("one call traced"), Calls.Num(), 1))
	{
		TestEqual(TEXT("path without the query"), Calls[0].Path, TEXT("/transactions/history"));
		TestEqual(TEXT("in flight"), Calls[0].Status, -1);
		TestFalse(TEXT("never the token"), Calls[0].Path.Contains(TEXT("secret")));
	}
	TestEqual(TEXT("in-flight count"), C->GetInFlightCount(), 1);
	C->ClearSession();
	C->RemoveFromRoot();

	// Oversized uploads are refused before anything is sent.
	UGFCApiClient* U = NewObject<UGFCApiClient>();
	U->SetSession(TEXT("me"), TEXT("t"));
	FGFCUploadRequest R;
	R.FilePath = TEXT("Z:/does/not/exist.mp4");
	bool bCalled = false;
	U->Upload(R, nullptr, [&](const FGFCApiResult& Res, const FGFCUploadResult&) { bCalled = true; TestEqual(TEXT("unreadable file"), Res.StatusCode, 400); });
	TestTrue(TEXT("fails fast"), bCalled);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
