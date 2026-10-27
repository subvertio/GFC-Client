// Tests for phone verification and report / block: the node's answers, the gfc://verified link,
// and what the app hides at once.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiClient.h"
#include "GFCApiTypes.h"
#include "GFCDeepLink.h"
#include "GFCJson.h"

namespace
{
	constexpr EAutomationTestFlags TrustFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCVerifyJsonTest, "GFC.Trust.VerifyJson", TrustFlags)
bool FGFCVerifyJsonTest::RunTest(const FString&)
{
	FGFCVerifyStatus S;
	TestTrue(TEXT("older node's status parses"), FGFCJson::ParseVerifyStatus(TEXT(R"({"verified":false})"), S));
	TestFalse(TEXT("not verified"), S.bVerified);
	TestEqual(TEXT("no rewards"), S.Rewards(), 0.0);
	TestTrue(TEXT("full status parses"), FGFCJson::ParseVerifyStatus(
		TEXT(R"({"verified":true,"rewards":{"grant":100.0,"first_post_bonus":1000.0},"wallets":["w1","w2",""]})"), S));
	TestTrue(TEXT("verified"), S.bVerified);
	TestEqual(TEXT("verifying paid grant + bonus"), S.Rewards(), 1100.0);
	TestEqual(TEXT("wallets (empty ids dropped)"), S.Wallets.Num(), 2);
	TestFalse(TEXT("status needs the field"), FGFCJson::ParseVerifyStatus(TEXT("{}"), S));

	FGFCVerifyLink Link;
	TestTrue(TEXT("link parses"), FGFCJson::ParseVerifyLink(
		TEXT(R"({"verified":false,"url":"https://node.galacticfederation.tech/verify?c=abc","expires_in":900})"), Link));
	TestEqual(TEXT("url"), Link.Url, FString(TEXT("https://node.galacticfederation.tech/verify?c=abc")));
	TestTrue(TEXT("already verified needs no url"), FGFCJson::ParseVerifyLink(TEXT(R"({"verified":true,"url":""})"), Link));
	TestTrue(TEXT("verified"), Link.bVerified);
	// Never open something that isn't the node's web page
	TestFalse(TEXT("javascript: refused"), FGFCJson::ParseVerifyLink(TEXT(R"J({"verified":false,"url":"javascript:alert(1)"})J"), Link));
	TestFalse(TEXT("plain http refused"), FGFCJson::ParseVerifyLink(TEXT(R"({"verified":false,"url":"http://evil.example/x"})"), Link));
	TestTrue(TEXT("a local test node is fine"), FGFCJson::ParseVerifyLink(TEXT(R"({"verified":false,"url":"http://127.0.0.1:8100/verify?c=x"})"), Link));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCVerifiedLinkTest, "GFC.Trust.VerifiedDeepLink", TrustFlags)
bool FGFCVerifiedLinkTest::RunTest(const FString&)
{
	TestTrue(TEXT("gfc://verified"), FGFCDeepLink::IsVerifiedLink(TEXT("gfc://verified"), TEXT("gfc")));
	TestTrue(TEXT("with a slash and query"), FGFCDeepLink::IsVerifiedLink(TEXT("GFC://Verified/?ref=x"), TEXT("gfc")));
	TestFalse(TEXT("a seed link"), FGFCDeepLink::IsVerifiedLink(TEXT("gfc://seed/abc"), TEXT("gfc")));
	TestFalse(TEXT("another scheme"), FGFCDeepLink::IsVerifiedLink(TEXT("https://verified/"), TEXT("gfc")));
	TestFalse(TEXT("lookalike host"), FGFCDeepLink::IsVerifiedLink(TEXT("gfc://verifiedx"), TEXT("gfc")));
	FString Seed;
	TestFalse(TEXT("isn't a seed link"), FGFCDeepLink::ParseSeedId(TEXT("gfc://verified"), {}, TEXT("gfc"), Seed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCBlocksTest, "GFC.Trust.BlocksAndHiding", TrustFlags)
bool FGFCBlocksTest::RunTest(const FString&)
{
	TArray<FString> Names;
	TestTrue(TEXT("blocks parse"), FGFCJson::ParseBlocks(TEXT(R"({"blocked":["Spammer","troll",""]})"), Names));
	TestEqual(TEXT("empty names dropped"), Names.Num(), 2);
	TestFalse(TEXT("blocks need the field"), FGFCJson::ParseBlocks(TEXT(R"({"items":[]})"), Names));

	// The node answers 410 for a post hidden after reports: a readable message, not a crash.
	const FString Msg = FGFCJson::ErrorMessage(410, TEXT(R"({"detail":"This post was hidden after reports."})"));
	TestTrue(TEXT("410 message"), Msg.Contains(TEXT("hidden after reports")));
	return true;
}

#endif
