#include "GFCModelParser.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GFCApiSettings.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FGFCModelBudget FGFCModelBudget::FromSettings()
{
	const UGFCApiSettings* S = GetDefault<UGFCApiSettings>();
	FGFCModelBudget B;
	B.MaxFileBytes = int64(S->MaxModelMB) * 1024 * 1024;
	B.MaxTriangles = S->MaxModelTriangles;
	return B;
}

namespace
{
	using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

	constexpr uint32 GlbMagic = 0x46546C67; // "glTF"
	constexpr uint32 ChunkJson = 0x4E4F534A;
	constexpr uint32 ChunkBin = 0x004E4942;

	struct FCtx
	{
		TSharedPtr<FJsonObject> Root;
		TArray<TArrayView<const uint8>> Buffers;
		TArray<TArray<uint8>> OwnedBuffers; // decoded data: URIs
		const FGFCModelBudget* Budget = nullptr;
		FString Error;

		const FJsonArray* Array(const TCHAR* Field) const
		{
			const FJsonArray* A = nullptr;
			return Root->TryGetArrayField(Field, A) ? A : nullptr;
		}

		TSharedPtr<FJsonObject> Item(const TCHAR* Field, int32 Index) const
		{
			const FJsonArray* A = Array(Field);
			if (!A || !A->IsValidIndex(Index) || !(*A)[Index].IsValid())
			{
				return nullptr;
			}
			const TSharedPtr<FJsonObject>* O = nullptr;
			return (*A)[Index]->TryGetObject(O) ? *O : nullptr;
		}

		bool Fail(const FString& Message)
		{
			if (Error.IsEmpty()) Error = Message;
			return false;
		}
	};

	int32 IntField(const TSharedPtr<FJsonObject>& O, const TCHAR* F, int32 Default = -1)
	{
		int32 V = Default;
		return O.IsValid() && O->TryGetNumberField(F, V) ? V : Default;
	}

	int32 ComponentSize(int32 Type)
	{
		switch (Type)
		{
		case 5120: case 5121: return 1; // byte, ubyte
		case 5122: case 5123: return 2; // short, ushort
		case 5125: case 5126: return 4; // uint, float
		default: return 0;
		}
	}

	int32 ComponentCount(const FString& Type)
	{
		if (Type == TEXT("SCALAR")) return 1;
		if (Type == TEXT("VEC2")) return 2;
		if (Type == TEXT("VEC3")) return 3;
		if (Type == TEXT("VEC4")) return 4;
		if (Type == TEXT("MAT4")) return 16;
		return 0;
	}

	/** Resolves an accessor to a validated byte range. */
	struct FAccessor
	{
		const uint8* Base = nullptr;
		int32 Count = 0;
		int32 Components = 0;
		int32 ComponentType = 0;
		int32 Stride = 0;
		bool bNormalized = false;

		double Read(int32 Element, int32 Component) const
		{
			const uint8* P = Base + int64(Element) * Stride + int64(Component) * ComponentSize(ComponentType);
			switch (ComponentType)
			{
			case 5126: { float F; FMemory::Memcpy(&F, P, 4); return F; }
			case 5125: { uint32 U; FMemory::Memcpy(&U, P, 4); return U; }
			case 5123: { uint16 U; FMemory::Memcpy(&U, P, 2); return bNormalized ? U / 65535.0 : U; }
			case 5122: { int16 I; FMemory::Memcpy(&I, P, 2); return bNormalized ? FMath::Max(I / 32767.0, -1.0) : I; }
			case 5121: return bNormalized ? *P / 255.0 : *P;
			case 5120: { const int8 I = int8(*P); return bNormalized ? FMath::Max(I / 127.0, -1.0) : I; }
			default: return 0.0;
			}
		}
	};

	bool ResolveBufferView(FCtx& C, int32 ViewIndex, TArrayView<const uint8>& Out, int32& OutStride)
	{
		const TSharedPtr<FJsonObject> View = C.Item(TEXT("bufferViews"), ViewIndex);
		if (!View.IsValid())
		{
			return C.Fail(TEXT("The model references a missing buffer view."));
		}
		const int32 Buffer = IntField(View, TEXT("buffer"));
		const int64 Offset = IntField(View, TEXT("byteOffset"), 0);
		const int64 Length = IntField(View, TEXT("byteLength"));
		OutStride = IntField(View, TEXT("byteStride"), 0);
		if (!C.Buffers.IsValidIndex(Buffer) || Offset < 0 || Length < 0 || Offset + Length > C.Buffers[Buffer].Num() || OutStride < 0 || OutStride > 252)
		{
			return C.Fail(TEXT("The model's buffer views are out of range."));
		}
		Out = C.Buffers[Buffer].Slice(int32(Offset), int32(Length));
		return true;
	}

	bool ResolveAccessor(FCtx& C, int32 Index, FAccessor& Out)
	{
		const TSharedPtr<FJsonObject> A = C.Item(TEXT("accessors"), Index);
		if (!A.IsValid())
		{
			return C.Fail(TEXT("The model references a missing accessor."));
		}
		if (A->HasField(TEXT("sparse")))
		{
			return C.Fail(TEXT("Sparse accessors aren't supported yet."));
		}
		FString Type;
		A->TryGetStringField(TEXT("type"), Type);
		Out.Components = ComponentCount(Type);
		Out.ComponentType = IntField(A, TEXT("componentType"), 0);
		Out.Count = IntField(A, TEXT("count"), 0);
		A->TryGetBoolField(TEXT("normalized"), Out.bNormalized);
		const int32 ElementSize = Out.Components * ComponentSize(Out.ComponentType);
		if (ElementSize <= 0 || Out.Count <= 0)
		{
			return C.Fail(TEXT("The model has an invalid accessor."));
		}
		TArrayView<const uint8> View;
		int32 Stride = 0;
		if (!ResolveBufferView(C, IntField(A, TEXT("bufferView")), View, Stride))
		{
			return false;
		}
		Out.Stride = Stride > 0 ? Stride : ElementSize;
		const int64 Offset = IntField(A, TEXT("byteOffset"), 0);
		const int64 Needed = Offset + int64(Out.Count - 1) * Out.Stride + ElementSize;
		if (Offset < 0 || Out.Stride < ElementSize || Needed > View.Num())
		{
			return C.Fail(TEXT("The model's accessors read past their data."));
		}
		Out.Base = View.GetData() + Offset;
		return true;
	}

	FMatrix44d NodeMatrix(const TSharedPtr<FJsonObject>& Node)
	{
		const FJsonArray* M = nullptr;
		if (Node->TryGetArrayField(TEXT("matrix"), M) && M->Num() == 16)
		{
			FMatrix44d Out;
			for (int32 i = 0; i < 16; ++i)
			{
				// glTF is column-major with column vectors; UE matrices are row vectors, so this is the transpose.
				Out.M[i / 4][i % 4] = (*M)[i]->AsNumber();
			}
			return Out;
		}
		auto Vec = [&](const TCHAR* F, int32 N, const TArray<double>& Def)
		{
			TArray<double> V = Def;
			const FJsonArray* A = nullptr;
			if (Node->TryGetArrayField(F, A) && A->Num() == N)
			{
				for (int32 i = 0; i < N; ++i) V[i] = (*A)[i]->AsNumber();
			}
			return V;
		};
		const TArray<double> T = Vec(TEXT("translation"), 3, {0, 0, 0});
		const TArray<double> R = Vec(TEXT("rotation"), 4, {0, 0, 0, 1});
		const TArray<double> S = Vec(TEXT("scale"), 3, {1, 1, 1});
		const FQuat4d Q(R[0], R[1], R[2], R[3]);
		const FTransform3d X(Q.GetNormalized(), FVector3d(T[0], T[1], T[2]), FVector3d(S[0], S[1], S[2]));
		return X.ToMatrixWithScale();
	}

	/** glTF (right-handed, Y up, metres) to Unreal (left-handed, Z up, cm). */
	FVector3f ToUnreal(const FVector3d& P, double Scale)
	{
		return FVector3f(float(-P.Z * Scale), float(P.X * Scale), float(P.Y * Scale));
	}

	bool CountTriangles(FCtx& C, const TSharedPtr<FJsonObject>& Mesh, int64& InOutTriangles)
	{
		const FJsonArray* Prims = nullptr;
		if (!Mesh->TryGetArrayField(TEXT("primitives"), Prims))
		{
			return true;
		}
		for (const TSharedPtr<FJsonValue>& PV : *Prims)
		{
			const TSharedPtr<FJsonObject>* P = nullptr;
			if (!PV.IsValid() || !PV->TryGetObject(P) || IntField(*P, TEXT("mode"), 4) != 4)
			{
				continue;
			}
			const int32 IndicesAcc = IntField(*P, TEXT("indices"));
			int32 Count = 0;
			if (IndicesAcc >= 0)
			{
				Count = IntField(C.Item(TEXT("accessors"), IndicesAcc), TEXT("count"), 0);
			}
			else
			{
				const TSharedPtr<FJsonObject>* Attr = nullptr;
				if ((*P)->TryGetObjectField(TEXT("attributes"), Attr))
				{
					Count = IntField(C.Item(TEXT("accessors"), IntField(*Attr, TEXT("POSITION"))), TEXT("count"), 0);
				}
			}
			InOutTriangles += FMath::Max(0, Count) / 3;
		}
		return true;
	}

	bool AddPrimitive(FCtx& C, const TSharedPtr<FJsonObject>& Prim, const FMatrix44d& World, FGFCModelData& Out)
	{
		if (IntField(Prim, TEXT("mode"), 4) != 4)
		{
			return true; // points/lines: skipped
		}
		const TSharedPtr<FJsonObject>* Attr = nullptr;
		if (!Prim->TryGetObjectField(TEXT("attributes"), Attr))
		{
			return true;
		}
		FAccessor Pos;
		if (!ResolveAccessor(C, IntField(*Attr, TEXT("POSITION")), Pos) || Pos.Components != 3)
		{
			return C.Fail(TEXT("A mesh has no valid positions."));
		}
		FGFCModelSection& S = Out.Sections.AddDefaulted_GetRef();
		S.Material = IntField(Prim, TEXT("material"));
		const double Scale = 100.0;
		S.Positions.SetNumUninitialized(Pos.Count);
		for (int32 i = 0; i < Pos.Count; ++i)
		{
			const FVector3d L(Pos.Read(i, 0), Pos.Read(i, 1), Pos.Read(i, 2));
			S.Positions[i] = ToUnreal(World.TransformPosition(L), Scale);
			Out.Bounds += S.Positions[i];
		}

		FAccessor Nrm;
		const int32 NrmIndex = IntField(*Attr, TEXT("NORMAL"));
		if (NrmIndex >= 0 && ResolveAccessor(C, NrmIndex, Nrm) && Nrm.Components == 3 && Nrm.Count == Pos.Count)
		{
			S.Normals.SetNumUninitialized(Nrm.Count);
			for (int32 i = 0; i < Nrm.Count; ++i)
			{
				const FVector3d L(Nrm.Read(i, 0), Nrm.Read(i, 1), Nrm.Read(i, 2));
				S.Normals[i] = ToUnreal(World.TransformVector(L), 1.0).GetSafeNormal();
			}
		}
		C.Error.Reset(); // a bad optional attribute isn't fatal

		FAccessor UV;
		const int32 UVIndex = IntField(*Attr, TEXT("TEXCOORD_0"));
		if (UVIndex >= 0 && ResolveAccessor(C, UVIndex, UV) && UV.Components == 2 && UV.Count == Pos.Count)
		{
			S.UVs.SetNumUninitialized(UV.Count);
			for (int32 i = 0; i < UV.Count; ++i)
			{
				S.UVs[i] = FVector2f(float(UV.Read(i, 0)), float(UV.Read(i, 1)));
			}
		}
		C.Error.Reset();

		const int32 IdxAcc = IntField(Prim, TEXT("indices"));
		if (IdxAcc >= 0)
		{
			FAccessor Idx;
			if (!ResolveAccessor(C, IdxAcc, Idx) || Idx.Components != 1)
			{
				return C.Fail(TEXT("A mesh has invalid indices."));
			}
			S.Indices.SetNumUninitialized(Idx.Count - Idx.Count % 3);
			for (int32 i = 0; i < S.Indices.Num(); ++i)
			{
				const double V = Idx.Read(i, 0);
				if (V < 0 || V >= Pos.Count)
				{
					return C.Fail(TEXT("A mesh has indices out of range."));
				}
				S.Indices[i] = int32(V);
			}
		}
		else
		{
			S.Indices.SetNumUninitialized(Pos.Count - Pos.Count % 3);
			for (int32 i = 0; i < S.Indices.Num(); ++i) S.Indices[i] = i;
		}
		Out.Triangles += S.Indices.Num() / 3;

		if (S.Normals.Num() != S.Positions.Num())
		{
			// Smooth normals from faces.
			S.Normals.Init(FVector3f::ZeroVector, S.Positions.Num());
			for (int32 i = 0; i + 2 < S.Indices.Num(); i += 3)
			{
				const FVector3f& A = S.Positions[S.Indices[i]];
				const FVector3f N = FVector3f::CrossProduct(S.Positions[S.Indices[i + 1]] - A, S.Positions[S.Indices[i + 2]] - A);
				for (int32 k = 0; k < 3; ++k) S.Normals[S.Indices[i + k]] += N;
			}
			for (FVector3f& N : S.Normals) N = N.GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
		}
		return true;
	}

	bool VisitNode(FCtx& C, int32 NodeIndex, const FMatrix44d& Parent, int32 Depth, TSet<int32>& Visited, FGFCModelData& Out)
	{
		if (Depth > C.Budget->MaxDepth || Visited.Num() > C.Budget->MaxNodes)
		{
			return C.Fail(TEXT("The model's node tree is too deep or too large."));
		}
		if (Visited.Contains(NodeIndex))
		{
			return C.Fail(TEXT("The model's node tree has a cycle."));
		}
		Visited.Add(NodeIndex);
		const TSharedPtr<FJsonObject> Node = C.Item(TEXT("nodes"), NodeIndex);
		if (!Node.IsValid())
		{
			return C.Fail(TEXT("The model references a missing node."));
		}
		const FMatrix44d World = NodeMatrix(Node) * Parent;
		const int32 MeshIndex = IntField(Node, TEXT("mesh"));
		if (MeshIndex >= 0)
		{
			const TSharedPtr<FJsonObject> Mesh = C.Item(TEXT("meshes"), MeshIndex);
			const FJsonArray* Prims = nullptr;
			if (Mesh.IsValid() && Mesh->TryGetArrayField(TEXT("primitives"), Prims))
			{
				for (const TSharedPtr<FJsonValue>& PV : *Prims)
				{
					const TSharedPtr<FJsonObject>* P = nullptr;
					if (PV.IsValid() && PV->TryGetObject(P) && !AddPrimitive(C, *P, World, Out))
					{
						return false;
					}
				}
			}
		}
		const FJsonArray* Children = nullptr;
		if (Node->TryGetArrayField(TEXT("children"), Children))
		{
			for (const TSharedPtr<FJsonValue>& Child : *Children)
			{
				if (!VisitNode(C, int32(Child->AsNumber()), World, Depth + 1, Visited, Out))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool DecodeDataUri(const FString& Uri, TArray<uint8>& Out)
	{
		FString Header, Payload;
		if (!Uri.StartsWith(TEXT("data:")) || !Uri.Split(TEXT(","), &Header, &Payload) || !Header.EndsWith(TEXT(";base64")))
		{
			return false;
		}
		return FBase64::Decode(Payload, Out);
	}
}

bool FGFCModelParser::Parse(const TArray<uint8>& Bytes, const FGFCModelBudget& Budget, FGFCModelData& Out, FString& OutError)
{
	Out = FGFCModelData();
	FCtx C;
	C.Budget = &Budget;
	auto Fail = [&](const FString& M) { OutError = M; return false; };

	if (Bytes.Num() > Budget.MaxFileBytes)
	{
		return Fail(FString::Printf(TEXT("The model is larger than %lld MB."), Budget.MaxFileBytes / (1024 * 1024)));
	}

	// ---- container ----
	FString JsonText;
	TArrayView<const uint8> BinChunk;
	uint32 Magic = 0;
	if (Bytes.Num() >= 12)
	{
		FMemory::Memcpy(&Magic, Bytes.GetData(), 4);
	}
	if (Magic == GlbMagic)
	{
		uint32 Version = 0, Length = 0;
		FMemory::Memcpy(&Version, Bytes.GetData() + 4, 4);
		FMemory::Memcpy(&Length, Bytes.GetData() + 8, 4);
		if (Version != 2 || Length > uint32(Bytes.Num()))
		{
			return Fail(TEXT("Only glTF 2.0 files are supported."));
		}
		int64 Cursor = 12;
		while (Cursor + 8 <= Length)
		{
			uint32 ChunkLen = 0, ChunkType = 0;
			FMemory::Memcpy(&ChunkLen, Bytes.GetData() + Cursor, 4);
			FMemory::Memcpy(&ChunkType, Bytes.GetData() + Cursor + 4, 4);
			const int64 Start = Cursor + 8;
			if (Start + ChunkLen > Length)
			{
				return Fail(TEXT("The .glb file is truncated."));
			}
			if (ChunkType == ChunkJson && JsonText.IsEmpty())
			{
				FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Start), int32(ChunkLen));
				JsonText = FString(Conv.Length(), Conv.Get());
			}
			else if (ChunkType == ChunkBin && BinChunk.Num() == 0)
			{
				BinChunk = TArrayView<const uint8>(Bytes.GetData() + Start, int32(ChunkLen));
			}
			Cursor = Start + ((ChunkLen + 3) & ~3u);
		}
	}
	else if (Bytes.Num() > 0 && (Bytes[0] == '{' || (Bytes.Num() > 3 && Bytes[0] == 0xEF && Bytes[3] == '{')))
	{
		const int32 Skip = Bytes[0] == 0xEF ? 3 : 0;
		FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Skip), Bytes.Num() - Skip);
		JsonText = FString(Conv.Length(), Conv.Get());
	}
	else
	{
		return Fail(TEXT("This isn't a glTF or GLB file."));
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, C.Root) || !C.Root.IsValid())
	{
		return Fail(TEXT("The model's JSON is invalid."));
	}

	// ---- unsupported features ----
	const FJsonArray* Required = nullptr;
	if (C.Root->TryGetArrayField(TEXT("extensionsRequired"), Required) && Required->Num() > 0)
	{
		TArray<FString> Names;
		for (const TSharedPtr<FJsonValue>& V : *Required) Names.Add(V->AsString());
		return Fail(FString::Printf(TEXT("This model needs features the viewer doesn't support yet: %s."), *FString::Join(Names, TEXT(", "))));
	}

	// ---- buffers ----
	if (const FJsonArray* Buffers = C.Array(TEXT("buffers")))
	{
		C.OwnedBuffers.SetNum(Buffers->Num());
		for (int32 i = 0; i < Buffers->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> B = C.Item(TEXT("buffers"), i);
			FString Uri;
			if (!B.IsValid() || !B->TryGetStringField(TEXT("uri"), Uri))
			{
				if (i != 0 || BinChunk.Num() == 0)
				{
					return Fail(TEXT("The model's binary data is missing."));
				}
				C.Buffers.Add(BinChunk);
				continue;
			}
			if (!DecodeDataUri(Uri, C.OwnedBuffers[i]))
			{
				return Fail(TEXT("The model refers to external files. Upload it as a single .glb."));
			}
			C.Buffers.Add(TArrayView<const uint8>(C.OwnedBuffers[i]));
		}
	}

	// ---- scene ----
	TArray<int32> RootNodes;
	int32 SceneIndex = IntField(C.Root, TEXT("scene"), 0);
	const TSharedPtr<FJsonObject> Scene = C.Item(TEXT("scenes"), SceneIndex);
	const FJsonArray* SceneNodes = nullptr;
	if (Scene.IsValid() && Scene->TryGetArrayField(TEXT("nodes"), SceneNodes))
	{
		for (const TSharedPtr<FJsonValue>& N : *SceneNodes) RootNodes.Add(int32(N->AsNumber()));
	}
	else if (const FJsonArray* Nodes = C.Array(TEXT("nodes")))
	{
		for (int32 i = 0; i < Nodes->Num(); ++i) RootNodes.Add(i); // no scene: treat every node as a root
	}
	if (const FJsonArray* Nodes = C.Array(TEXT("nodes")); Nodes && Nodes->Num() > Budget.MaxNodes)
	{
		return Fail(TEXT("The model has too many nodes."));
	}

	// ---- triangle budget before reading any vertices ----
	int64 Triangles = 0;
	{
		TArray<int32> Stack = RootNodes;
		TSet<int32> Seen;
		while (Stack.Num() > 0)
		{
			const int32 N = Stack.Pop();
			if (Seen.Contains(N) || Seen.Num() > Budget.MaxNodes)
			{
				continue;
			}
			Seen.Add(N);
			const TSharedPtr<FJsonObject> Node = C.Item(TEXT("nodes"), N);
			if (!Node.IsValid())
			{
				continue;
			}
			if (const TSharedPtr<FJsonObject> Mesh = C.Item(TEXT("meshes"), IntField(Node, TEXT("mesh"))))
			{
				CountTriangles(C, Mesh, Triangles);
			}
			const FJsonArray* Children = nullptr;
			if (Node->TryGetArrayField(TEXT("children"), Children))
			{
				for (const TSharedPtr<FJsonValue>& Ch : *Children) Stack.Add(int32(Ch->AsNumber()));
			}
		}
	}
	if (Triangles > Budget.MaxTriangles)
	{
		return Fail(FString::Printf(TEXT("The model has %lld triangles; the limit on phones is %d."), Triangles, Budget.MaxTriangles));
	}

	// ---- geometry ----
	TSet<int32> Visited;
	for (const int32 N : RootNodes)
	{
		if (!VisitNode(C, N, FMatrix44d::Identity, 0, Visited, Out))
		{
			return Fail(C.Error);
		}
	}
	if (Out.Sections.Num() == 0 || Out.Triangles == 0)
	{
		return Fail(TEXT("The model has no triangles to show."));
	}

	// ---- materials and images ----
	TMap<int32, int32> ImageRemap;
	if (const FJsonArray* Materials = C.Array(TEXT("materials")))
	{
		for (int32 i = 0; i < Materials->Num(); ++i)
		{
			FGFCModelMaterial& M = Out.Materials.AddDefaulted_GetRef();
			const TSharedPtr<FJsonObject> Mat = C.Item(TEXT("materials"), i);
			const TSharedPtr<FJsonObject>* Pbr = nullptr;
			if (!Mat.IsValid() || !Mat->TryGetObjectField(TEXT("pbrMetallicRoughness"), Pbr))
			{
				continue;
			}
			const FJsonArray* Factor = nullptr;
			if ((*Pbr)->TryGetArrayField(TEXT("baseColorFactor"), Factor) && Factor->Num() == 4)
			{
				M.BaseColor = FLinearColor(float((*Factor)[0]->AsNumber()), float((*Factor)[1]->AsNumber()),
					float((*Factor)[2]->AsNumber()), float((*Factor)[3]->AsNumber()));
			}
			double D;
			if ((*Pbr)->TryGetNumberField(TEXT("metallicFactor"), D)) M.Metallic = float(FMath::Clamp(D, 0.0, 1.0));
			if ((*Pbr)->TryGetNumberField(TEXT("roughnessFactor"), D)) M.Roughness = float(FMath::Clamp(D, 0.0, 1.0));

			const TSharedPtr<FJsonObject>* TexRef = nullptr;
			if (!(*Pbr)->TryGetObjectField(TEXT("baseColorTexture"), TexRef))
			{
				continue;
			}
			const int32 ImageIndex = IntField(C.Item(TEXT("textures"), IntField(*TexRef, TEXT("index"))), TEXT("source"));
			if (const int32* Existing = ImageRemap.Find(ImageIndex))
			{
				M.BaseColorImage = *Existing;
				continue;
			}
			const TSharedPtr<FJsonObject> Image = C.Item(TEXT("images"), ImageIndex);
			if (!Image.IsValid() || Out.Images.Num() >= Budget.MaxImages)
			{
				continue; // missing or over budget: the material keeps its colour
			}
			TArray<uint8> Encoded;
			FString Uri;
			if (Image->TryGetStringField(TEXT("uri"), Uri))
			{
				if (!DecodeDataUri(Uri, Encoded))
				{
					continue; // external image file: ignored, never fetched
				}
			}
			else
			{
				TArrayView<const uint8> View;
				int32 Stride;
				if (!ResolveBufferView(C, IntField(Image, TEXT("bufferView")), View, Stride))
				{
					C.Error.Reset();
					continue;
				}
				Encoded = TArray<uint8>(View.GetData(), View.Num());
			}
			M.BaseColorImage = Out.Images.Add(MoveTemp(Encoded));
			ImageRemap.Add(ImageIndex, M.BaseColorImage);
		}
	}
	for (FGFCModelSection& S : Out.Sections)
	{
		if (!Out.Materials.IsValidIndex(S.Material))
		{
			S.Material = -1;
		}
	}
	return true;
}
