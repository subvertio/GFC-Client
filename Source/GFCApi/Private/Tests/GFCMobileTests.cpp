// Tests for the mobile-first pieces: deep links, qualified views, the cache
// policy's accounting, and the runtime model parser's budgets.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCCacheStore.h"
#include "GFCDeepLink.h"
#include "GFCModelParser.h"
#include "GFCViewTracker.h"
#include "HAL/FileManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;

	/** Builds a .glb: JSON chunk + BIN chunk, 4-byte aligned. */
	TArray<uint8> MakeGlb(const FString& Json, const TArray<uint8>& Bin)
	{
		FTCHARToUTF8 Utf8(*Json);
		TArray<uint8> J((const uint8*)Utf8.Get(), Utf8.Length());
		while (J.Num() % 4) J.Add(' ');
		TArray<uint8> B = Bin;
		while (B.Num() % 4) B.Add(0);
		TArray<uint8> Out;
		auto U32 = [&Out](uint32 V) { Out.Append((const uint8*)&V, 4); };
		U32(0x46546C67);
		U32(2);
		U32(12 + 8 + J.Num() + (B.Num() ? 8 + B.Num() : 0));
		U32(J.Num());
		U32(0x4E4F534A);
		Out.Append(J);
		if (B.Num())
		{
			U32(B.Num());
			U32(0x004E4942);
			Out.Append(B);
		}
		return Out;
	}

	/** One triangle: positions (3 x vec3 float) then indices (3 x ushort). */
	TArray<uint8> TriangleBin(float Height = 1.f)
	{
		const float P[9] = {0, 0, 0, 1, 0, 0, 0, Height, 0};
		const uint16 I[3] = {0, 1, 2};
		TArray<uint8> Bin((const uint8*)P, sizeof(P));
		Bin.Append((const uint8*)I, sizeof(I));
		return Bin;
	}

	FString TriangleJson(const FString& Extra = FString(), int32 IndexCount = 3, int32 PosByteLength = 36)
	{
		return FString::Printf(TEXT(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
			"nodes":[{"mesh":0}],
			"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
			"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0.5,0.25,1],"metallicFactor":0.2,"roughnessFactor":0.6}}],
			"buffers":[{"byteLength":42}],
			"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":%d},{"buffer":0,"byteOffset":36,"byteLength":6}],
			"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			             {"bufferView":1,"componentType":5123,"count":%d,"type":"SCALAR"}]%s})"),
			PosByteLength, IndexCount, *Extra);
	}
}

// ---------------------------------------------------------------------------
// Deep links
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCDeepLinkTest, "GFC.Mobile.DeepLink", Flags)
bool FGFCDeepLinkTest::RunTest(const FString&)
{
	const TArray<FString> Hosts{TEXT("node.galacticfederation.tech"), TEXT("galacticfederation.tech")};
	auto Parse = [&](const TCHAR* Url) -> FString
	{
		FString Id;
		return FGFCDeepLink::ParseSeedId(Url, Hosts, TEXT("gfc"), Id) ? Id : FString(TEXT("<rejected>"));
	};
	// Accepted
	TestEqual(TEXT("app link"), Parse(TEXT("https://node.galacticfederation.tech/p/aac2d0277ccf677c3763167d")), TEXT("aac2d0277ccf677c3763167d"));
	TestEqual(TEXT("second host"), Parse(TEXT("https://galacticfederation.tech/p/abc_123-x")), TEXT("abc_123-x"));
	TestEqual(TEXT("host case"), Parse(TEXT("HTTPS://Node.GalacticFederation.Tech/p/abc")), TEXT("abc"));
	TestEqual(TEXT("trailing slash"), Parse(TEXT("https://node.galacticfederation.tech/p/abc/")), TEXT("abc"));
	TestEqual(TEXT("query and fragment"), Parse(TEXT("https://node.galacticfederation.tech/p/abc?s=x&t=1#top")), TEXT("abc"));
	TestEqual(TEXT("custom scheme"), Parse(TEXT("gfc://seed/abc123")), TEXT("abc123"));
	TestEqual(TEXT("custom scheme case"), Parse(TEXT("GFC://SEED/abc123")), TEXT("abc123"));
	TestEqual(TEXT("quoted"), Parse(TEXT("\"gfc://seed/abc\"")), TEXT("abc"));
	// Rejected safely
	const TCHAR* Bad[] = {
		TEXT(""), TEXT("abc"), TEXT("gfc:seed/abc"),
		TEXT("http://node.galacticfederation.tech/p/abc"),          // not https
		TEXT("https://evil.example/p/abc"),                         // other host
		TEXT("https://node.galacticfederation.tech.evil.example/p/abc"),
		TEXT("https://user@node.galacticfederation.tech/p/abc"),    // userinfo
		TEXT("https://node.galacticfederation.tech:444/p/abc"),     // port
		TEXT("https://node.galacticfederation.tech/x/abc"),         // other path
		TEXT("https://node.galacticfederation.tech/p/"),            // no id
		TEXT("https://node.galacticfederation.tech/p/a/b"),         // extra segment
		TEXT("https://node.galacticfederation.tech/p/../wallets"),  // traversal
		TEXT("https://node.galacticfederation.tech/p/a%2Fb"),       // encoded
		TEXT("https://node.galacticfederation.tech/p/abc def"),     // space
		TEXT("gfc://seed/"), TEXT("gfc://wallet/abc"), TEXT("gfc://seed/<script>"),
		TEXT("javascript://seed/abc"), TEXT("file:///p/abc"),
	};
	for (const TCHAR* B : Bad)
	{
		TestEqual(FString::Printf(TEXT("rejects [%s]"), B), Parse(B), TEXT("<rejected>"));
	}
	TestEqual(TEXT("too long"), Parse(*(TEXT("gfc://seed/") + FString::ChrN(200, 'a'))), TEXT("<rejected>"));

	// PC command line
	FString Id;
	TestTrue(TEXT("command line -GFCLink="), FGFCDeepLink::FindInCommandLine(TEXT("GFCClient.uproject -game -GFCLink=gfc://seed/cmd1"), Id) && Id == TEXT("cmd1"));
	TestTrue(TEXT("bare link argument"), FGFCDeepLink::FindInCommandLine(TEXT("GFCClient.uproject -game \"https://node.galacticfederation.tech/p/cmd2\""), Id) && Id == TEXT("cmd2"));
	TestFalse(TEXT("no link"), FGFCDeepLink::FindInCommandLine(TEXT("GFCClient.uproject -game -log"), Id));

	// What we share is what we open: the share URL must round-trip through the app link parser.
	FString Back;
	TestTrue(TEXT("share URL opens in the app"), FGFCDeepLink::ParseSeedId(UGFCApiClient::BuildShareUrl(TEXT("round_trip-1")), Back) && Back == TEXT("round_trip-1"));
	return true;
}

// ---------------------------------------------------------------------------
// Qualified views
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCViewTrackerTest, "GFC.Mobile.QualifiedView", Flags)
bool FGFCViewTrackerTest::RunTest(const FString&)
{
	FGFCViewRules Rules; // 2 s image, 3 s model, 5 s media, 50% of clips under 10 s
	auto Run = [](FGFCViewTracker& T, float Seconds, bool bVisible, bool bLoaded, float Step = 1.f / 60.f)
	{
		bool bReported = false;
		for (float t = 0; t < Seconds; t += Step) bReported |= T.Tick(Step, bVisible, bLoaded);
		return bReported;
	};

	{
		FGFCViewTracker T(Rules);
		T.Begin(TEXT("img"), EGFCMediaKind::Image, false);
		// A fast scroll: the card is on screen for 0.4 s at a time, many times over.
		bool bReported = false;
		for (int32 i = 0; i < 20; ++i)
		{
			bReported |= Run(T, 0.4f, true, true);
			bReported |= Run(T, 0.2f, false, true);
		}
		TestFalse(TEXT("fast scroll-past never qualifies (time doesn't add up across flashes)"), bReported);
		TestTrue(TEXT("2 s continuous on screen qualifies"), Run(T, 2.1f, true, true));
		TestTrue(TEXT("marked reported"), T.HasReported(TEXT("img")));
		TestFalse(TEXT("only once"), Run(T, 5.f, true, true));
		T.Begin(TEXT("img"), EGFCMediaKind::Image, false);
		TestFalse(TEXT("scroll back: not again this session"), Run(T, 5.f, true, true));
	}
	{
		FGFCViewTracker T(Rules);
		T.Begin(TEXT("thumbonly"), EGFCMediaKind::Image, false);
		TestFalse(TEXT("thumbnail only (not loaded) doesn't qualify"), Run(T, 10.f, true, false));
	}
	{
		FGFCViewTracker T(Rules);
		T.Begin(TEXT("mine"), EGFCMediaKind::Model3D, true);
		TestFalse(TEXT("own seed never reported"), Run(T, 10.f, true, true) || T.NotifyInteraction());
	}
	{
		FGFCViewTracker T(Rules);
		T.Begin(TEXT("m1"), EGFCMediaKind::Model3D, false);
		TestFalse(TEXT("interaction before the model loads doesn't count"), T.NotifyInteraction());
		TestFalse(TEXT("2.9 s isn't enough for a model"), Run(T, 2.9f, true, true));
		T.Begin(TEXT("m2"), EGFCMediaKind::Model3D, false);
		Run(T, 0.1f, true, true);
		TestTrue(TEXT("orbiting a loaded model qualifies"), T.NotifyInteraction());
	}
	{
		FGFCViewTracker T(Rules);
		T.Begin(TEXT("v"), EGFCMediaKind::Video, false);
		TestFalse(TEXT("video doesn't qualify by screen time"), Run(T, 30.f, true, true));
		TestFalse(TEXT("4 s of a long video"), T.NotifyPlayback(4.f, 120.f));
		TestTrue(TEXT("5 s of playback"), T.NotifyPlayback(5.f, 120.f));
		T.Begin(TEXT("a"), EGFCMediaKind::Audio, false);
		TestFalse(TEXT("40% of a 6 s clip"), T.NotifyPlayback(2.4f, 6.f));
		TestTrue(TEXT("50% of a 6 s clip"), T.NotifyPlayback(3.f, 6.f));
	}
	{
		FGFCViewRules Fast;
		Fast.ImageSeconds = 0.5f;
		FGFCViewTracker T(Fast);
		T.Begin(TEXT("tuned"), EGFCMediaKind::Image, false);
		TestTrue(TEXT("thresholds are tunable"), Run(T, 0.6f, true, true));
	}
	return true;
}

// ---------------------------------------------------------------------------
// Cache policy accounting
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCCacheStoreTest, "GFC.Mobile.Cache", Flags)
bool FGFCCacheStoreTest::RunTest(const FString&)
{
	const FString Base = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/GFCCacheTest"));
	const FString Root = Base / TEXT("GFCCache");
	IFileManager::Get().DeleteDirectory(*Base, false, true);
	IFileManager::Get().MakeDirectory(*Root, true);

	// Guard: never operate on a folder not named GFCCache.
	AddExpectedError(TEXT("Cache root must be a folder named GFCCache"), EAutomationExpectedErrorFlags::Contains, 1);
	{
		FGFCCacheStore Wrong(Base, FGFCCacheLimits());
		TestFalse(TEXT("root must be named GFCCache"), Wrong.IsRootValid());
		TestEqual(TEXT("invalid root deletes nothing"), Wrong.DeleteOrphans(), 0);
	}

	// Orphans from a crash are deleted on start.
	FFileHelper::SaveStringToFile(TEXT("left over"), *(Root / TEXT("old-1.glb")));
	FFileHelper::SaveStringToFile(TEXT("left over"), *(Root / TEXT("old-2.png")));
	const FString Outside = Base / TEXT("keep.txt");
	FFileHelper::SaveStringToFile(TEXT("not ours"), *Outside);

	FGFCCacheLimits L;
	L.MaxDiskBytes = 1000;
	L.MaxMemoryItems = 3;
	L.MaxMemoryBytes = 300;
	L.MinFreeDiskBytes = 500;
	FGFCCacheStore S(Root, L);
	S.FreeSpaceProvider = []() { return uint64(1000000); };
	TestEqual(TEXT("orphans deleted on start"), S.DeleteOrphans(), 2);
	TestTrue(TEXT("files outside the cache untouched"), IFileManager::Get().FileExists(*Outside));

	auto Put = [&](const FString& Owner, int32 Bytes)
	{
		const FString P = S.MakeFilePath(Owner, TEXT("g/l.b?"));
		TArray<uint8> Data;
		Data.SetNumZeroed(Bytes);
		FFileHelper::SaveArrayToFile(Data, *P);
		return S.AddFile(Owner, P, Bytes) ? P : FString();
	};

	// Disk cap
	const FString A = Put(TEXT("seed:a"), 400);
	const FString B = Put(TEXT("seed:b"), 400);
	TestTrue(TEXT("extension sanitised"), A.EndsWith(TEXT(".glb")));
	TestEqual(TEXT("two files"), S.GetStats().DiskFiles, 2);
	const FString C = Put(TEXT("seed:c"), 400);
	TestFalse(TEXT("cap reached: oldest other owner evicted"), IFileManager::Get().FileExists(*A));
	TestTrue(TEXT("new file kept"), !C.IsEmpty() && IFileManager::Get().FileExists(*C));
	TestTrue(TEXT("never over the cap"), S.GetStats().DiskBytes <= L.MaxDiskBytes);
	TestTrue(TEXT("single file over the cap refused"), Put(TEXT("seed:d"), 1001).IsEmpty());
	TestEqual(TEXT("over-cap file not left on disk"), S.DeleteOrphans(), 0);

	// Low storage
	S.FreeSpaceProvider = []() { return uint64(800); };
	TestEqual(TEXT("refuses when the device would drop under the free-space floor"), (int32)S.CanAdmit(400), (int32)FGFCCacheStore::EAdmit::LowDiskSpace);
	S.FreeSpaceProvider = []() { return uint64(1000000); };
	TestEqual(TEXT("ok with space"), (int32)S.CanAdmit(400), (int32)FGFCCacheStore::EAdmit::Ok);
	TestEqual(TEXT("bigger than the cap"), (int32)S.CanAdmit(5000), (int32)FGFCCacheStore::EAdmit::TooBig);

	// Scroll away = release
	S.ReleaseOwner(TEXT("seed:b"));
	TestFalse(TEXT("released owner's file deleted"), IFileManager::Get().FileExists(*B));

	// One heavy item at a time
	TestTrue(TEXT("first heavy item"), S.BeginHeavy(TEXT("seed:c")).IsEmpty());
	TestEqual(TEXT("next heavy item hands back the previous to release"), S.BeginHeavy(TEXT("seed:e")), TEXT("seed:c"));
	S.ReleaseOwner(TEXT("seed:c"));
	TestEqual(TEXT("disk back to baseline after viewing and leaving"), S.GetStats().DiskFiles, 0);

	// Memory caps (thumbnails)
	TArray<FString> Evicted;
	Evicted.Append(S.AddMemory(TEXT("t1"), 100));
	Evicted.Append(S.AddMemory(TEXT("t2"), 100));
	S.SetVisible({TEXT("t1")});
	Evicted.Append(S.AddMemory(TEXT("t3"), 100));
	Evicted.Append(S.AddMemory(TEXT("t4"), 100));
	TestTrue(TEXT("oldest non-visible evicted first"), Evicted.Contains(TEXT("t2")) && !Evicted.Contains(TEXT("t1")));
	TestTrue(TEXT("count cap"), S.GetStats().MemoryItems <= L.MaxMemoryItems);
	TestTrue(TEXT("bytes cap"), S.GetStats().MemoryBytes <= L.MaxMemoryBytes);
	const TArray<FString> Trimmed = S.TrimToVisible();
	TestTrue(TEXT("background keeps only visible thumbnails"), S.HasMemory(TEXT("t1")) && S.GetStats().MemoryItems == 1 && Trimmed.Num() == 2);

	// Logout wipes everything
	Put(TEXT("seed:f"), 100);
	FFileHelper::SaveStringToFile(TEXT("untracked"), *(Root / TEXT("stray.bin")));
	S.WipeAll();
	TArray<FString> Left;
	IFileManager::Get().FindFiles(Left, *(Root / TEXT("*")), true, false);
	TestEqual(TEXT("logout: no files left"), Left.Num(), 0);
	TestEqual(TEXT("logout: no memory entries"), S.GetStats().MemoryItems, 0);
	TestTrue(TEXT("logout: no heavy item"), S.GetHeavyOwner().IsEmpty());

	IFileManager::Get().DeleteDirectory(*Base, false, true);
	return true;
}

// ---------------------------------------------------------------------------
// Runtime model parser and budgets
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCModelParserTest, "GFC.Mobile.ModelParser", Flags)
bool FGFCModelParserTest::RunTest(const FString&)
{
	FGFCModelBudget Budget;
	FGFCModelData M;
	FString Err;

	// A valid one-triangle .glb
	TestTrue(TEXT("valid glb parses"), FGFCModelParser::Parse(MakeGlb(TriangleJson(), TriangleBin()), Budget, M, Err));
	TestEqual(TEXT("one triangle"), M.Triangles, 1);
	TestEqual(TEXT("one section"), M.Sections.Num(), 1);
	TestEqual(TEXT("normals generated"), M.Sections[0].Normals.Num(), 3);
	TestEqual(TEXT("material colour"), M.Materials[0].BaseColor, FLinearColor(1, 0.5f, 0.25f, 1));
	TestTrue(TEXT("metres to cm, glTF Y-up to Unreal Z-up"), FMath::IsNearlyEqual(M.Bounds.Max.Z, 100.f) && FMath::IsNearlyEqual(M.Bounds.Max.Y, 100.f));

	// Same model as .gltf with an embedded data: URI
	const FString Gltf = TriangleJson().Replace(TEXT(R"("buffers":[{"byteLength":42}])"),
		*FString::Printf(TEXT(R"("buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,%s"}])"), *FBase64::Encode(TriangleBin())));
	FTCHARToUTF8 GU(*Gltf);
	TestTrue(TEXT("embedded .gltf parses"), FGFCModelParser::Parse(TArray<uint8>((const uint8*)GU.Get(), GU.Length()), Budget, M, Err));

	// Budgets
	FGFCModelBudget Tight;
	Tight.MaxTriangles = 0;
	TestFalse(TEXT("triangle budget"), FGFCModelParser::Parse(MakeGlb(TriangleJson(), TriangleBin()), Tight, M, Err));
	TestTrue(TEXT("says why"), Err.Contains(TEXT("triangles")));
	Tight = FGFCModelBudget();
	Tight.MaxFileBytes = 64;
	TestFalse(TEXT("file size budget"), FGFCModelParser::Parse(MakeGlb(TriangleJson(), TriangleBin()), Tight, M, Err));

	// Hostile or unsupported files are rejected, never read out of bounds
	TestFalse(TEXT("accessor past its buffer"), FGFCModelParser::Parse(MakeGlb(TriangleJson(FString(), 3, 400), TriangleBin()), Budget, M, Err));
	TestFalse(TEXT("index count past its view"), FGFCModelParser::Parse(MakeGlb(TriangleJson(FString(), 300), TriangleBin()), Budget, M, Err));
	TArray<uint8> BadIdx = TriangleBin();
	BadIdx[36] = 9; // index 9 of 3 vertices
	TestFalse(TEXT("index out of range"), FGFCModelParser::Parse(MakeGlb(TriangleJson(), BadIdx), Budget, M, Err));
	TestFalse(TEXT("compressed (Draco) rejected with a message"), FGFCModelParser::Parse(
		MakeGlb(TriangleJson(TEXT(R"(,"extensionsRequired":["KHR_draco_mesh_compression"])")), TriangleBin()), Budget, M, Err));
	TestTrue(TEXT("names the feature"), Err.Contains(TEXT("draco")));
	const FString External = TriangleJson().Replace(TEXT(R"({"byteLength":42})"), TEXT(R"({"byteLength":42,"uri":"https://evil.example/x.bin"})"));
	FTCHARToUTF8 EU(*External);
	TestFalse(TEXT("external URIs never fetched"), FGFCModelParser::Parse(TArray<uint8>((const uint8*)EU.Get(), EU.Length()), Budget, M, Err));
	const FString Cycle = TriangleJson().Replace(TEXT(R"("nodes":[{"mesh":0}])"), TEXT(R"("nodes":[{"mesh":0,"children":[1]},{"children":[0]}])"));
	TestFalse(TEXT("node cycle"), FGFCModelParser::Parse(MakeGlb(Cycle, TriangleBin()), Budget, M, Err));
	TestFalse(TEXT("not a model"), FGFCModelParser::Parse({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, Budget, M, Err));
	TArray<uint8> Truncated = MakeGlb(TriangleJson(), TriangleBin());
	Truncated.SetNum(Truncated.Num() - 20);
	TestFalse(TEXT("truncated glb"), FGFCModelParser::Parse(Truncated, Budget, M, Err));
	TestFalse(TEXT("json garbage"), FGFCModelParser::Parse(MakeGlb(TEXT("{not json"), TriangleBin()), Budget, M, Err));
	return true;
}

// ---------------------------------------------------------------------------
// Mobile settings sanity
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCMobileSettingsTest, "GFC.Mobile.Settings", Flags)
bool FGFCMobileSettingsTest::RunTest(const FString&)
{
	const UGFCApiSettings* S = GetDefault<UGFCApiSettings>();
	TestTrue(TEXT("app link hosts configured"), S->AppLinkHosts.Num() > 0);
	TestEqual(TEXT("Android intent filter claims node.galacticfederation.tech first"), S->AppLinkHosts[0], TEXT("node.galacticfederation.tech"));
	TestTrue(TEXT("disk cap is set"), S->MaxDiskCacheMB > 0 && S->MaxDiskCacheMB <= 500);
	TestTrue(TEXT("model cap fits in the disk cap"), S->MaxModelMB <= S->MaxDiskCacheMB);
	TestTrue(TEXT("texture cap is phone-sized"), S->MaxTextureSize <= 4096);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
