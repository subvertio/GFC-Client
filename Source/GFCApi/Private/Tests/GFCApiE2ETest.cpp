// End-to-end test against a running node. Skipped unless -GFCE2ENode=<url> is given,
// so it never creates accounts on a node by accident. Use a local test node:
//   Scripts/RunLocalNode.ps1, then Scripts/RunTests.ps1 -E2ENode http://127.0.0.1:8000
//
// Optional:
//   -GFCE2EExpectSeed=<id>   assert this seed is in the feed and readable
//   -GFCE2ESaveSession       leave the test user logged in (SaveGame slot) so a
//                            -game run opens straight to the feed

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "GFCApi.h"
#include "GFCApiClient.h"
#include "GFCJson.h"
#include "GFCSessionStore.h"
#include "GFCUpload.h"
#include "GFCMediaCapture.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "GFCApiSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"

namespace
{
	/** Runs async steps in order: each step gets a Next() to call when its request finishes. */
	struct FGFCE2ERunner : TSharedFromThis<FGFCE2ERunner>
	{
		using FStep = TFunction<void(TFunction<void()> Next)>;
		TArray<TPair<FString, FStep>> Steps;
		int32 Index = -1;
		bool bWaiting = false;
		bool bFinished = false;

		void Add(const FString& Name, FStep Step) { Steps.Add({Name, MoveTemp(Step)}); }

		/** Returns true when every step has run. */
		bool Tick()
		{
			if (bFinished)
			{
				return true;
			}
			if (bWaiting)
			{
				return false;
			}
			if (++Index >= Steps.Num())
			{
				bFinished = true;
				return true;
			}
			bWaiting = true;
			UE_LOG(LogGFCApi, Display, TEXT("E2E step: %s"), *Steps[Index].Key);
			TWeakPtr<FGFCE2ERunner> Weak = AsShared();
			Steps[Index].Value([Weak]() { if (TSharedPtr<FGFCE2ERunner> S = Weak.Pin()) { S->bWaiting = false; } });
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCLocalNodeE2ETest, "GFC.E2E.LocalNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FGFCLocalNodeE2ETest::RunTest(const FString&)
{
	FString Node;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GFCE2ENode="), Node) || Node.IsEmpty())
	{
		AddInfo(TEXT("Skipped: pass -GFCE2ENode=http://127.0.0.1:8000 to run against a local test node."));
		return true;
	}
	FString ExpectSeed;
	FParse::Value(FCommandLine::Get(), TEXT("GFCE2EExpectSeed="), ExpectSeed);
	const bool bSaveSession = FParse::Param(FCommandLine::Get(), TEXT("GFCE2ESaveSession"));

	UGFCApiClient* C = NewObject<UGFCApiClient>();
	C->AddToRoot();
	C->SetBaseUrl(Node);
	C->TimeoutSeconds = 20.f;
	if (!TestEqual(TEXT("node url accepted"), C->GetBaseUrl(), Node.TrimChar(TEXT('/'))))
	{
		C->RemoveFromRoot();
		return false;
	}

	// Throwaway credentials, generated here and never printed.
	struct FCtx
	{
		FString User = FString::Printf(TEXT("e2e_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10).ToLower());
		FString Password = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FString Passphrase = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FString OldToken;
		FString WalletA, WalletB;
		FString SeedId;
	};
	TSharedRef<FCtx> X = MakeShared<FCtx>();
	TSharedRef<FGFCE2ERunner> Run = MakeShared<FGFCE2ERunner>();
	AddInfo(FString::Printf(TEXT("Node %s, test user %s"), *C->GetBaseUrl(), *X->User));

	Run->Add(TEXT("register"), [this, C, X](auto Next)
	{
		C->Register(X->User, X->Password, TEXT(""), [this, C, X, Next](const FGFCApiResult& R, const FGFCAuthResult& A)
		{
			TestTrue(FString::Printf(TEXT("register ok (%s)"), *R.Error), R.bOk);
			TestEqual(TEXT("registered username"), A.Username, X->User);
			TestTrue(TEXT("register logs in"), C->IsLoggedIn());
			Next();
		});
	});
	Run->Add(TEXT("register duplicate"), [this, C, X](auto Next)
	{
		C->Register(X->User, X->Password, TEXT(""), [this, Next](const FGFCApiResult& R, const FGFCAuthResult&)
		{
			TestEqual(TEXT("duplicate is 409"), R.StatusCode, 409);
			TestEqual(TEXT("node's message"), R.Error, TEXT("username already exists"));
			Next();
		});
	});
	Run->Add(TEXT("login wrong password"), [this, C, X](auto Next)
	{
		C->Login(X->User, TEXT("definitely-wrong"), [this, Next](const FGFCApiResult& R, const FGFCAuthResult&)
		{
			TestEqual(TEXT("wrong password is 401"), R.StatusCode, 401);
			Next();
		});
	});
	Run->Add(TEXT("login"), [this, C, X](auto Next)
	{
		C->Login(X->User.ToUpper(), X->Password, [this, C, X, Next](const FGFCApiResult& R, const FGFCAuthResult& A)
		{
			TestTrue(FString::Printf(TEXT("login ok (%s)"), *R.Error), R.bOk);
			TestEqual(TEXT("canonical username"), A.Username, X->User);
			TestTrue(TEXT("logged in"), C->IsLoggedIn());
			Next();
		});
	});
	Run->Add(TEXT("users/me"), [this, C, X](auto Next)
	{
		C->GetMe([this, X, Next](const FGFCApiResult& R, const FGFCProfile& P)
		{
			TestTrue(TEXT("me ok (bearer header accepted)"), R.bOk);
			TestEqual(TEXT("me username"), P.Username, X->User);
			Next();
		});
	});
	Run->Add(TEXT("create wallet"), [this, C, X](auto Next)
	{
		C->CreateWallet(TEXT("main"), [this, X, Next](const FGFCApiResult& R, const FGFCWallet& W)
		{
			TestTrue(FString::Printf(TEXT("create wallet ok (%s)"), *R.Error), R.bOk);
			TestEqual(TEXT("wallet id is an Ed25519 public key (64 hex)"), W.Id.Len(), 64);
			TestEqual(TEXT("unverified phone: no grant yet"), W.BalanceCents, int64(0));
			X->WalletA = W.Id;
			Next();
		});
	});
	Run->Add(TEXT("create second wallet"), [this, C, X](auto Next)
	{
		C->CreateWallet(TEXT(""), [this, X, Next](const FGFCApiResult& R, const FGFCWallet& W)
		{
			TestTrue(TEXT("second wallet ok"), R.bOk);
			TestEqual(TEXT("default name"), W.Name, TEXT("Default Wallet"));
			X->WalletB = W.Id;
			Next();
		});
	});
	Run->Add(TEXT("wallet balance"), [this, C, X](auto Next)
	{
		C->GetWallet(X->WalletA, [this, X, Next](const FGFCApiResult& R, const FGFCWallet& W)
		{
			TestTrue(TEXT("balance ok"), R.bOk);
			TestEqual(TEXT("same wallet"), W.Id, X->WalletA);
			TestEqual(TEXT("name"), W.Name, TEXT("main"));
			TestEqual(TEXT("balance 0.00"), FGFCJson::FormatCents(W.BalanceCents), TEXT("0.00"));
			Next();
		});
	});
	Run->Add(TEXT("unknown wallet"), [this, C](auto Next)
	{
		C->GetWallet(TEXT("nope"), [this, Next](const FGFCApiResult& R, const FGFCWallet&)
		{
			TestEqual(TEXT("unknown wallet is 404"), R.StatusCode, 404);
			Next();
		});
	});
	Run->Add(TEXT("history"), [this, C, X](auto Next)
	{
		C->GetHistory(X->WalletA, [this, Next](const FGFCApiResult& R, const TArray<FGFCTransaction>& Txs)
		{
			TestTrue(TEXT("history ok"), R.bOk);
			TestEqual(TEXT("new wallet has no history"), Txs.Num(), 0);
			Next();
		});
	});
	Run->Add(TEXT("transfer without funds"), [this, C, X](auto Next)
	{
		C->CreateTransfer(X->WalletA, X->WalletB, 150, TEXT("e2e"), [this, C, Next](const FGFCApiResult& R, const FGFCTransaction&)
		{
			TestEqual(TEXT("insufficient balance is 400"), R.StatusCode, 400);
			TestFalse(TEXT("error message from node"), R.Error.IsEmpty());
			TestTrue(TEXT("still logged in"), C->IsLoggedIn());
			Next();
		});
	});
	Run->Add(TEXT("transfer from someone else's wallet"), [this, C, X](auto Next)
	{
		C->CreateTransfer(TEXT("GFC-TREASURY-GENESIS-001"), X->WalletA, 100, TEXT("e2e"), [this, Next](const FGFCApiResult& R, const FGFCTransaction&)
		{
			TestEqual(TEXT("not your wallet is 403"), R.StatusCode, 403);
			Next();
		});
	});
	Run->Add(TEXT("feed"), [this, C, ExpectSeed](auto Next)
	{
		C->GetFeed([this, C, ExpectSeed, Next](const FGFCApiResult& R, const TArray<FGFCSeed>& Seeds)
		{
			TestTrue(TEXT("feed ok"), R.bOk);
			AddInfo(FString::Printf(TEXT("feed has %d seeds"), Seeds.Num()));
			if (!ExpectSeed.IsEmpty())
			{
				const FGFCSeed* Found = Seeds.FindByPredicate([&](const FGFCSeed& S) { return S.Id == ExpectSeed; });
				TestNotNull(TEXT("expected seed in feed"), Found);
				if (Found)
				{
					TestFalse(TEXT("seed has a title"), Found->Title.IsEmpty());
					TestFalse(TEXT("seed has a creator"), Found->Owner.IsEmpty());
					TestFalse(TEXT("seed has a thumbnail"), Found->Thumb.IsEmpty());
				}
			}
			Next();
		});
	});
	if (!ExpectSeed.IsEmpty())
	{
		Run->Add(TEXT("seed detail"), [this, C, ExpectSeed](auto Next)
		{
			C->GetSeed(ExpectSeed, [this, C, ExpectSeed, Next](const FGFCApiResult& R, const FGFCSeed& S)
			{
				TestTrue(TEXT("seed ok"), R.bOk);
				TestEqual(TEXT("seed id"), S.Id, ExpectSeed);
				// The thumbnail is served by the node and decodes as an image.
				C->DownloadBytes(C->ResolveMediaUrl(S.Thumb), 8 * 1024 * 1024, [this, Next](bool bOk, const TArray<uint8>& Bytes)
				{
					TestTrue(TEXT("thumbnail downloads"), bOk && Bytes.Num() > 0);
					Next();
				});
			});
		});
		Run->Add(TEXT("share info"), [this, C, ExpectSeed](auto Next)
		{
			C->GetShareInfo(ExpectSeed, [this, ExpectSeed, Next](const FGFCApiResult& R, const FGFCShareInfo& S)
			{
				TestTrue(TEXT("share ok"), R.bOk);
				TestTrue(TEXT("node share url is /p/{id}"), S.Url.EndsWith(TEXT("/p/") + ExpectSeed));
				TestTrue(TEXT("x intent"), S.XIntentUrl.StartsWith(TEXT("https://x.com/intent/post?")));
				TestEqual(TEXT("client builds the same page path"), UGFCApiClient::BuildShareUrl(ExpectSeed).Right(ExpectSeed.Len() + 3), TEXT("/p/") + ExpectSeed);
				Next();
			});
		});
	}
	Run->Add(TEXT("export with wrong password"), [this, C, X](auto Next)
	{
		C->ExportWallets(TEXT("wrong-password"), X->Passphrase, [this, C, Next](const FGFCApiResult& R, const FString& Json)
		{
			TestEqual(TEXT("wrong password is 401"), R.StatusCode, 401);
			TestEqual(TEXT("reported as wrong password"), R.Error, TEXT("Wrong password."));
			TestTrue(TEXT("wrong password doesn't log out"), C->IsLoggedIn());
			TestTrue(TEXT("no backup"), Json.IsEmpty());
			Next();
		});
	});
	Run->Add(TEXT("export backup"), [this, C, X](auto Next)
	{
		C->ExportWallets(X->Password, X->Passphrase, [this, X, Next](const FGFCApiResult& R, const FString& Json)
		{
			TestTrue(FString::Printf(TEXT("export ok (%s)"), *R.Error), R.bOk);
			const TSharedPtr<FJsonObject> Obj = FGFCJson::ParseObject(Json);
			TestTrue(TEXT("backup is JSON"), Obj.IsValid());
			if (Obj.IsValid())
			{
				TestEqual(TEXT("kind"), Obj->GetStringField(TEXT("kind")), TEXT("wallets"));
				TestEqual(TEXT("kdf"), Obj->GetStringField(TEXT("kdf")), TEXT("scrypt"));
			}
			// The backup is encrypted: no wallet private key or the password in it.
			TestFalse(TEXT("no private_key field in clear"), Json.Contains(TEXT("private_key")));
			TestFalse(TEXT("password not in backup"), Json.Contains(X->Password));
			Next();
		});
	});
	Run->Add(TEXT("upload a photo"), [this, C, X](auto Next)
	{
		// A 320x200 PNG made here, posted with progress like the Create screen does.
		IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = IW.CreateImageWrapper(EImageFormat::PNG);
		TArray<FColor> Px;
		Px.SetNum(320 * 200);
		for (int32 i = 0; i < Px.Num(); ++i) Px[i] = FColor(uint8(i % 320), uint8(i / 320), 160, 255);
		Png->SetRaw(Px.GetData(), Px.Num() * 4, 320, 200, ERGBFormat::BGRA, 8);
		const TArray64<uint8>& Bytes = Png->GetCompressed();
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/GFCE2E"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = Dir / TEXT("e2e_photo.png");
		FFileHelper::SaveArrayToFile(TArrayView<const uint8>(Bytes.GetData(), int32(Bytes.Num())), *File);

		FGFCUploadRequest Req;
		Req.FilePath = File;
		Req.FileName = TEXT("e2e_photo.png");
		Req.MimeType = TEXT("image/png");
		Req.Title = TEXT("E2E photo");
		Req.Description = TEXT("Posted by the end-to-end test");
		Req.ThumbnailJpeg = FGFCMediaCapture::MakeImageThumbnail(File, 256);
		TestTrue(TEXT("thumbnail made on the device"), Req.ThumbnailJpeg.Num() > 100);
		TSharedRef<int64> MaxSent = MakeShared<int64>(0);
		C->Upload(Req, [MaxSent](int64 Sent, int64) { *MaxSent = FMath::Max(*MaxSent, Sent); },
			[this, C, X, Next, MaxSent, File](const FGFCApiResult& R, const FGFCUploadResult& U)
			{
				X->SeedId = U.Seed.Id;
				TestTrue(FString::Printf(TEXT("upload ok (%s)"), *R.Error), R.bOk);
				TestFalse(TEXT("posted seed has an id"), U.Seed.Id.IsEmpty());
				TestEqual(TEXT("title kept"), U.Seed.Title, TEXT("E2E photo"));
				TestTrue(TEXT("signed with the user's wallet (can earn)"), U.bRewardsEnabled);
				TestTrue(TEXT("the node stored the device thumbnail"), !U.Seed.Thumb.IsEmpty());
				TestTrue(TEXT("progress was reported"), *MaxSent > 0);
				C->GetSeed(U.Seed.Id, [this, Next](const FGFCApiResult& G, const FGFCSeed& S)
				{
					TestTrue(TEXT("posted seed is readable"), G.bOk);
					TestEqual(TEXT("as an image"), S.GetMediaKind(), EGFCMediaKind::Image);
					Next();
				});
			});
	});
	Run->Add(TEXT("phone verification link"), [this, C, Node](auto Next)
	{
		C->GetVerifyStatus([this, C, Node, Next](const FGFCApiResult& R, const FGFCVerifyStatus& S)
		{
			TestTrue(FString::Printf(TEXT("verify status ok (%s)"), *R.Error), R.bOk);
			TestFalse(TEXT("a new account isn't verified"), S.bVerified);
			TestEqual(TEXT("nothing paid before verifying"), S.Rewards(), 0.0);
			C->CreateVerifyLink([this, Node, Next](const FGFCApiResult& L, const FGFCVerifyLink& Link)
			{
				TestTrue(FString::Printf(TEXT("verify link ok (%s)"), *L.Error), L.bOk);
				TestTrue(TEXT("link is the node's verify page"), Link.Url.Contains(TEXT("/verify?c=")));
				Next();
			});
		});
	});
	Run->Add(TEXT("can't report your own post"), [this, C, X](auto Next)
	{
		C->ReportSeed(X->SeedId, TEXT("spam"), [this, Next](const FGFCApiResult& R)
		{
			TestEqual(TEXT("own post refused"), R.StatusCode, 400);
			Next();
		});
	});
	Run->Add(TEXT("another account reports and blocks"), [this, C, X, Node](auto Next)
	{
		UGFCApiClient* B = NewObject<UGFCApiClient>();
		B->AddToRoot();
		B->SetBaseUrl(Node);
		B->TimeoutSeconds = 20.f;
		const FString UserB = X->User + TEXT("b");
		B->Register(UserB, X->Password, TEXT(""), [this, B, X, Next](const FGFCApiResult& R, const FGFCAuthResult&)
		{
			TestTrue(FString::Printf(TEXT("second account (%s)"), *R.Error), R.bOk);
			B->ReportSeed(X->SeedId, TEXT("spam"), [this, B, X, Next](const FGFCApiResult& Rep)
			{
				TestTrue(FString::Printf(TEXT("report ok (%s)"), *Rep.Error), Rep.bOk);
				FGFCSeed S;
				S.Id = X->SeedId;
				S.Owner = X->User;
				TestTrue(TEXT("reported post hidden in the app at once"), B->IsHiddenForMe(S));
				B->BlockUser(X->User, [this, B, X, Next](const FGFCApiResult& Blk)
				{
					TestTrue(FString::Printf(TEXT("block ok (%s)"), *Blk.Error), Blk.bOk);
					B->GetFeed([this, B, X, Next](const FGFCApiResult& F, const TArray<FGFCSeed>& Items)
					{
						TestTrue(TEXT("feed ok"), F.bOk);
						TestFalse(TEXT("the node drops the blocked account's posts"),
							Items.ContainsByPredicate([X](const FGFCSeed& It) { return It.Owner.Equals(X->User, ESearchCase::IgnoreCase); }));
						B->RefreshBlocks([this, B, X, Next](const FGFCApiResult& L)
						{
							TestTrue(TEXT("blocks listed"), L.bOk);
							B->UnblockUser(X->User, [this, B, Next](const FGFCApiResult& U)
							{
								TestTrue(TEXT("unblock ok"), U.bOk);
								B->RemoveFromRoot();
								Next();
							});
						});
					});
				});
			});
		});
	});
	Run->Add(TEXT("logout revokes the token"), [this, C, X](auto Next)
	{
		X->OldToken = C->GetTokenForStorage();
		C->Logout([this, C, X, Next](const FGFCApiResult& R)
		{
			TestTrue(TEXT("logout ok"), R.bOk);
			TestFalse(TEXT("logged out locally"), C->IsLoggedIn());
			C->SetSession(X->User, X->OldToken);
			C->GetMe([this, C, X, Next](const FGFCApiResult& Me, const FGFCProfile&)
			{
				TestEqual(TEXT("revoked token gets 401"), Me.StatusCode, 401);
				TestFalse(TEXT("401 clears the session"), C->IsLoggedIn());
				X->OldToken.Reset();
				Next();
			});
		});
	});
	if (bSaveSession)
	{
		Run->Add(TEXT("save session for UI run"), [this, C, X](auto Next)
		{
			C->Login(X->User, X->Password, [this, C, X, Next](const FGFCApiResult& R, const FGFCAuthResult&)
			{
				TestTrue(TEXT("re-login ok"), R.bOk);
				UGFCSessionSave* Save = Cast<UGFCSessionSave>(UGameplayStatics::CreateSaveGameObject(UGFCSessionSave::StaticClass()));
				Save->Username = C->GetUsername();
				Save->NodeBaseUrl = C->GetBaseUrl();
				TestTrue(TEXT("protect token"), FGFCSecureStore::Protect(C->GetTokenForStorage(), Save->ProtectedToken));
				for (const FString& Id : {X->WalletA, X->WalletB})
				{
					FGFCKnownWallet& W = Save->KnownWallets.AddDefaulted_GetRef();
					W.Owner = C->GetUsername();
					W.NodeBaseUrl = C->GetBaseUrl();
					W.WalletId = Id;
					W.Name = Id == X->WalletA ? TEXT("main") : TEXT("Default Wallet");
				}
				TestTrue(TEXT("session saved"), UGameplayStatics::SaveGameToSlot(Save, UGFCApiSubsystem::SlotName, 0));
				Next();
			});
		});
	}

	const double Start = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Run, C, Start]()
	{
		if (FPlatformTime::Seconds() - Start > 120.0)
		{
			AddError(FString::Printf(TEXT("timed out during step %d"), Run->Index));
			C->RemoveFromRoot();
			return true;
		}
		if (!Run->Tick())
		{
			return false;
		}
		C->RemoveFromRoot();
		return true;
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
