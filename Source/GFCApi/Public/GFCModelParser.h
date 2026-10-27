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
};

/**
 * Minimal, defensive glTF 2.0 loader for user uploads (.glb, or .gltf with
 * embedded data: URIs). Supports static triangle meshes, the node hierarchy,
 * and metallic-roughness materials with a base colour texture.
 *
 * Not supported (rejected with a clear message): Draco/meshopt compression,
 * Basis textures, sparse accessors, external file URIs. Skins and animations
 * are ignored (the bind pose is shown). See docs/roadmap.md for the plan.
 *
 * Every offset is bounds-checked: these files come from strangers.
 */
struct GFCAPI_API FGFCModelParser
{
	static bool Parse(const TArray<uint8>& Bytes, const FGFCModelBudget& Budget, FGFCModelData& Out, FString& OutError);
};
