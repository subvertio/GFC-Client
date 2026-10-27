// Tests for the node answers the desktop layout reads: search, like/bookmark toggles and the earnings stats.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiTypes.h"
#include "GFCJson.h"

namespace
{
	constexpr EAutomationTestFlags DesktopApiFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCSearchJsonTest, "GFC.Desktop.SearchJson", DesktopApiFlags)
bool FGFCSearchJsonTest::RunTest(const FString&)
{
	TArray<FGFCSeed> Seeds;
	TestTrue(TEXT("search parses"), FGFCJson::ParseSearchSeeds(TEXT(R"({"seeds":[
		{"id":"a1","title":"Teapot","owner":"alice","likes":3,"liked":true},
		{"title":"no id"}],"transactions":[{"id":"t1"}],"seed_count":2,"transaction_count":1})"), Seeds));
	TestEqual(TEXT("seeds without an id dropped"), Seeds.Num(), 1);
	TestEqual(TEXT("id"), Seeds[0].Id, FString(TEXT("a1")));
	TestTrue(TEXT("liked"), Seeds[0].bLiked);
	TestTrue(TEXT("empty query result"), FGFCJson::ParseSearchSeeds(TEXT(R"({"seeds":[],"transactions":[]})"), Seeds));
	TestEqual(TEXT("none"), Seeds.Num(), 0);
	TestFalse(TEXT("needs the seeds field"), FGFCJson::ParseSearchSeeds(TEXT(R"({"items":[]})"), Seeds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCToggleJsonTest, "GFC.Desktop.ToggleJson", DesktopApiFlags)
bool FGFCToggleJsonTest::RunTest(const FString&)
{
	FGFCToggleResult R;
	TestTrue(TEXT("like parses"), FGFCJson::ParseToggle(TEXT(R"({"ok":true,"liked":true,"like_count":7})"), R));
	TestTrue(TEXT("liked"), R.bOn);
	TestEqual(TEXT("count"), R.Count, 7);
	TestTrue(TEXT("unlike parses"), FGFCJson::ParseToggle(TEXT(R"({"ok":true,"liked":false,"like_count":6})"), R));
	TestFalse(TEXT("unliked"), R.bOn);
	TestTrue(TEXT("bookmark parses"), FGFCJson::ParseToggle(TEXT(R"({"ok":true,"bookmarked":true})"), R));
	TestTrue(TEXT("bookmarked"), R.bOn);
	TestEqual(TEXT("no count for bookmarks"), R.Count, -1);
	TestFalse(TEXT("needs a state"), FGFCJson::ParseToggle(TEXT(R"({"ok":true})"), R));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCNetworkStatsJsonTest, "GFC.Desktop.NetworkStatsJson", DesktopApiFlags)
bool FGFCNetworkStatsJsonTest::RunTest(const FString&)
{
	FGFCNetworkStats S;
	TestTrue(TEXT("stats parse"), FGFCJson::ParseNetworkStats(TEXT(R"({"ok":true,"total_transfers":12,"total_reward_gfc":3.5,
		"top_seeders":[{"wallet":"w1","total_earned_gfc":2.25,"transfer_count":9,"total_bytes_mb":1.0},{"wallet":"","total_earned_gfc":1}]})"), S));
	TestEqual(TEXT("transfers"), S.TotalTransfers, int64(12));
	TestEqual(TEXT("total"), S.TotalRewardGfc, 3.5);
	TestEqual(TEXT("earners (no wallet dropped)"), S.TopEarners.Num(), 1);
	TestEqual(TEXT("earned"), S.TopEarners[0].EarnedGfc, 2.25);
	TestEqual(TEXT("count"), S.TopEarners[0].Transfers, 9);
	TestFalse(TEXT("needs top_seeders"), FGFCJson::ParseNetworkStats(TEXT(R"({"ok":true})"), S));
	return true;
}

#endif
