// Tests for the earnings moment: the first-post bonus field, "earned since last time", and the ka-ching loader.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiSubsystem.h"
#include "GFCApiTypes.h"
#include "GFCJson.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/GFCRewardBurst.h"

namespace
{
	constexpr EAutomationTestFlags RewardFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;

	void Put32(TArray<uint8>& B, uint32 V) { for (int32 i = 0; i < 4; ++i) B.Add(uint8(V >> (8 * i))); }
	void Put16(TArray<uint8>& B, uint16 V) { B.Add(uint8(V)); B.Add(uint8(V >> 8)); }
	void Tag(TArray<uint8>& B, const char* T) { B.Append(reinterpret_cast<const uint8*>(T), 4); }

	/** A tiny WAV: fmt, an odd-sized LIST chunk (padded), then data. */
	TArray<uint8> MakeWav(uint16 Bits, int32 Frames)
	{
		TArray<uint8> B;
		Tag(B, "RIFF"); Put32(B, 0); Tag(B, "WAVE");
		Tag(B, "fmt "); Put32(B, 16); Put16(B, 1); Put16(B, 2); Put32(B, 48000); Put32(B, 48000 * 4); Put16(B, 4); Put16(B, Bits);
		Tag(B, "LIST"); Put32(B, 3); B.Append({'a', 'b', 'c', 0});
		Tag(B, "data"); Put32(B, Frames * 4);
		for (int32 i = 0; i < Frames * 4; ++i) B.Add(uint8(i));
		return B;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCWavTest, "GFC.Reward.WavLoader", RewardFlags)
bool FGFCWavTest::RunTest(const FString&)
{
	TArray<uint8> Pcm;
	int32 Rate = 0, Channels = 0;
	TestTrue(TEXT("16-bit stereo parses"), UGFCRewardBurst::ParseWav(MakeWav(16, 100), Pcm, Rate, Channels));
	TestEqual(TEXT("rate"), Rate, 48000);
	TestEqual(TEXT("channels"), Channels, 2);
	TestEqual(TEXT("samples after the LIST chunk"), Pcm.Num(), 400);
	TestFalse(TEXT("8-bit refused"), UGFCRewardBurst::ParseWav(MakeWav(8, 100), Pcm, Rate, Channels));
	TestFalse(TEXT("not a wav"), UGFCRewardBurst::ParseWav(TArray<uint8>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, Pcm, Rate, Channels));
	TArray<uint8> Cut = MakeWav(16, 100);
	Cut.SetNum(Cut.Num() - 50);
	TestFalse(TEXT("truncated refused"), UGFCRewardBurst::ParseWav(Cut, Pcm, Rate, Channels));

	// The shipped ka-ching loads.
	TArray<uint8> Bytes;
	if (TestTrue(TEXT("ka_ching.wav present"), FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectContentDir() / TEXT("GFC/Sounds/ka_ching.wav")))))
	{
		TestTrue(TEXT("ka_ching.wav is 16-bit PCM"), UGFCRewardBurst::ParseWav(Bytes, Pcm, Rate, Channels));
		TestTrue(TEXT("about two seconds"), Pcm.Num() > Rate * Channels * 2 / 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCEarnedTest, "GFC.Reward.EarnedSinceLastSeen", RewardFlags)
bool FGFCEarnedTest::RunTest(const FString&)
{
	TestEqual(TEXT("first sighting is not a gain"), UGFCApiSubsystem::EarnedSince(-1, 100000), int64(0));
	TestEqual(TEXT("went up"), UGFCApiSubsystem::EarnedSince(5000, 105000), int64(100000));
	TestEqual(TEXT("unchanged"), UGFCApiSubsystem::EarnedSince(5000, 5000), int64(0));
	TestEqual(TEXT("spent is not a gain"), UGFCApiSubsystem::EarnedSince(5000, 1000), int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCBonusJsonTest, "GFC.Reward.FirstPostBonusJson", RewardFlags)
bool FGFCBonusJsonTest::RunTest(const FString&)
{
	FGFCUploadResult R;
	TestTrue(TEXT("parses"), FGFCJson::ParseUploadResult(TEXT(R"({"id":"s1","title":"gm","rewards_enabled":true,"first_post_bonus":1000})"), R));
	TestEqual(TEXT("bonus"), R.FirstPostBonus, 1000.0);
	TestFalse(TEXT("not pending"), R.bFirstPostBonusPending);

	FGFCUploadResult P;
	FGFCJson::ParseUploadResult(TEXT(R"({"id":"s2","first_post_bonus_pending":true})"), P);
	TestEqual(TEXT("no bonus yet"), P.FirstPostBonus, 0.0);
	TestTrue(TEXT("pending"), P.bFirstPostBonusPending);

	FGFCUploadResult N;
	FGFCJson::ParseUploadResult(TEXT(R"({"id":"s3","first_post_bonus":-5})"), N);
	TestEqual(TEXT("never negative"), N.FirstPostBonus, 0.0);
	return true;
}

#endif
