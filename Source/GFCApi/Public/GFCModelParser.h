#pragma once

#include "CoreMinimal.h"

/** Limits checked while parsing a model, before anything big is allocated. */
struct GFCAPI_API FGFCModelBudget
{
	int64 MaxFileBytes = 50ll * 1024 * 1024;
	int32 MaxTriangles = 300000;
	int32 MaxImages = 16;
	int32 MaxNodes = 4096;
	int32 MaxDepth = 64;
	int32 MaxJoints = 256;
	/** Keyframes across every channel of every kept animation. */
	int32 MaxAnimationKeys = 200000;
	int32 MaxAnimations = 4;

	/** From UGFCApiSettings. */
	static FGFCModelBudget FromSettings();
};

struct GFCAPI_API FGFCModelSection
{
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;
	TArray<FVector2f> UVs;
	TArray<int32> Indices;
	int32 Material = -1;

	/** Skinned sections only: the skin, and the vertices in glTF mesh space (metres, Y up) with 4 influences each. */
	int32 Skin = -1;
	TArray<FVector3f> SkinPositions;
	TArray<FVector3f> SkinNormals;
	TArray<uint16> SkinJoints; // 4 per vertex, indices into the skin's joints
	TArray<FVector4f> SkinWeights; // normalised
};

/** A node's rest transform, in glTF space. Parent is a node index, or -1. */
struct GFCAPI_API FGFCModelNode
{
	int32 Parent = -1;
	FVector3d Translation = FVector3d::ZeroVector;
	FQuat4d Rotation = FQuat4d::Identity;
	FVector3d Scale = FVector3d::OneVector;
};

struct GFCAPI_API FGFCModelSkin
{
	TArray<int32> JointNodes;
	/** Row-vector form (the transpose of glTF's column-major matrices), like every matrix here. */
	TArray<FMatrix44d> InverseBind;
};

struct GFCAPI_API FGFCAnimationChannel
{
	enum class EPath : uint8 { Translation, Rotation, Scale };
	int32 Node = -1;
	EPath Path = EPath::Translation;
	bool bStep = false;
	TArray<float> Times;
	/** XYZ for translation and scale, XYZW (a quaternion) for rotation. */
	TArray<FVector4d> Values;
};

struct GFCAPI_API FGFCModelAnimation
{
	FString Name;
	float Duration = 0.f;
	TArray<FGFCAnimationChannel> Channels;
};

struct GFCAPI_API FGFCModelMaterial
{
	FLinearColor BaseColor = FLinearColor::White;
	float Metallic = 1.f;
	float Roughness = 1.f;
	/** Index into FGFCModelData::Images, or -1. */
	int32 BaseColorImage = -1;
};

/** A static model in Unreal space (Z up, centimetres), ready for a procedural mesh. */
struct GFCAPI_API FGFCModelData
{
	TArray<FGFCModelSection> Sections;
	TArray<FGFCModelMaterial> Materials;
	/** Encoded PNG/JPEG bytes, decoded (and downscaled to the texture budget) on the game thread. */
	TArray<TArray<uint8>> Images;
	FBox3f Bounds = FBox3f(ForceInit);
	int32 Triangles = 0;

	/** Rig and motion; empty for static models. NodeOrder lists the scene's nodes parents first. */
	TArray<FGFCModelNode> Nodes;
	TArray<int32> NodeOrder;
	TArray<FGFCModelSkin> Skins;
	TArray<FGFCModelAnimation> Animations;

	/** True when there's something to play: an animation and at least one skinned section. */
	bool IsAnimated() const;
};

/**
 * Plays a model's animation on the CPU: node poses at a time, then linear blend skinning.
 * Cheap enough for one model on a phone at 30 fps (the bug model: 86k vertices, 36 joints).
 */
struct GFCAPI_API FGFCModelPose
{
	/** Every node's world matrix (glTF space, row vectors) for Animation at Time (looped). Animation -1 is the rest pose. */
	static void Evaluate(const FGFCModelData& Model, int32 Animation, float Time, TArray<FMatrix44d>& OutGlobals);

	/** One skinned section's vertices and normals in Unreal space (cm, Z up). */
	static void SkinSection(const FGFCModelData& Model, const FGFCModelSection& Section, const TArray<FMatrix44d>& Globals,
		TArray<FVector>& OutPositions, TArray<FVector>& OutNormals);
};

/**
 * Minimal, defensive glTF 2.0 loader for user uploads (.glb, or .gltf with
 * embedded data: URIs). Supports static triangle meshes, the node hierarchy,
 * and metallic-roughness materials with a base colour texture.
 *
 * Skins (JOINTS_0/WEIGHTS_0, 4 influences) and node animations (translation,
 * rotation, scale; linear, step or cubic spline keys) are read so the viewer can
 * play them. The skeleton's root is held in place horizontally, so a walk cycle
 * walks on the spot. Morph targets are ignored.
 *
 * Not supported (rejected with a clear message): Draco/meshopt compression,
 * Basis textures, sparse accessors, external file URIs.
 *
 * Every offset is bounds-checked: these files come from strangers.
 */
struct GFCAPI_API FGFCModelParser
{
	static bool Parse(const TArray<uint8>& Bytes, const FGFCModelBudget& Budget, FGFCModelData& Out, FString& OutError);
};
