// Unit tests for the GFC API layer: JSON parsing, error handling and URL building.
// Run: Session Frontend > Automation > "GFC.Api", or headless with Scripts/RunTests.ps1.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCJson.h"
#include "GFCSessionStore.h"
#include "GFCUrl.h"

namespace
{
	constexpr EAutomationTestFlags GFCTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

// ---------------------------------------------------------------------------
// JSON parsing
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonAuthTest, "GFC.Api.Json.Auth", GFCTestFlags)
bool FGFCJsonAuthTest::RunTest(const FString&)
{
	FGFCAuthResult A;
	TestTrue(TEXT("login response parses"), FGFCJson::ParseAuth(TEXT(R"({"ok": true, "token": "tok-123", "user": "Alice"})"), A));
	TestEqual(TEXT("username"), A.Username, TEXT("Alice"));
	TestEqual(TEXT("token"), A.Token, TEXT("tok-123"));

	FGFCAuthResult B;
	TestFalse(TEXT("missing token fails"), FGFCJson::ParseAuth(TEXT(R"({"ok": true, "user": "alice"})"), B));
	TestFalse(TEXT("not JSON fails"), FGFCJson::ParseAuth(TEXT("<html>502</html>"), B));
	TestFalse(TEXT("empty fails"), FGFCJson::ParseAuth(TEXT(""), B));

	FGFCAuthResult C;
	TestTrue(TEXT("register response parses"), FGFCJson::ParseAuth(
		TEXT(R"({"ok": true, "token": "t", "user": "new", "recovery_code": "ABCDE-FGHJK-MNPQR-STUVW-XYZ23-45678"})"), C));
	TestEqual(TEXT("recovery code"), C.RecoveryCode, TEXT("ABCDE-FGHJK-MNPQR-STUVW-XYZ23-45678"));
	TestTrue(TEXT("login has no recovery code"), A.RecoveryCode.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonRecoveryTest, "GFC.Api.Json.Recovery", GFCTestFlags)
bool FGFCJsonRecoveryTest::RunTest(const FString&)
{
	FGFCAccountSecurity S;
	TestTrue(TEXT("security parses"), FGFCJson::ParseAccountSecurity(
		TEXT(R"({"recovery_code": true, "email": "a***@example.com", "email_verified": true, "phone_verified": false})"), S));
	TestTrue(TEXT("code"), S.bRecoveryCode);
	TestTrue(TEXT("email verified"), S.bEmailVerified);
	TestEqual(TEXT("masked email"), S.Email, TEXT("a***@example.com"));
	TestFalse(TEXT("phone"), S.bPhoneVerified);
	TestFalse(TEXT("older node fails"), FGFCJson::ParseAccountSecurity(TEXT(R"({"detail": "Not Found"})"), S));

	FGFCVerifyLink L;
	TestTrue(TEXT("email link parses"), FGFCJson::ParseEmailLink(
		TEXT(R"({"url": "https://node.example/account/email?f=abc", "expires_in": 1800})"), L));
	TestEqual(TEXT("url"), L.Url, TEXT("https://node.example/account/email?f=abc"));
	TestFalse(TEXT("other schemes refused"), FGFCJson::ParseEmailLink(TEXT(R"j({"url": "javascript:alert(1)"})j"), L));

	FString Code;
	TestTrue(TEXT("new code parses"), FGFCJson::ParseRecoveryCode(TEXT(R"({"recovery_code": "AAAAA-BBBBB"})"), Code));
	TestFalse(TEXT("error fails"), FGFCJson::ParseRecoveryCode(TEXT(R"({"detail": "wrong password"})"), Code));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonProfileTest, "GFC.Api.Json.Profile", GFCTestFlags)
bool FGFCJsonProfileTest::RunTest(const FString&)
{
	FGFCProfile P;
	TestTrue(TEXT("parses"), FGFCJson::ParseProfile(TEXT(R"({"ok": true, "username": "bob", "first_name": "Bob", "last_name": "",
		"bio": "hi", "email": "b@example.com", "phone": "", "avatar": null, "created_at": "2026-10-01T00:00:00"})"), P));
	TestEqual(TEXT("username"), P.Username, TEXT("bob"));
	TestEqual(TEXT("first"), P.FirstName, TEXT("Bob"));
	TestTrue(TEXT("null avatar is empty"), P.Avatar.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonWalletTest, "GFC.Api.Json.Wallet", GFCTestFlags)
bool FGFCJsonWalletTest::RunTest(const FString&)
{
	FGFCWallet W;
	TestTrue(TEXT("GET /wallets/{id} parses"), FGFCJson::ParseWallet(
		TEXT(R"({"id": "ab12", "name": "main", "balance": 100.0, "balance_cents": 10029})"), W));
	TestEqual(TEXT("id"), W.Id, TEXT("ab12"));
	TestEqual(TEXT("name"), W.Name, TEXT("main"));
	TestEqual(TEXT("cents are authoritative"), W.BalanceCents, int64(10029));
	TestEqual(TEXT("balance from cents"), W.Balance, 100.29);

	FGFCWallet Old;
	TestTrue(TEXT("without balance_cents"), FGFCJson::ParseWallet(TEXT(R"({"id": "x", "name": "", "balance": 0.29})"), Old));
	TestEqual(TEXT("float balance rounds, not truncates"), Old.BalanceCents, int64(29));

	FGFCWallet Created;
	TestTrue(TEXT("POST /wallets/create parses"), FGFCJson::ParseCreatedWallet(
		TEXT(R"({"id": "c0ffee", "name": "Default Wallet", "gfc_balance": 100.0})"), Created));
	TestEqual(TEXT("created id"), Created.Id, TEXT("c0ffee"));
	TestEqual(TEXT("grant"), Created.BalanceCents, int64(10000));

	FGFCWallet Bad;
	TestFalse(TEXT("no id fails"), FGFCJson::ParseWallet(TEXT(R"({"name": "x"})"), Bad));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonHistoryTest, "GFC.Api.Json.History", GFCTestFlags)
bool FGFCJsonHistoryTest::RunTest(const FString&)
{
	const TCHAR* Body = TEXT(R"({"transactions": [
		{"id": "t1", "sender": "GFC-TREASURY-GENESIS-001", "recipient": "w1", "amount": 100.0, "reason": "grant",
		 "timestamp": "2026-10-27T10:00:00", "amount_cents": 10000, "tx_hash": "h1", "block_index": 3, "tx_type": "GRANT"},
		{"id": "t2", "sender": "w1", "recipient": "w2", "amount": 2.5, "reason": "tip",
		 "timestamp": "2026-10-27T11:00:00", "amount_cents": 250, "tx_hash": "", "block_index": null, "tx_type": "TRANSFER"},
		"not an object",
		{"id": "t3", "sender": "w1", "recipient": "w2", "amount": 0.07, "amount_cents": 0, "tx_type": null}
	]})");
	TArray<FGFCTransaction> Txs;
	TestTrue(TEXT("parses"), FGFCJson::ParseHistory(Body, Txs));
	if (!TestEqual(TEXT("skips non-objects"), Txs.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("grant type"), Txs[0].TxType, TEXT("GRANT"));
	TestEqual(TEXT("block index"), Txs[0].BlockIndex, int64(3));
	TestEqual(TEXT("null block index is -1"), Txs[1].BlockIndex, int64(-1));
	TestEqual(TEXT("cents"), Txs[1].AmountCents, int64(250));
	TestEqual(TEXT("amount_cents 0 falls back to amount"), Txs[2].AmountCents, int64(7));
	TestEqual(TEXT("null type defaults to TRANSFER"), Txs[2].TxType, TEXT("TRANSFER"));

	TArray<FGFCTransaction> Empty;
	TestTrue(TEXT("empty list"), FGFCJson::ParseHistory(TEXT(R"({"transactions": []})"), Empty));
	TestEqual(TEXT("no items"), Empty.Num(), 0);
	TestFalse(TEXT("wrong shape fails"), FGFCJson::ParseHistory(TEXT(R"({"items": []})"), Empty));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonTransferTest, "GFC.Api.Json.Transfer", GFCTestFlags)
bool FGFCJsonTransferTest::RunTest(const FString&)
{
	FGFCTransaction Tx;
	TestTrue(TEXT("parses"), FGFCJson::ParseTransaction(TEXT(R"({"id": "t9", "sender": "a", "recipient": "b", "amount": 1.5,
		"amount_cents": 150, "reason": "", "timestamp": "2026", "tx_hash": "hh", "tx_type": "TRANSFER", "block_index": 7, "signed": true})"), Tx));
	TestTrue(TEXT("signed"), Tx.bSigned);
	TestEqual(TEXT("cents"), Tx.AmountCents, int64(150));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonFeedTest, "GFC.Api.Json.Feed", GFCTestFlags)
bool FGFCJsonFeedTest::RunTest(const FString&)
{
	const TCHAR* Body = TEXT(R"({"items": [
		{"id": "s1", "title": "My ship", "description": "Low-poly cruiser", "tags": ["3d", "ship", ""],
		 "full": "/originals/s1_ship.glb", "thumb": "/thumbs/s1.jpg", "content_type": "3d", "file_size": 123456,
		 "gfc_earned": 1.25, "likes": 4, "comments": 2, "views": 99, "created_at": "2026-10-27T09:00:00",
		 "owner": "alice", "owner_avatar": "/avatars/alice.png", "liked": true, "bookmarked": false},
		{"id": "s2", "title": "", "description": null, "tags": [], "full": "/originals/s2_sunset.png", "thumb": null,
		 "content_type": "image", "file_size": 10, "gfc_earned": 0, "likes": 0, "comments": 0, "views": 0,
		 "created_at": "", "owner": "bob", "owner_avatar": ""},
		{"title": "no id: skipped"}
	]})");
	TArray<FGFCSeed> Seeds;
	TestTrue(TEXT("parses"), FGFCJson::ParseFeed(Body, Seeds));
	if (!TestEqual(TEXT("seeds without id are skipped"), Seeds.Num(), 2))
	{
		return false;
	}
	const FGFCSeed& Ship = Seeds[0];
	TestEqual(TEXT("title"), Ship.Title, TEXT("My ship"));
	TestEqual(TEXT("empty tags dropped"), Ship.Tags.Num(), 2);
	TestEqual(TEXT("thumb"), Ship.Thumb, TEXT("/thumbs/s1.jpg"));
	TestEqual(TEXT("file size"), Ship.FileSize, int64(123456));
	TestEqual(TEXT("views"), Ship.Views, 99);
	TestTrue(TEXT("liked"), Ship.bLiked);
	TestFalse(TEXT("bookmarked"), Ship.bBookmarked);
	TestEqual(TEXT("3d kind"), Ship.GetMediaKind(), EGFCMediaKind::Model3D);

	const FGFCSeed& Sunset = Seeds[1];
	TestTrue(TEXT("null thumb is empty"), Sunset.Thumb.IsEmpty());
	TestTrue(TEXT("null description is empty"), Sunset.Description.IsEmpty());
	TestEqual(TEXT("empty title falls back to file name"), Sunset.Title, TEXT("s2_sunset"));
	TestEqual(TEXT("image kind"), Sunset.GetMediaKind(), EGFCMediaKind::Image);

	TArray<FGFCSeed> Empty;
	TestTrue(TEXT("empty feed"), FGFCJson::ParseFeed(TEXT(R"({"items": []})"), Empty));
	TestFalse(TEXT("garbage fails"), FGFCJson::ParseFeed(TEXT("[1,2,3]"), Empty));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCJsonSeedTest, "GFC.Api.Json.Seed", GFCTestFlags)
bool FGFCJsonSeedTest::RunTest(const FString&)
{
	FGFCSeed S;
	TestTrue(TEXT("parses /seeds/{id}"), FGFCJson::ParseSeed(TEXT(R"({"id": "s1", "title": "Song", "full": "/originals/s1.mp3",
		"thumb": null, "content_type": "audio", "owner": "carol", "reward_wallet": "rw1", "tags": []})"), S));
	TestEqual(TEXT("reward wallet"), S.RewardWallet, TEXT("rw1"));
	TestEqual(TEXT("audio"), S.GetMediaKind(), EGFCMediaKind::Audio);

	FGFCShareInfo Share;
	TestTrue(TEXT("parses /p/{id}/share"), FGFCJson::ParseShareInfo(TEXT(R"({"url": "https://galacticfederation.tech/p/s1",
		"text": "Song by carol on GFC", "x_intent_url": "https://x.com/intent/post?text=Song&url=u", "app_link": "gfc://seed/s1"})"), Share));
	TestEqual(TEXT("share url"), Share.Url, TEXT("https://galacticfederation.tech/p/s1"));
	TestEqual(TEXT("app link"), Share.AppLink, TEXT("gfc://seed/s1"));

	FString TokenId;
	TestTrue(TEXT("finds current token"), FGFCJson::ParseCurrentTokenId(TEXT(R"({"tokens": [
		{"id": "a", "current": false}, {"id": "b", "current": true}]})"), TokenId));
	TestEqual(TEXT("current token id"), TokenId, TEXT("b"));
	TestFalse(TEXT("none current"), FGFCJson::ParseCurrentTokenId(TEXT(R"({"tokens": [{"id": "a", "current": false}]})"), TokenId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCMediaKindTest, "GFC.Api.Json.MediaKind", GFCTestFlags)
bool FGFCMediaKindTest::RunTest(const FString&)
{
	auto Kind = [](const TCHAR* ContentType, const TCHAR* Full)
	{
		FGFCSeed S;
		S.ContentType = ContentType;
		S.Full = Full;
		return S.GetMediaKind();
	};
	TestEqual(TEXT("node 3d"), Kind(TEXT("3d"), TEXT("")), EGFCMediaKind::Model3D);
	TestEqual(TEXT("MIME model"), Kind(TEXT("model/gltf-binary"), TEXT("")), EGFCMediaKind::Model3D);
	TestEqual(TEXT("MIME video"), Kind(TEXT("video/mp4"), TEXT("")), EGFCMediaKind::Video);
	TestEqual(TEXT("other + .GLB"), Kind(TEXT("other"), TEXT("/originals/x.GLB")), EGFCMediaKind::Model3D);
	TestEqual(TEXT("unknown"), Kind(TEXT(""), TEXT("/originals/x.bin")), EGFCMediaKind::Other);
	return true;
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCErrorTest, "GFC.Api.Errors", GFCTestFlags)
bool FGFCErrorTest::RunTest(const FString&)
{
	TestEqual(TEXT("detail string"), FGFCJson::ErrorMessage(409, TEXT(R"({"detail": "username already exists"})")),
		TEXT("username already exists"));
	TestEqual(TEXT("validation list"), FGFCJson::ErrorMessage(422, TEXT(R"({"detail": [
		{"type": "missing", "loc": ["body", "password"], "msg": "Field required"},
		{"type": "missing", "loc": ["body", "username"], "msg": "Field required"}]})")),
		TEXT("password: Field required; username: Field required"));
	TestTrue(TEXT("401 without body asks to log in"), FGFCJson::ErrorMessage(401, TEXT("")).Contains(TEXT("log in")));
	TestTrue(TEXT("HTML 502"), FGFCJson::ErrorMessage(502, TEXT("<html>Bad gateway</html>")).Contains(TEXT("502")));
	TestTrue(TEXT("unreachable"), FGFCJson::ErrorMessage(0, TEXT("")).Contains(TEXT("reach")));
	TestEqual(TEXT("empty detail falls back"), FGFCJson::ErrorMessage(404, TEXT(R"({"detail": ""})")), TEXT("Not found."));
	TestEqual(TEXT("429"), FGFCJson::ErrorMessage(429, TEXT("{}")), TEXT("Too many requests. Try again in a moment."));

	FGFCApiResult R = FGFCApiResult::Failure(401, TEXT("x"));
	TestTrue(TEXT("401 is auth error"), R.IsAuthError());
	TestFalse(TEXT("401 is not forbidden"), R.IsForbidden());
	TestTrue(TEXT("403 is forbidden"), FGFCApiResult::Failure(403, TEXT("x")).IsForbidden());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCAmountTest, "GFC.Api.Amount", GFCTestFlags)
bool FGFCAmountTest::RunTest(const FString&)
{
	auto Cents = [](const TCHAR* Text) -> int64
	{
		int64 C = -1;
		return FGFCJson::ParseAmountToCents(Text, C) ? C : -1;
	};
	TestEqual(TEXT("2"), Cents(TEXT("2")), int64(200));
	TestEqual(TEXT("2.5"), Cents(TEXT("2.5")), int64(250));
	TestEqual(TEXT("2.50"), Cents(TEXT(" 2.50 ")), int64(250));
	TestEqual(TEXT("0.29 exact"), Cents(TEXT("0.29")), int64(29));
	TestEqual(TEXT(".5"), Cents(TEXT(".5")), int64(50));
	TestEqual(TEXT("3."), Cents(TEXT("3.")), int64(300));
	TestEqual(TEXT("three decimals"), Cents(TEXT("1.234")), int64(-1));
	TestEqual(TEXT("negative"), Cents(TEXT("-1")), int64(-1));
	TestEqual(TEXT("zero"), Cents(TEXT("0.00")), int64(-1));
	TestEqual(TEXT("letters"), Cents(TEXT("1e3")), int64(-1));
	TestEqual(TEXT("comma"), Cents(TEXT("1,5")), int64(-1));
	TestEqual(TEXT("two dots"), Cents(TEXT("1.2.3")), int64(-1));
	TestEqual(TEXT("empty"), Cents(TEXT("")), int64(-1));
	TestEqual(TEXT("dot only"), Cents(TEXT(".")), int64(-1));

	TestEqual(TEXT("format"), FGFCJson::FormatCents(10029), TEXT("100.29"));
	TestEqual(TEXT("format small"), FGFCJson::FormatCents(7), TEXT("0.07"));
	TestEqual(TEXT("format negative"), FGFCJson::FormatCents(-250), TEXT("-2.50"));

	// The node computes int(amount * 100). Check the client's nudged amount survives that for every cent value up to 1000 GFC.
	for (int64 C = 1; C <= 100000; ++C)
	{
		const double Sent = (static_cast<double>(C) + 0.25) / 100.0;
		if (static_cast<int64>(Sent * 100.0) != C || FMath::RoundToInt64(Sent * 100.0) != C)
		{
			AddError(FString::Printf(TEXT("amount for %lld cents doesn't survive the node's conversion"), C));
			break;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// URL building
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCUrlTest, "GFC.Api.Url", GFCTestFlags)
bool FGFCUrlTest::RunTest(const FString&)
{
	TestEqual(TEXT("unreserved kept"), FGFCUrl::Encode(TEXT("AZaz09-._~")), TEXT("AZaz09-._~"));
	TestEqual(TEXT("space and symbols"), FGFCUrl::Encode(TEXT("a b&c=d/e?f#g+h")), TEXT("a%20b%26c%3Dd%2Fe%3Ff%23g%2Bh"));
	TestEqual(TEXT("UTF-8"), FGFCUrl::Encode(TEXT("café \U0001F680")), TEXT("caf%C3%A9%20%F0%9F%9A%80"));

	TestEqual(TEXT("join"), FGFCUrl::Join(TEXT("http://n:8000/"), TEXT("/feed")), TEXT("http://n:8000/feed"));
	TestEqual(TEXT("join no slashes"), FGFCUrl::Join(TEXT("http://n"), TEXT("feed")), TEXT("http://n/feed"));
	TestEqual(TEXT("join many slashes"), FGFCUrl::Join(TEXT("http://n///"), TEXT("/feed")), TEXT("http://n/feed"));

	TestEqual(TEXT("media path"), FGFCUrl::ResolveMedia(TEXT("https://node.example"), TEXT("/thumbs/a.jpg")),
		TEXT("https://node.example/thumbs/a.jpg"));
	TestEqual(TEXT("absolute media kept"), FGFCUrl::ResolveMedia(TEXT("https://node.example"), TEXT("https://cdn.example/a.jpg")),
		TEXT("https://cdn.example/a.jpg"));
	TestTrue(TEXT("empty media"), FGFCUrl::ResolveMedia(TEXT("https://node.example"), TEXT("")).IsEmpty());

	TestEqual(TEXT("form body"), FGFCUrl::FormBody({{TEXT("username"), TEXT("al ice")}, {TEXT("password"), TEXT("p&ss=1")}}),
		TEXT("username=al%20ice&password=p%26ss%3D1"));
	TestEqual(TEXT("query"), FGFCUrl::WithQuery(TEXT("/transactions/history"), {{TEXT("wallet_id"), TEXT("ab cd")}}),
		TEXT("/transactions/history?wallet_id=ab%20cd"));
	TestEqual(TEXT("empty query values skipped"), FGFCUrl::WithQuery(TEXT("/x"), {{TEXT("a"), TEXT("")}}), TEXT("/x"));

	TestEqual(TEXT("share url"), FGFCUrl::ShareUrl(TEXT("https://galacticfederation.tech/"), TEXT("abc-123")),
		TEXT("https://galacticfederation.tech/p/abc-123"));
	TestEqual(TEXT("share url encodes id"), FGFCUrl::ShareUrl(TEXT("https://g.tech"), TEXT("a/b")), TEXT("https://g.tech/p/a%2Fb"));

	TestEqual(TEXT("share text"), FGFCUrl::ShareText(TEXT("My ship"), TEXT("alice")), TEXT("My ship by alice on GFC"));
	TestEqual(TEXT("share text no owner"), FGFCUrl::ShareText(TEXT(""), TEXT("")), TEXT("a seed on GFC"));

	TestEqual(TEXT("x intent"), FGFCUrl::XIntentUrl(TEXT("My ship by alice on GFC"), TEXT("https://galacticfederation.tech/p/abc")),
		TEXT("https://x.com/intent/post?text=My%20ship%20by%20alice%20on%20GFC&url=https%3A%2F%2Fgalacticfederation.tech%2Fp%2Fabc"));

	TestEqual(TEXT("normalize"), FGFCUrl::NormalizeBaseUrl(TEXT("  http://127.0.0.1:8000/ ")), TEXT("http://127.0.0.1:8000"));
	TestTrue(TEXT("reject no scheme"), FGFCUrl::NormalizeBaseUrl(TEXT("node.example")).IsEmpty());
	TestTrue(TEXT("reject other scheme"), FGFCUrl::NormalizeBaseUrl(TEXT("ftp://node.example")).IsEmpty());
	TestTrue(TEXT("reject scheme only"), FGFCUrl::NormalizeBaseUrl(TEXT("https://")).IsEmpty());

	TestTrue(TEXT("client share url uses settings"),
		UGFCApiClient::BuildShareUrl(TEXT("s1")).EndsWith(TEXT("/p/s1")));
	return true;
}

// Regression: unquoted URLs in DefaultGame.ini get cut at "//" and came through as "https:".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCSettingsTest, "GFC.Api.Settings", GFCTestFlags)
bool FGFCSettingsTest::RunTest(const FString&)
{
	const UGFCApiSettings* S = GetDefault<UGFCApiSettings>();
	TestFalse(TEXT("ini NodeBaseUrl is a full URL"), FGFCUrl::NormalizeBaseUrl(S->NodeBaseUrl).IsEmpty());
	TestFalse(TEXT("ini ShareBaseUrl is a full URL"), FGFCUrl::NormalizeBaseUrl(S->ShareBaseUrl).IsEmpty());
	TestTrue(TEXT("share base is https"), UGFCApiSettings::GetEffectiveShareBaseUrl().StartsWith(TEXT("https://")));
	TestTrue(TEXT("share url has a host"), UGFCApiClient::BuildShareUrl(TEXT("s1")).StartsWith(TEXT("https://")) &&
		!UGFCApiClient::BuildShareUrl(TEXT("s1")).StartsWith(TEXT("https:/p/")));
	TestFalse(TEXT("effective node url"), UGFCApiSettings::GetEffectiveNodeBaseUrl().IsEmpty());
	return true;
}

// ---------------------------------------------------------------------------
// Client behaviour without a network
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCClientOfflineTest, "GFC.Api.Client.Offline", GFCTestFlags)
bool FGFCClientOfflineTest::RunTest(const FString&)
{
	UGFCApiClient* Client = NewObject<UGFCApiClient>();
	Client->SetBaseUrl(TEXT("http://127.0.0.1:1/"));
	TestEqual(TEXT("base url normalised"), Client->GetBaseUrl(), TEXT("http://127.0.0.1:1"));
	Client->SetBaseUrl(TEXT("not a url"));
	TestEqual(TEXT("bad base url ignored"), Client->GetBaseUrl(), TEXT("http://127.0.0.1:1"));
	TestEqual(TEXT("media url"), Client->ResolveMediaUrl(TEXT("/thumbs/t.jpg")), TEXT("http://127.0.0.1:1/thumbs/t.jpg"));

	// Authenticated calls without a session fail immediately, without touching the network.
	bool bCalled = false;
	Client->CreateWallet(TEXT("x"), [&](const FGFCApiResult& R, const FGFCWallet&)
	{
		bCalled = true;
		TestFalse(TEXT("no session fails"), R.bOk);
		TestEqual(TEXT("as 401"), R.StatusCode, 401);
	});
	TestTrue(TEXT("callback ran synchronously"), bCalled);

	Client->SetSession(TEXT("alice"), TEXT("secret-token"));
	TestTrue(TEXT("logged in"), Client->IsLoggedIn());

	bCalled = false;
	Client->CreateTransfer(TEXT("w1"), TEXT("w2"), 0, TEXT(""), [&](const FGFCApiResult& R, const FGFCTransaction&)
	{
		bCalled = true;
		TestEqual(TEXT("zero amount rejected locally"), R.StatusCode, 400);
	});
	TestTrue(TEXT("zero amount callback"), bCalled);

	bCalled = false;
	Client->ExportWallets(TEXT("pw"), TEXT("short"), [&](const FGFCApiResult& R, const FString& Json)
	{
		bCalled = true;
		TestEqual(TEXT("short passphrase rejected locally"), R.StatusCode, 400);
		TestTrue(TEXT("no backup"), Json.IsEmpty());
	});
	TestTrue(TEXT("short passphrase callback"), bCalled);

	Client->ClearSession();
	TestFalse(TEXT("logged out"), Client->IsLoggedIn());
	TestTrue(TEXT("token forgotten"), Client->GetTokenForStorage().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCSecureStoreTest, "GFC.Api.SecureStore", GFCTestFlags)
bool FGFCSecureStoreTest::RunTest(const FString&)
{
	const FString Secret = TEXT("tok_éxample-1234567890");
	TArray<uint8> Blob;
	TestTrue(TEXT("protect"), FGFCSecureStore::Protect(Secret, Blob));
	FString Back;
	TestTrue(TEXT("unprotect"), FGFCSecureStore::Unprotect(Blob, Back));
	TestTrue(TEXT("round trip"), Back == Secret);
	if (FGFCSecureStore::IsHardwareOrOsBacked())
	{
		const FTCHARToUTF8 Utf8(*Secret);
		const TArrayView<const uint8> Plain(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		bool bContainsPlain = false;
		for (int32 i = 0; i + Plain.Num() <= Blob.Num(); ++i)
		{
			if (FMemory::Memcmp(Blob.GetData() + i, Plain.GetData(), Plain.Num()) == 0)
			{
				bContainsPlain = true;
				break;
			}
		}
		TestFalse(TEXT("blob doesn't contain the plain token"), bContainsPlain);

		TArray<uint8> Tampered = Blob;
		Tampered[Tampered.Num() / 2] ^= 0x5A;
		FString Out;
		TestFalse(TEXT("tampered blob fails"), FGFCSecureStore::Unprotect(Tampered, Out));
	}
	FString Out;
	TestFalse(TEXT("empty blob fails"), FGFCSecureStore::Unprotect(TArray<uint8>(), Out));
	return true;
}

// ---------------------------------------------------------------------------
// A real HTTP round trip to a closed port: transport errors surface as status 0.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCClientUnreachableTest, "GFC.Api.Client.Unreachable", GFCTestFlags)
bool FGFCClientUnreachableTest::RunTest(const FString&)
{
	UGFCApiClient* Client = NewObject<UGFCApiClient>();
	Client->AddToRoot();
	Client->SetBaseUrl(TEXT("http://127.0.0.1:9"));
	Client->TimeoutSeconds = 5.f;

	struct FState { bool bDone = false; FGFCApiResult Result; };
	TSharedRef<FState> State = MakeShared<FState>();
	Client->GetFeed([State](const FGFCApiResult& R, const TArray<FGFCSeed>&) { State->bDone = true; State->Result = R; });

	const double Start = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Client, Start]()
	{
		if (!State->bDone && FPlatformTime::Seconds() - Start < 15.0)
		{
			return false;
		}
		TestTrue(TEXT("callback fired"), State->bDone);
		TestFalse(TEXT("failed"), State->Result.bOk);
		TestEqual(TEXT("status 0 = unreachable"), State->Result.StatusCode, 0);
		TestTrue(TEXT("friendly message"), State->Result.Error.Contains(TEXT("reach")));
		Client->RemoveFromRoot();
		return true;
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
