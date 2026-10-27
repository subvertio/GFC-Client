// Tests for skinned, animated glTF models: parsing the rig, posing, skinning, and walking in place.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GFCModelParser.h"
#include "Misc/Base64.h"

namespace
{
	constexpr EAutomationTestFlags AnimFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::EngineFilter;

	void PutF(TArray<uint8>& B, std::initializer_list<float> Values)
	{
		for (float V : Values) B.Append(reinterpret_cast<const uint8*>(&V), 4);
	}

	/**
	 * Two joints in a chain (the second 1 m above the first) and one triangle bound to the second.
	 * The animation turns joint 1 by 90 degrees about Y at t=1 and moves the root (joint 0)
	 * from (0,0,0) to (5,0.5,7) over 2 s; in place, only its height should change.
	 */
	FString MakeSkinnedGltf()
	{
		TArray<uint8> B;
		PutF(B, {1, 1, 0, 1.2f, 1, 0, 1, 1.2f, 0});                            // 0: positions (36)
		B.Append({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0});                       // 36: joints (12)
		for (int32 i = 0; i < 3; ++i) PutF(B, {1, 0, 0, 0});                  // 48: weights (48)
		PutF(B, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});            // 96: inverse bind, joint 0
		PutF(B, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1});           //     joint 1 (column-major)
		PutF(B, {0, 1, 2});                                                    // 224: times (12)
		const float S = FMath::Sin(PI / 4.f), C = FMath::Cos(PI / 4.f);
		PutF(B, {0, 0, 0, 1, 0, S, 0, C, 0, 0, 0, 1});                         // 236: rotations (48)
		PutF(B, {0, 2});                                                       // 284: times (8)
		PutF(B, {0, 0, 0, 5, 0.5f, 7});                                        // 292: translations (24)
		check(B.Num() == 316);

		const FString Data = FBase64::Encode(B);
		return FString::Printf(TEXT(R"({
"asset":{"version":"2.0"},
"scene":0,"scenes":[{"nodes":[0,2]}],
"nodes":[{"children":[1]},{"translation":[0,1,0]},{"mesh":0,"skin":0}],
"skins":[{"joints":[0,1],"inverseBindMatrices":3}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2}}]}],
"animations":[{"name":"walk","channels":[
  {"sampler":0,"target":{"node":1,"path":"rotation"}},
  {"sampler":1,"target":{"node":0,"path":"translation"}}],
 "samplers":[{"input":4,"output":5},{"input":6,"output":7}]}],
"buffers":[{"byteLength":316,"uri":"data:application/octet-stream;base64,%s"}],
"bufferViews":[
  {"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":12},
  {"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":128},
  {"buffer":0,"byteOffset":224,"byteLength":12},{"buffer":0,"byteOffset":236,"byteLength":48},
  {"buffer":0,"byteOffset":284,"byteLength":8},{"buffer":0,"byteOffset":292,"byteLength":24}],
"accessors":[
  {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
  {"bufferView":1,"componentType":5121,"count":3,"type":"VEC4"},
  {"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},
  {"bufferView":3,"componentType":5126,"count":2,"type":"MAT4"},
  {"bufferView":4,"componentType":5126,"count":3,"type":"SCALAR"},
  {"bufferView":5,"componentType":5126,"count":3,"type":"VEC4"},
  {"bufferView":6,"componentType":5126,"count":2,"type":"SCALAR"},
  {"bufferView":7,"componentType":5126,"count":2,"type":"VEC3"}]
})"), *Data);
	}

	bool Parse(const FString& Json, FGFCModelData& Out, FString& Error)
	{
		FTCHARToUTF8 Utf8(*Json);
		TArray<uint8> Bytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return FGFCModelParser::Parse(Bytes, FGFCModelBudget(), Out, Error);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCSkinnedParseTest, "GFC.Mobile.AnimatedModel.Parse", AnimFlags)
bool FGFCSkinnedParseTest::RunTest(const FString&)
{
	FGFCModelData M;
	FString Error;
	if (!TestTrue(TEXT("parses: ") + Error, Parse(MakeSkinnedGltf(), M, Error)))
	{
		return false;
	}
	TestTrue(TEXT("animated"), M.IsAnimated());
	TestEqual(TEXT("one skin, two joints"), M.Skins.Num() == 1 ? M.Skins[0].JointNodes.Num() : 0, 2);
	TestEqual(TEXT("the section is skinned"), M.Sections[0].Skin, 0);
	TestEqual(TEXT("animation name"), M.Animations[0].Name, FString(TEXT("walk")));
	TestEqual(TEXT("duration"), M.Animations[0].Duration, 2.f);
	TestEqual(TEXT("parents first"), M.NodeOrder.IndexOfByKey(0) < M.NodeOrder.IndexOfByKey(1), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCSkinningTest, "GFC.Mobile.AnimatedModel.SkinsInPlace", AnimFlags)
bool FGFCSkinningTest::RunTest(const FString&)
{
	FGFCModelData M;
	FString Error;
	if (!Parse(MakeSkinnedGltf(), M, Error))
	{
		AddError(Error);
		return false;
	}
	TArray<FMatrix44d> Globals;
	TArray<FVector> P, N;

	// Rest pose: the vertex (1,1,0) m is at Unreal (0,100,100) cm.
	FGFCModelPose::Evaluate(M, -1, 0.f, Globals);
	FGFCModelPose::SkinSection(M, M.Sections[0], Globals, P, N);
	TestTrue(TEXT("rest pose"), P[0].Equals(FVector(0, 100, 100), 0.01));

	// t=1: joint 1 has turned 90 degrees about Y, and the root has risen 0.25 m but not travelled.
	FGFCModelPose::Evaluate(M, 0, 1.f, Globals);
	FGFCModelPose::SkinSection(M, M.Sections[0], Globals, P, N);
	TestTrue(FString::Printf(TEXT("posed and in place (got %s)"), *P[0].ToString()), P[0].Equals(FVector(100, 0, 125), 0.05));

	// Looping: 2.5 s is 0.5 s into the cycle.
	TArray<FMatrix44d> A, B;
	FGFCModelPose::Evaluate(M, 0, 2.5f, A);
	FGFCModelPose::Evaluate(M, 0, 0.5f, B);
	TestTrue(TEXT("loops"), A[1].Equals(B[1], 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGFCBadSkinTest, "GFC.Mobile.AnimatedModel.RejectsBadJoints", AnimFlags)
bool FGFCBadSkinTest::RunTest(const FString&)
{
	// A skin that names a node outside the scene is refused, not trusted.
	FString Json = MakeSkinnedGltf().Replace(TEXT("\"joints\":[0,1]"), TEXT("\"joints\":[0,7]"));
	FGFCModelData M;
	FString Error;
	TestFalse(TEXT("refused"), Parse(Json, M, Error));
	TestFalse(TEXT("with a reason"), Error.IsEmpty());
	return true;
}

#endif
