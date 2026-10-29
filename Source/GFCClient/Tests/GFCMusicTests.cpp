// Tests for the music player: time labels and which files count as audio.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiTypes.h"
#include "GFCMediaCapture.h"
#include "Viewer/GFCMusicPlayer.h"
#include "Viewer/GFCWavStreamer.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCPlaybackPathTest, "GFC.Music.PlaybackPath", MusicFlags)
bool FGFCPlaybackPathTest::RunTest(const FString&)
{
	FGFCSeed S;
	S.Full = TEXT("/originals/clip.mov");
	TestEqual(TEXT("original until the copy exists"), S.GetPlaybackPath(), S.Full);
	S.Stream = TEXT("/streams/abc.mp4");
	TestEqual(TEXT("the streaming copy once it does"), S.GetPlaybackPath(), S.Stream);
	return true;
}

namespace
{
	void MusicPut32(TArray<uint8>& B, uint32 V) { for (int32 i = 0; i < 4; ++i) B.Add(uint8(V >> (8 * i))); }
	void MusicPut16(TArray<uint8>& B, uint16 V) { B.Add(uint8(V)); B.Add(uint8(V >> 8)); }
	void MusicTag(TArray<uint8>& B, const char* T) { B.Append(reinterpret_cast<const uint8*>(T), 4); }

	/** RIFF/WAVE header: fmt (plain or extensible), a LIST chunk, then data of DataBytes. */
	TArray<uint8> Header(uint16 Format, uint16 Channels, uint32 Rate, uint16 Bits, uint32 DataBytes, bool bExtensible = false)
	{
		TArray<uint8> B;
		MusicTag(B, "RIFF"); MusicPut32(B, 0); MusicTag(B, "WAVE");
		MusicTag(B, "fmt "); MusicPut32(B, bExtensible ? 40 : 16);
		MusicPut16(B, bExtensible ? 0xFFFE : Format); MusicPut16(B, Channels); MusicPut32(B, Rate); MusicPut32(B, Rate * Channels * Bits / 8);
		MusicPut16(B, Channels * Bits / 8); MusicPut16(B, Bits);
		if (bExtensible) { MusicPut16(B, 22); MusicPut16(B, Bits); MusicPut32(B, 3); MusicPut16(B, Format); for (int32 i = 0; i < 14; ++i) B.Add(0); }
		MusicTag(B, "LIST"); MusicPut32(B, 5); B.Append({'I', 'N', 'F', 'O', '!', 0}); // odd size: padded
		MusicTag(B, "data"); MusicPut32(B, DataBytes);
		return B;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCWavHeaderTest, "GFC.Music.WavHeader", MusicFlags)
bool FGFCWavHeaderTest::RunTest(const FString&)
{
	FGFCWavFormat F;
	FString Err;
	// The live track's layout: 16-bit stereo 48 kHz, a LIST chunk before the samples
	TArray<uint8> H = Header(1, 2, 48000, 16, 57369600);
	TestTrue(TEXT("parses"), UGFCWavStreamer::ParseHeader(H.GetData(), H.Num(), F, Err) == EGFCWavParse::Ok);
	TestEqual(TEXT("data offset after LIST"), F.DataOffset, int64(H.Num()));
	TestEqual(TEXT("duration"), int32(FMath::RoundToInt(F.DurationSeconds())), 299);
	TestTrue(TEXT("needs more when cut"), UGFCWavStreamer::ParseHeader(H.GetData(), 30, F, Err) == EGFCWavParse::NeedMore);

	TArray<uint8> Ext = Header(1, 2, 44100, 24, 6000, true);
	TestTrue(TEXT("extensible 24-bit"), UGFCWavStreamer::ParseHeader(Ext.GetData(), Ext.Num(), F, Err) == EGFCWavParse::Ok);
	TestEqual(TEXT("24-bit frames"), F.BlockAlign, 6);
	TArray<uint8> Flt = Header(3, 1, 22050, 32, 400);
	TestTrue(TEXT("float"), UGFCWavStreamer::ParseHeader(Flt.GetData(), Flt.Num(), F, Err) == EGFCWavParse::Ok);
	TestTrue(TEXT("float flagged"), F.bFloat);

	TArray<uint8> Six = Header(1, 6, 48000, 16, 1200);
	TestTrue(TEXT("5.1 refused"), UGFCWavStreamer::ParseHeader(Six.GetData(), Six.Num(), F, Err) == EGFCWavParse::Unsupported);
	const uint8 Mp3[16] = {'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	TestTrue(TEXT("mp3 isn't wav"), UGFCWavStreamer::ParseHeader(Mp3, 16, F, Err) == EGFCWavParse::Unsupported);
	TestFalse(TEXT("mp3 doesn't look like wav"), UGFCWavStreamer::LooksLikeWav(Mp3, 16));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCWavConvertTest, "GFC.Music.WavToPcm16", MusicFlags)
bool FGFCWavConvertTest::RunTest(const FString&)
{
	FGFCWavFormat F;
	F.Channels = 1;
	TArray<uint8> Out;
	auto Sample = [&Out](int32 i) { int16 V; FMemory::Memcpy(&V, Out.GetData() + i * 2, 2); return V; };

	F.BitsPerSample = 24; F.BlockAlign = 3;
	const uint8 S24[6] = {0x00, 0x00, 0x80, 0xFF, 0xFF, 0x7F}; // min, max
	UGFCWavStreamer::ToPcm16(S24, 2, F, Out);
	TestEqual(TEXT("24-bit min"), Sample(0), int16(-32768));
	TestEqual(TEXT("24-bit max"), Sample(1), int16(32767));

	F.BitsPerSample = 32; F.BlockAlign = 4; F.bFloat = true;
	const float Fl[3] = {0.5f, -1.f, 2.f};
	UGFCWavStreamer::ToPcm16(reinterpret_cast<const uint8*>(Fl), 3, F, Out);
	TestEqual(TEXT("float half"), Sample(0), int16(16383));
	TestEqual(TEXT("float -1"), Sample(1), int16(-32767));
	TestEqual(TEXT("float clipped"), Sample(2), int16(32767));

	F.BitsPerSample = 8; F.BlockAlign = 1; F.bFloat = false;
	const uint8 S8[2] = {128, 255};
	UGFCWavStreamer::ToPcm16(S8, 2, F, Out);
	TestEqual(TEXT("8-bit silence"), Sample(0), int16(0));
	TestEqual(TEXT("8-bit high"), Sample(1), int16(127 << 8));
	return true;
}

#endif