// Tests for the desktop layout: the UI scale rule (phone curve vs monitor scale) and the trending ranking.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/GFCDPIScalingRule.h"
#include "UI/GFCDesktopRails.h"
#include "UI/GFCUIStyle.h"

namespace
{
	constexpr EAutomationTestFlags DesktopFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCDesktopScaleTest, "GFC.Desktop.UIScale", DesktopFlags)
bool FGFCDesktopScaleTest::RunTest(const FString&)
{
	// The target phone keeps its 443 dp layout, on the phone and in the PC phone preview.
	TestEqual(TEXT("phone"), UGFCDPIScalingRule::ScaleFor(FIntPoint(1080, 2388), false, 1.f), 2.4375f, 0.001f);
	TestEqual(TEXT("phone in landscape still uses the curve"), UGFCDPIScalingRule::ScaleFor(FIntPoint(2388, 1080), false, 1.f), 2.4375f, 0.001f);
	TestEqual(TEXT("PC phone preview"), UGFCDPIScalingRule::ScaleFor(FIntPoint(486, 1075), true, 1.f), 1.09707f, 0.001f);
	// Landscape PC windows follow the monitor, so 1280 px is 1280 dp at 100%: the desktop layout.
	TestEqual(TEXT("PC window at 100%"), UGFCDPIScalingRule::ScaleFor(FIntPoint(1280, 800), true, 1.f), 1.f);
	TestEqual(TEXT("PC window at 150%"), UGFCDPIScalingRule::ScaleFor(FIntPoint(1280, 800), true, 1.5f), 1.5f);
	TestTrue(TEXT("1280x800 is wide"), 1280.f / UGFCDPIScalingRule::ScaleFor(FIntPoint(1280, 800), true, 1.f) >= GFCUI::Desktop::Breakpoint);
	TestTrue(TEXT("the phone preview is not"), 486.f / UGFCDPIScalingRule::ScaleFor(FIntPoint(486, 1075), true, 1.f) < GFCUI::Desktop::Breakpoint);
	TestEqual(TEXT("minimised window"), UGFCDPIScalingRule::ScaleFor(FIntPoint(0, 0), true, 1.f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCTrendingTest, "GFC.Desktop.Trending", DesktopFlags)
bool FGFCTrendingTest::RunTest(const FString&)
{
	auto Seed = [](const TCHAR* Id, int32 Likes, double Earned, int32 Views)
	{
		FGFCSeed S;
		S.Id = Id;
		S.Likes = Likes;
		S.GfcEarned = Earned;
		S.Views = Views;
		return S;
	};
	const TArray<FGFCSeed> Feed = {Seed(TEXT("a"), 1, 0.0, 50), Seed(TEXT("b"), 3, 0.0, 1), Seed(TEXT("c"), 1, 2.5, 0),
		Seed(TEXT("d"), 1, 0.0, 90), Seed(TEXT("e"), 0, 0.0, 0), Seed(TEXT("f"), 0, 0.0, 0)};
	const TArray<FGFCSeed> Top = UGFCRightRail::RankTrending(Feed, 5);
	TestEqual(TEXT("at most 5"), Top.Num(), 5);
	TestEqual(TEXT("most liked first"), Top[0].Id, FString(TEXT("b")));
	TestEqual(TEXT("then GFC earned"), Top[1].Id, FString(TEXT("c")));
	TestEqual(TEXT("then views"), Top[2].Id, FString(TEXT("d")));
	TestEqual(TEXT("ties keep feed order"), Top[4].Id, FString(TEXT("e")));
	TestEqual(TEXT("empty feed"), UGFCRightRail::RankTrending({}, 5).Num(), 0);

	FGFCTopEarner E;
	E.Wallet = TEXT("0123456789abcdef0123456789abcdef");
	TestEqual(TEXT("unconfirmed earner shows the wallet"), UGFCRightRail::EarnerName(E), GFCUI::ShortId(E.Wallet));
	E.Owner = TEXT("alice");
	TestEqual(TEXT("earner shows the account"), UGFCRightRail::EarnerName(E), FString(TEXT("@alice")));
	return true;
}

#endif
