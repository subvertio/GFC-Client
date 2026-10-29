// Tests for the Bridge to Base answers (/bridge/info, /bridge/quote, /bridge/deposit) and the address check.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiTypes.h"
#include "GFCJson.h"

namespace
{
	constexpr EAutomationTestFlags BridgeApiFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCBridgeInfoJsonTest, "GFC.Bridge.InfoJson", BridgeApiFlags)
bool FGFCBridgeInfoJsonTest::RunTest(const FString&)
{
	FGFCBridgeInfo Info;
	TestTrue(TEXT("info parses"), FGFCJson::ParseBridgeInfo(TEXT(R"({"enabled":true,"chains":[
		{"chain_id":84532,"name":"Base Sepolia","escrow_wallet":"ab12","token_address":"0x1111111111111111111111111111111111111111"},
		{"chain_id":8453,"name":"Base","escrow_wallet":"cd34","token_address":null},
		{"chain_id":0,"escrow_wallet":"ef56"},
		{"chain_id":10,"escrow_wallet":""}],
		"min_deposit_cents":10000,"relay_fee_cents":100,"faucet_hold_days":14,"notice":"Not an investment."})"), Info));
	TestTrue(TEXT("enabled"), Info.bEnabled);
	TestEqual(TEXT("chains without an id or escrow dropped"), Info.Chains.Num(), 2);
	TestEqual(TEXT("chain id"), Info.Chains[0].ChainId, int64(84532));
	TestEqual(TEXT("name"), Info.Chains[0].Name, FString(TEXT("Base Sepolia")));
	TestEqual(TEXT("token"), Info.Chains[0].TokenAddress, FString(TEXT("0x1111111111111111111111111111111111111111")));
	TestTrue(TEXT("no token yet"), Info.Chains[1].TokenAddress.IsEmpty());
	TestEqual(TEXT("min"), Info.MinDepositCents, int64(10000));
	TestEqual(TEXT("fee"), Info.RelayFeeCents, int64(100));
	TestEqual(TEXT("hold"), Info.FaucetHoldDays, 14.0);
	TestEqual(TEXT("notice"), Info.Notice, FString(TEXT("Not an investment.")));

	TestTrue(TEXT("a bad token address reads as none"), FGFCJson::ParseBridgeInfo(TEXT(R"({"enabled":true,"chains":[
		{"chain_id":8453,"escrow_wallet":"ab","token_address":"0xnot-an-address"}]})"), Info));
	TestTrue(TEXT("token cleared"), Info.Chains[0].TokenAddress.IsEmpty());
	TestEqual(TEXT("unnamed chain"), Info.Chains[0].Name, FString(TEXT("Chain 8453")));

	TestTrue(TEXT("disabled node parses"), FGFCJson::ParseBridgeInfo(TEXT(R"({"enabled":false,"chains":[]})"), Info));
	TestFalse(TEXT("disabled"), Info.bEnabled);
	TestTrue(TEXT("enabled with no usable chain parses"), FGFCJson::ParseBridgeInfo(TEXT(R"({"enabled":true,"chains":[{"chain_id":1}]})"), Info));
	TestFalse(TEXT("but isn't enabled"), Info.bEnabled);
	TestFalse(TEXT("needs chains"), FGFCJson::ParseBridgeInfo(TEXT(R"({"enabled":true})"), Info));
	TestFalse(TEXT("older node: 404 body"), FGFCJson::ParseBridgeInfo(TEXT(R"({"detail":"Not Found"})"), Info));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCBridgeQuoteJsonTest, "GFC.Bridge.QuoteJson", BridgeApiFlags)
bool FGFCBridgeQuoteJsonTest::RunTest(const FString&)
{
	FGFCBridgeQuote Q;
	TestTrue(TEXT("quote parses"), FGFCJson::ParseBridgeQuote(TEXT(R"({"wallet_id":"w","balance_cents":60000,"bridgeable_cents":50000,"held_cents":10000})"), Q));
	TestEqual(TEXT("balance"), Q.BalanceCents, int64(60000));
	TestEqual(TEXT("bridgeable"), Q.BridgeableCents, int64(50000));
	TestEqual(TEXT("held"), Q.HeldCents, int64(10000));
	TestTrue(TEXT("clamped"), FGFCJson::ParseBridgeQuote(TEXT(R"({"balance_cents":100,"bridgeable_cents":500})"), Q));
	TestEqual(TEXT("never more than the balance"), Q.BridgeableCents, int64(100));
	TestEqual(TEXT("nothing held"), Q.HeldCents, int64(0));
	TestFalse(TEXT("needs bridgeable_cents"), FGFCJson::ParseBridgeQuote(TEXT(R"({"balance_cents":100})"), Q));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCBridgeDepositJsonTest, "GFC.Bridge.DepositJson", BridgeApiFlags)
bool FGFCBridgeDepositJsonTest::RunTest(const FString&)
{
	FGFCBridgeDeposit D;
	TestTrue(TEXT("deposit parses"), FGFCJson::ParseBridgeDeposit(TEXT(R"({"ok":true,"tx_id":"t1","tx_hash":"abc","chain_id":84532,
		"evm_address":"0xabababababababababababababababababababab","amount_cents":15000,"mint_cents":14900,"status":"deposited"})"), D));
	TestEqual(TEXT("hash"), D.TxHash, FString(TEXT("abc")));
	TestEqual(TEXT("chain"), D.ChainId, int64(84532));
	TestEqual(TEXT("amount"), D.AmountCents, int64(15000));
	TestEqual(TEXT("minted"), D.MintCents, int64(14900));
	TestFalse(TEXT("needs ok"), FGFCJson::ParseBridgeDeposit(TEXT(R"({"tx_hash":"abc"})"), D));
	TestFalse(TEXT("needs a hash"), FGFCJson::ParseBridgeDeposit(TEXT(R"({"ok":true})"), D));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCEvmAddressTest, "GFC.Bridge.EvmAddress", BridgeApiFlags)
bool FGFCEvmAddressTest::RunTest(const FString&)
{
	TestTrue(TEXT("lower case"), FGFCJson::IsEvmAddress(TEXT("0xabababababababababababababababababababab")));
	TestTrue(TEXT("mixed case"), FGFCJson::IsEvmAddress(TEXT("0xAbC0000000000000000000000000000000000001")));
	TestTrue(TEXT("pasted with spaces"), FGFCJson::IsEvmAddress(TEXT("  0xabababababababababababababababababababab \n")));
	TestFalse(TEXT("zero address"), FGFCJson::IsEvmAddress(TEXT("0x0000000000000000000000000000000000000000")));
	TestFalse(TEXT("too short"), FGFCJson::IsEvmAddress(TEXT("0xabab")));
	TestFalse(TEXT("too long"), FGFCJson::IsEvmAddress(TEXT("0xababababababababababababababababababababab")));
	TestFalse(TEXT("no 0x"), FGFCJson::IsEvmAddress(TEXT("abababababababababababababababababababababab")));
	TestFalse(TEXT("capital 0X"), FGFCJson::IsEvmAddress(TEXT("0Xabababababababababababababababababababab")));
	TestFalse(TEXT("not hex"), FGFCJson::IsEvmAddress(TEXT("0xgggggggggggggggggggggggggggggggggggggggg")));
	TestFalse(TEXT("a GFC wallet id"), FGFCJson::IsEvmAddress(FString::ChrN(64, TEXT('a'))));
	TestFalse(TEXT("empty"), FGFCJson::IsEvmAddress(FString()));
	return true;
}

#endif
