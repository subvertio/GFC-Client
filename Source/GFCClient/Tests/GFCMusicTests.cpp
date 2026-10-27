// Tests for the music player: time labels and which files count as audio.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiTypes.h"
#include "GFCMediaCapture.h"
#include "Viewer/GFCMusicPlayer.h"

namespace
{
	constexpr EAutomationTestFlags MusicFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCMusicTimeTest, "GFC.Music.TimeLabels", MusicFlags)
bool FGFCMusicTimeTest::RunTest(const FString&)
{
	TestEqual(TEXT("zero"), UGFCMusicPlayer::FormatTime(0.f), FString(TEXT("0:00")));
	TestEqual(TEXT("seconds"), UGFCMusicPlayer::FormatTime(9.7f), FString(TEXT("0:09")));
	TestEqual(TEXT("minutes"), UGFCMusicPlayer::FormatTime(225.f), FString(TEXT("3:45")));
	TestEqual(TEXT("hours"), UGFCMusicPlayer::FormatTime(3725.f), FString(TEXT("1:02:05")));
	TestEqual(TEXT("never negative"), UGFCMusicPlayer::FormatTime(-4.f), FString(TEXT("0:00")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCAudioKindsTest, "GFC.Music.AudioFormats", MusicFlags)
bool FGFCAudioKindsTest::RunTest(const FString&)
{
	for (const TCHAR* Ext : {TEXT("mp3"), TEXT("m4a"), TEXT("aac"), TEXT("wav"), TEXT("ogg"), TEXT("oga"), TEXT("opus"),
		TEXT("flac"), TEXT("aif"), TEXT("aiff"), TEXT("wma")})
	{
		FGFCSeed S;
		S.ContentType = TEXT("other"); // a node that doesn't know the extension yet
		S.Full = FString::Printf(TEXT("/originals/track.%s"), Ext);
		TestEqual(FString::Printf(TEXT(".%s is audio"), Ext), S.GetMediaKind(), EGFCMediaKind::Audio);
		TestTrue(FString::Printf(TEXT(".%s uploads with an audio type"), Ext),
			FGFCMediaCapture::MimeForFile(FString::Printf(TEXT("track.%s"), Ext)).StartsWith(TEXT("audio/")));
	}
	return true;
}

#endif
