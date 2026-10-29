// Tests for the node answers the desktop layout reads: search, like/bookmark toggles and the earnings stats.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiClient.h"
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
		"top_seeders":[{"wallet":"w1","owner":"alice","total_earned_gfc":2.25,"transfer_count":9,"total_bytes_mb":1.0},
		{"wallet":"w2","owner":"","total_earned_gfc":1.5},{"wallet":"w3","total_earned_gfc":1.25},{"wallet":"","total_earned_gfc":1}]})"), S));
	TestEqual(TEXT("transfers"), S.TotalTransfers, int64(12));
	TestEqual(TEXT("total"), S.TotalRewardGfc, 3.5);
	TestEqual(TEXT("earners (no wallet dropped)"), S.TopEarners.Num(), 3);
	TestEqual(TEXT("earned"), S.TopEarners[0].EarnedGfc, 2.25);
	TestEqual(TEXT("count"), S.TopEarners[0].Transfers, 9);
	TestEqual(TEXT("owner"), S.TopEarners[0].Owner, FString(TEXT("alice")));
	TestTrue(TEXT("owner unconfirmed"), S.TopEarners[1].Owner.IsEmpty());
	TestTrue(TEXT("older node: no owner"), S.TopEarners[2].Owner.IsEmpty());
	TestFalse(TEXT("needs top_seeders"), FGFCJson::ParseNetworkStats(TEXT(R"({"ok":true})"), S));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCHealthJsonTest, "GFC.Desktop.HealthJson", DesktopApiFlags)
bool FGFCHealthJsonTest::RunTest(const FString&)
{
	FGFCNodeHealth H;
	TestTrue(TEXT("mainnet parses"), FGFCJson::ParseHealth(TEXT(R"({"status":"ok","network":"mainnet","phone_required_to_post":true})"), H));
	TestEqual(TEXT("network"), H.Network, FString(TEXT("mainnet")));
	TestTrue(TEXT("phone required"), H.bPhoneRequiredToPost);
	TestTrue(TEXT("testnet parses"), FGFCJson::ParseHealth(TEXT(R"({"status":"ok","network":"testnet","phone_required_to_post":false})"), H));
	TestFalse(TEXT("anyone posts on a test net"), H.bPhoneRequiredToPost);
	TestTrue(TEXT("older node parses"), FGFCJson::ParseHealth(TEXT(R"({"status":"ok"})"), H));
	TestFalse(TEXT("older node: no rule"), H.bPhoneRequiredToPost);
	TestTrue(TEXT("older node: no network"), H.Network.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCSeedEditJsonTest, "GFC.Desktop.SeedEditJson", DesktopApiFlags)
bool FGFCSeedEditJsonTest::RunTest(const FString&)
{
	FGFCSeedEdit E;
	TestTrue(TEXT("edit result parses"), FGFCJson::ParseSeedEditResult(TEXT(R"J({"ok":true,"id":"s1","title":"My ship (fixed)",
		"description":"d","tags":["ships","3d"],"edited_at":"2026-10-29T10:00:00Z","synced":false})J"), E));
	TestEqual(TEXT("title"), E.Title, FString(TEXT("My ship (fixed)")));
	TestEqual(TEXT("tags"), E.Tags.Num(), 2);
	TestFalse(TEXT("unsynced"), E.bSynced);
	TestFalse(TEXT("needs ok and edited_at"), FGFCJson::ParseSeedEditResult(TEXT(R"({"ok":true,"id":"s1"})"), E));

	TArray<FGFCSeedEdit> Edits;
	TestTrue(TEXT("history parses"), FGFCJson::ParseSeedEdits(TEXT(R"({"edits":[
		{"id":"e2","title":"New","description":"","tags":[],"edited_at":"2026-10-29T10:00:00Z"},
		{"id":"e1","title":"Old","description":"x","tags":["a"],"edited_at":"2026-10-28T10:00:00Z"}]})"), Edits));
	TestEqual(TEXT("two versions"), Edits.Num(), 2);
	TestEqual(TEXT("newest first"), Edits[0].Title, FString(TEXT("New")));
	TestTrue(TEXT("history is synced by default"), Edits[1].bSynced);

	FGFCSeed Seed;
	FGFCJson::ReadSeed(FGFCJson::ParseObject(TEXT(R"({"id":"s1","title":"t","edited_at":"2026-10-29T10:00:00Z"})")), Seed);
	TestEqual(TEXT("seed edited_at"), Seed.EditedAt, FString(TEXT("2026-10-29T10:00:00Z")));
	FGFCJson::ReadSeed(FGFCJson::ParseObject(TEXT(R"({"id":"s2","title":"t","edited_at":""})")), Seed);
	TestTrue(TEXT("never edited"), Seed.EditedAt.IsEmpty());

	TestEqual(TEXT("tags joined"), UGFCApiClient::JoinTags({TEXT(" ships "), TEXT(""), TEXT("3d")}), FString(TEXT("ships,3d")));
	TestEqual(TEXT("tags split"), UGFCApiClient::SplitTags(TEXT("a, b,,  c ")).Num(), 3);
	return true;
}

#endif
