#include "GFCJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString Str(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		// Tolerates null and numbers (some ids/timestamps may be numeric on older nodes).
		const TSharedPtr<FJsonValue> V = Obj->TryGetField(Field);
		if (!V.IsValid() || V->IsNull())
		{
			return FString();
		}
		FString Out;
		return V->TryGetString(Out) ? Out : FString();
	}

	double Num(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double Default = 0.0)
	{
		const TSharedPtr<FJsonValue> V = Obj->TryGetField(Field);
		double Out = Default;
		if (V.IsValid() && !V->IsNull())
		{
			V->TryGetNumber(Out);
		}
		return Out;
	}

	int64 Int(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int64 Default = 0)
	{
		const TSharedPtr<FJsonValue> V = Obj->TryGetField(Field);
		int64 Out = Default;
		if (V.IsValid() && !V->IsNull())
		{
			V->TryGetNumber(Out);
		}
		return Out;
	}

	bool Bool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		bool Out = false;
		Obj->TryGetBoolField(Field, Out);
		return Out;
	}

	bool ReadItems(const FString& Body, const TCHAR* Field, TArray<TSharedPtr<FJsonValue>>& OutItems)
	{
		const TSharedPtr<FJsonObject> Obj = FGFCJson::ParseObject(Body);
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Items))
		{
			return false;
		}
		OutItems = *Items;
		return true;
	}
}

TSharedPtr<FJsonObject> FGFCJson::ParseObject(const FString& Body)
{
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Obj))
	{
		return nullptr;
	}
	return Obj;
}

bool FGFCJson::ParseAuth(const FString& Body, FGFCAuthResult& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	Out.Token = Str(Obj, TEXT("token"));
	Out.Username = Str(Obj, TEXT("user"));
	return !Out.Token.IsEmpty() && !Out.Username.IsEmpty();
}

bool FGFCJson::ParseProfile(const FString& Body, FGFCProfile& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	Out.Username = Str(Obj, TEXT("username"));
	Out.FirstName = Str(Obj, TEXT("first_name"));
	Out.LastName = Str(Obj, TEXT("last_name"));
	Out.Bio = Str(Obj, TEXT("bio"));
	Out.Avatar = Str(Obj, TEXT("avatar"));
	Out.CreatedAt = Str(Obj, TEXT("created_at"));
	return !Out.Username.IsEmpty();
}

bool FGFCJson::ParseWallet(const FString& Body, FGFCWallet& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	Out.Id = Str(Obj, TEXT("id"));
	Out.Name = Str(Obj, TEXT("name"));
	if (Obj->HasField(TEXT("balance_cents")))
	{
		Out.BalanceCents = Int(Obj, TEXT("balance_cents"));
	}
	else
	{
		Out.BalanceCents = FMath::RoundToInt64(Num(Obj, TEXT("balance")) * 100.0);
	}
	Out.Balance = Out.BalanceCents / 100.0;
	return !Out.Id.IsEmpty();
}

bool FGFCJson::ParseCreatedWallet(const FString& Body, FGFCWallet& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	Out.Id = Str(Obj, TEXT("id"));
	Out.Name = Str(Obj, TEXT("name"));
	Out.BalanceCents = FMath::RoundToInt64(Num(Obj, TEXT("gfc_balance")) * 100.0);
	Out.Balance = Out.BalanceCents / 100.0;
	return !Out.Id.IsEmpty();
}

void FGFCJson::ReadTransaction(const TSharedPtr<FJsonObject>& Obj, FGFCTransaction& Out)
{
	Out.Id = Str(Obj, TEXT("id"));
	Out.Sender = Str(Obj, TEXT("sender"));
	Out.Recipient = Str(Obj, TEXT("recipient"));
	Out.AmountCents = Obj->HasField(TEXT("amount_cents"))
		? Int(Obj, TEXT("amount_cents"))
		: FMath::RoundToInt64(Num(Obj, TEXT("amount")) * 100.0);
	if (Out.AmountCents == 0)
	{
		Out.AmountCents = FMath::RoundToInt64(Num(Obj, TEXT("amount")) * 100.0);
	}
	Out.Amount = Out.AmountCents / 100.0;
	Out.Reason = Str(Obj, TEXT("reason"));
	Out.Timestamp = Str(Obj, TEXT("timestamp"));
	Out.TxHash = Str(Obj, TEXT("tx_hash"));
	Out.TxType = Str(Obj, TEXT("tx_type"));
	if (Out.TxType.IsEmpty())
	{
		Out.TxType = TEXT("TRANSFER");
	}
	Out.BlockIndex = Int(Obj, TEXT("block_index"), -1);
	Out.bSigned = Bool(Obj, TEXT("signed"));
}

bool FGFCJson::ParseTransaction(const FString& Body, FGFCTransaction& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	ReadTransaction(Obj, Out);
	return !Out.Id.IsEmpty();
}

bool FGFCJson::ParseHistory(const FString& Body, TArray<FGFCTransaction>& Out)
{
	TArray<TSharedPtr<FJsonValue>> Items;
	if (!ReadItems(Body, TEXT("transactions"), Items))
	{
		return false;
	}
	Out.Reset(Items.Num());
	for (const TSharedPtr<FJsonValue>& Item : Items)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Item.IsValid() && Item->TryGetObject(Obj))
		{
			ReadTransaction(*Obj, Out.AddDefaulted_GetRef());
		}
	}
	return true;
}

void FGFCJson::ReadSeed(const TSharedPtr<FJsonObject>& Obj, FGFCSeed& Out)
{
	Out.Id = Str(Obj, TEXT("id"));
	Out.Title = Str(Obj, TEXT("title"));
	Out.Description = Str(Obj, TEXT("description"));
	Out.Tags.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
	if (Obj->TryGetArrayField(TEXT("tags"), Tags))
	{
		for (const TSharedPtr<FJsonValue>& T : *Tags)
		{
			FString Tag;
			if (T.IsValid() && T->TryGetString(Tag) && !Tag.IsEmpty())
			{
				Out.Tags.Add(Tag);
			}
		}
	}
	Out.Full = Str(Obj, TEXT("full"));
	Out.Thumb = Str(Obj, TEXT("thumb"));
	Out.ContentType = Str(Obj, TEXT("content_type"));
	Out.FileSize = Int(Obj, TEXT("file_size"));
	Out.GfcEarned = Num(Obj, TEXT("gfc_earned"));
	Out.Likes = static_cast<int32>(Int(Obj, TEXT("likes")));
	Out.Comments = static_cast<int32>(Int(Obj, TEXT("comments")));
	Out.Views = static_cast<int32>(Int(Obj, TEXT("views")));
	Out.CreatedAt = Str(Obj, TEXT("created_at"));
	Out.Owner = Str(Obj, TEXT("owner"));
	Out.OwnerAvatar = Str(Obj, TEXT("owner_avatar"));
	Out.RewardWallet = Str(Obj, TEXT("reward_wallet"));
	Out.bLiked = Bool(Obj, TEXT("liked"));
	Out.bBookmarked = Bool(Obj, TEXT("bookmarked"));
	ReadRepost(Obj, Out.RepostOf);
	if (Out.Title.IsEmpty())
	{
		Out.Title = FPaths::GetBaseFilename(Out.Full);
	}
}

bool FGFCJson::ParseSeed(const FString& Body, FGFCSeed& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	ReadSeed(Obj, Out);
	return !Out.Id.IsEmpty();
}

bool FGFCJson::ParseFeed(const FString& Body, TArray<FGFCSeed>& Out)
{
	return ReadSeeds(Body, TEXT("items"), Out);
}

bool FGFCJson::ParseSearchSeeds(const FString& Body, TArray<FGFCSeed>& Out)
{
	return ReadSeeds(Body, TEXT("seeds"), Out);
}

bool FGFCJson::ReadSeeds(const FString& Body, const TCHAR* Field, TArray<FGFCSeed>& Out)
{
	TArray<TSharedPtr<FJsonValue>> Items;
	if (!ReadItems(Body, Field, Items))
	{
		return false;
	}
	Out.Reset(Items.Num());
	for (const TSharedPtr<FJsonValue>& Item : Items)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Item.IsValid() && Item->TryGetObject(Obj))
		{
			FGFCSeed Seed;
			ReadSeed(*Obj, Seed);
			if (!Seed.Id.IsEmpty())
			{
				Out.Add(MoveTemp(Seed));
			}
		}
	}
	return true;
}

bool FGFCJson::ParseShareInfo(const FString& Body, FGFCShareInfo& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	Out.Url = Str(Obj, TEXT("url"));
	Out.Text = Str(Obj, TEXT("text"));
	Out.XIntentUrl = Str(Obj, TEXT("x_intent_url"));
	Out.AppLink = Str(Obj, TEXT("app_link"));
	return !Out.Url.IsEmpty();
}

bool FGFCJson::ParseCurrentTokenId(const FString& Body, FString& OutId)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	// Accept {"tokens": [...]} or {"items": [...]}.
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Obj->TryGetArrayField(TEXT("tokens"), Items) && !Obj->TryGetArrayField(TEXT("items"), Items))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		const TSharedPtr<FJsonObject>* T = nullptr;
		if (Item.IsValid() && Item->TryGetObject(T) && Bool(*T, TEXT("current")))
		{
			OutId = Str(*T, TEXT("id"));
			return !OutId.IsEmpty();
		}
	}
	return false;
}

void FGFCJson::ReadRepost(const TSharedPtr<FJsonObject>& Obj, FGFCRepostInfo& Out)
{
	Out = FGFCRepostInfo();
	const TSharedPtr<FJsonObject>* Repost = nullptr;
	if (Obj.IsValid() && Obj->TryGetObjectField(TEXT("repost_of"), Repost) && Repost->IsValid())
	{
		Out.SeedId = Str(*Repost, TEXT("seed_id"));
		Out.Owner = Str(*Repost, TEXT("owner"));
		Out.Url = Str(*Repost, TEXT("url"));
	}
}

bool FGFCJson::ParseUploadResult(const FString& Body, FGFCUploadResult& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	ReadSeed(Obj, Out.Seed);
	if (Out.Seed.Description.IsEmpty())
	{
		Out.Seed.Description = Str(Obj, TEXT("desc")); // the upload response uses "desc"
	}
	Out.bRewardsEnabled = Bool(Obj, TEXT("rewards_enabled"));
	Out.RepostOf = Out.Seed.RepostOf;
	Out.FirstPostBonus = FMath::Max(0.0, Num(Obj, TEXT("first_post_bonus")));
	Out.bFirstPostBonusPending = Bool(Obj, TEXT("first_post_bonus_pending"));
	return !Out.Seed.Id.IsEmpty();
}

bool FGFCJson::ParseVerifyStatus(const FString& Body, FGFCVerifyStatus& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid() || !Obj->HasField(TEXT("verified")))
	{
		return false;
	}
	Out = FGFCVerifyStatus();
	Out.bVerified = Bool(Obj, TEXT("verified"));
	const TSharedPtr<FJsonObject>* Rewards = nullptr;
	if (Obj->TryGetObjectField(TEXT("rewards"), Rewards))
	{
		Out.Grant = FMath::Max(0.0, Num(*Rewards, TEXT("grant")));
		Out.FirstPostBonus = FMath::Max(0.0, Num(*Rewards, TEXT("first_post_bonus")));
	}
	const TArray<TSharedPtr<FJsonValue>>* Wallets = nullptr;
	if (Obj->TryGetArrayField(TEXT("wallets"), Wallets))
	{
		for (const TSharedPtr<FJsonValue>& V : *Wallets)
		{
			FString Id;
			if (V.IsValid() && V->TryGetString(Id) && !Id.IsEmpty() && Id.Len() <= 128)
			{
				Out.Wallets.Add(Id);
			}
		}
	}
	return true;
}

bool FGFCJson::ParseVerifyLink(const FString& Body, FGFCVerifyLink& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid() || !Obj->HasField(TEXT("verified")))
	{
		return false;
	}
	Out.bVerified = Bool(Obj, TEXT("verified"));
	Out.Url = Str(Obj, TEXT("url"));
	// Only ever open the node's own https page, whatever a node sends.
	return Out.bVerified || Out.Url.StartsWith(TEXT("https://")) || Out.Url.StartsWith(TEXT("http://127.0.0.1"))
		|| Out.Url.StartsWith(TEXT("http://localhost"));
}

bool FGFCJson::ParseBlocks(const FString& Body, TArray<FString>& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Obj.IsValid() || !Obj->TryGetArrayField(TEXT("blocked"), Arr))
	{
		return false;
	}
	Out.Reset();
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		FString Name;
		if (V.IsValid() && V->TryGetString(Name) && !Name.IsEmpty())
		{
			Out.Add(Name);
		}
	}
	return true;
}

bool FGFCJson::ParseToggle(const FString& Body, FGFCToggleResult& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid())
	{
		return false;
	}
	if (Obj->HasField(TEXT("liked")))
	{
		Out.bOn = Bool(Obj, TEXT("liked"));
		Out.Count = int32(Int(Obj, TEXT("like_count"), -1));
		return true;
	}
	if (Obj->HasField(TEXT("bookmarked")))
	{
		Out.bOn = Bool(Obj, TEXT("bookmarked"));
		Out.Count = -1;
		return true;
	}
	return false;
}

bool FGFCJson::ParseNetworkStats(const FString& Body, FGFCNetworkStats& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	const TArray<TSharedPtr<FJsonValue>>* Top = nullptr;
	if (!Obj.IsValid() || !Obj->TryGetArrayField(TEXT("top_seeders"), Top))
	{
		return false;
	}
	Out.TotalRewardGfc = Num(Obj, TEXT("total_reward_gfc"));
	Out.TotalTransfers = Int(Obj, TEXT("total_transfers"));
	Out.TopEarners.Reset();
	for (const TSharedPtr<FJsonValue>& V : *Top)
	{
		const TSharedPtr<FJsonObject>* E = nullptr;
		if (V.IsValid() && V->TryGetObject(E))
		{
			FGFCTopEarner Earner;
			Earner.Wallet = Str(*E, TEXT("wallet"));
			Earner.EarnedGfc = Num(*E, TEXT("total_earned_gfc"));
			Earner.Transfers = int32(Int(*E, TEXT("transfer_count")));
			if (!Earner.Wallet.IsEmpty())
			{
				Out.TopEarners.Add(MoveTemp(Earner));
			}
		}
	}
	return true;
}

bool FGFCJson::ParsePlayResult(const FString& Body, FGFCPlayResult& Out)
{
	const TSharedPtr<FJsonObject> Obj = ParseObject(Body);
	if (!Obj.IsValid() || !Obj->HasField(TEXT("credited")))
	{
		return false;
	}
	Out.bCredited = Bool(Obj, TEXT("credited"));
	Out.Amount = Num(Obj, TEXT("amount"));
	Out.Reason = Str(Obj, TEXT("reason"));
	return true;
}

FString FGFCJson::ErrorMessage(int32 StatusCode, const FString& Body)
{
	if (StatusCode == 0)
	{
		return TEXT("Can't reach the node. Check your connection and the node address.");
	}

	if (const TSharedPtr<FJsonObject> Obj = ParseObject(Body))
	{
		FString Detail;
		if (Obj->TryGetStringField(TEXT("detail"), Detail) && !Detail.IsEmpty())
		{
			return Detail;
		}
		// FastAPI validation errors: [{"loc": ["body", "password"], "msg": "Field required"}, ...]
		const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
		if (Obj->TryGetArrayField(TEXT("detail"), Errors) && Errors->Num() > 0)
		{
			TArray<FString> Parts;
			for (const TSharedPtr<FJsonValue>& E : *Errors)
			{
				const TSharedPtr<FJsonObject>* EObj = nullptr;
				if (!E.IsValid() || !E->TryGetObject(EObj))
				{
					continue;
				}
				FString Msg;
				(*EObj)->TryGetStringField(TEXT("msg"), Msg);
				const TArray<TSharedPtr<FJsonValue>>* Loc = nullptr;
				FString Field;
				if ((*EObj)->TryGetArrayField(TEXT("loc"), Loc) && Loc->Num() > 0)
				{
					(*Loc).Last()->TryGetString(Field);
				}
				Parts.Add(Field.IsEmpty() ? Msg : FString::Printf(TEXT("%s: %s"), *Field, *Msg));
			}
			if (Parts.Num() > 0)
			{
				return FString::Join(Parts, TEXT("; "));
			}
		}
	}

	switch (StatusCode)
	{
	case 400: return TEXT("The node rejected the request.");
	case 401: return TEXT("Your session has expired. Please log in again.");
	case 403: return TEXT("Not allowed with this account or token.");
	case 404: return TEXT("Not found.");
	case 409: return TEXT("That already exists.");
	case 429: return TEXT("Too many requests. Try again in a moment.");
	default: break;
	}
	return StatusCode >= 500
		? FString::Printf(TEXT("The node had a problem (HTTP %d). Try again later."), StatusCode)
		: FString::Printf(TEXT("Request failed (HTTP %d)."), StatusCode);
}

bool FGFCJson::ParseAmountToCents(const FString& Text, int64& OutCents)
{
	const FString T = Text.TrimStartAndEnd();
	if (T.IsEmpty() || T.Len() > 15)
	{
		return false;
	}
	FString Whole, Frac;
	if (!T.Split(TEXT("."), &Whole, &Frac))
	{
		Whole = T;
	}
	if (Whole.IsEmpty() && Frac.IsEmpty())
	{
		return false;
	}
	if (Frac.Len() > 2)
	{
		return false;
	}
	for (const TCHAR C : Whole + Frac)
	{
		if (!FChar::IsDigit(C))
		{
			return false; // also rejects '-', '+', ',' and exponents
		}
	}
	const int64 W = Whole.IsEmpty() ? 0 : FCString::Atoi64(*Whole);
	int64 F = Frac.IsEmpty() ? 0 : FCString::Atoi64(*Frac);
	if (Frac.Len() == 1)
	{
		F *= 10;
	}
	OutCents = W * 100 + F;
	return OutCents > 0;
}

FString FGFCJson::FormatCents(int64 Cents)
{
	const bool bNeg = Cents < 0;
	const int64 A = bNeg ? -Cents : Cents;
	return FString::Printf(TEXT("%s%lld.%02lld"), bNeg ? TEXT("-") : TEXT(""), A / 100, A % 100);
}
