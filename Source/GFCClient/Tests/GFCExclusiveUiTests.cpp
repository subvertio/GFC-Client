// Tests for what GFC Exclusives say on cards and the post screen.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCApiTypes.h"
#include "UI/GFCSeedCard.h"

namespace
{
	constexpr EAutomationTestFlags ExclusiveUiFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCExclusiveUnlockTextTest, "GFC.Exclusive.UnlockText", ExclusiveUiFlags)
bool FGFCExclusiveUnlockTextTest::RunTest(const FString&)
{
	auto Line = [](EGFCExclusiveAccess A, EGFCMediaKind K) { return GFCExclusiveUnlockText(A, K).ToString(); };
	TestEqual(TEXT("logged out, video"), Line(EGFCExclusiveAccess::NeedsLogin, EGFCMediaKind::Video), FString(TEXT("Log in to watch the full clip")));
	TestEqual(TEXT("unverified, video"), Line(EGFCExclusiveAccess::NeedsVerify, EGFCMediaKind::Video),
		FString(TEXT("Verify your phone to watch the full clip")));
	TestEqual(TEXT("logged out, text"), Line(EGFCExclusiveAccess::NeedsLogin, EGFCMediaKind::Text), FString(TEXT("Log in to read the full post")));
	TestEqual(TEXT("unverified, text"), Line(EGFCExclusiveAccess::NeedsVerify, EGFCMediaKind::Text),
		FString(TEXT("Verify your phone to read the full post")));
	TestTrue(TEXT("a video they may watch needs no line"), Line(EGFCExclusiveAccess::Full, EGFCMediaKind::Video).IsEmpty());
	TestTrue(TEXT("a text post they may read points to it"), Line(EGFCExclusiveAccess::Full, EGFCMediaKind::Text).StartsWith(TEXT("Read the full post")));
	return true;
}

#endif
