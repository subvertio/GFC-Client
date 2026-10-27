#include "Viewer/GFCModelViewer.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GFCCacheSubsystem.h"
#include "GFCModelParser.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

const TCHAR* AGFCModelViewer::MaterialPath = TEXT("/Game/GFC/M_GFCModel.M_GFCModel");

namespace
{
	// Far from anything else in the world; only the model is rendered anyway.
	const FVector StageLocation(0.f, 0.f, -500000.f);
	constexpr float FovDegrees = 40.f;
}

AGFCModelViewer::AGFCModelViewer()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCastShadow(false);

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(GetRootComponent());
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = false;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->FOVAngle = FovDegrees;
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetBloom(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetEyeAdaptation(false);
	Capture->ShowFlags.SetAmbientOcclusion(false);
	Capture->ShowFlags.SetDynamicShadows(false);
	// Auto exposure is off project-wide; give the capture a fixed, bright-enough exposure.
	Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Capture->PostProcessSettings.AutoExposureBias = 1.6f;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Capture->PostProcessBlendWeight = 1.f;

	// Three-point lighting riding with the camera. The mobile renderer takes one directional
	// light, so the key is directional and the fill and rim are point lights (placed per model).
	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Capture);
	KeyLight->SetRelativeRotation(FRotator(-30.f, 35.f, 0.f)); // from above, camera left
	KeyLight->SetIntensity(5.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.88f)); // warm
	KeyLight->SetCastShadows(false);
	KeyLight->SetMobility(EComponentMobility::Movable);

	auto MakePoint = [this](const TCHAR* Name, const FLinearColor& Color)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(Capture);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetLightColor(Color);
		L->SetCastShadows(false);
		L->SetMobility(EComponentMobility::Movable);
		L->bUseInverseSquaredFalloff = true;
		return L;
	};
	FillLight = MakePoint(TEXT("FillLight"), FLinearColor(0.78f, 0.86f, 1.f)); // cool, softer, camera right
	RimLight = MakePoint(TEXT("RimLight"), FLinearColor(1.f, 1.f, 1.f));     // behind and above: edges glow
}

void AGFCModelViewer::UpdateLights()
{
	// Placed in camera space around the model and scaled with its size, so a 2 cm ring and
	// a 20 m ship get the same look. Candelas = lux * metres^2 at the model.
	const float D = Distance, R = Radius;
	auto Place = [&](UPointLightComponent* L, const FVector& CameraSpace, float Lux)
	{
		const float Metres = CameraSpace.Size() / 100.f;
		L->SetRelativeLocation(CameraSpace);
		L->SetIntensity(Lux * Metres * Metres);
		L->SetAttenuationRadius((CameraSpace.Size() + R) * 3.f);
	};
	// Camera space: X forward (towards the model), Y right, Z up. The model sits at X = D.
	Place(FillLight, FVector(D * 0.3f, D * 0.8f, -R * 0.2f), 1.6f);
	Place(RimLight, FVector(D + R * 2.2f, -R * 0.8f, R * 1.6f), 4.f);
}

UMaterialInterface* AGFCModelViewer::BaseMaterial() const
{
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, MaterialPath))
	{
		return M;
	}
	// Fallback without our material: untextured, but still a lit, coloured shape.
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

void AGFCModelViewer::ShowModel(const FGFCModelData& Model, int32 MaxTextureSize, int32 RenderSize)
{
	SetActorLocation(StageLocation);

	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->InitAutoFormat(RenderSize, RenderSize);
	RenderTarget->UpdateResourceImmediate(true);
	Capture->TextureTarget = RenderTarget;
	Capture->ShowOnlyComponents.Reset();
	Capture->ShowOnlyComponents.Add(Mesh);

	// Textures, decoded once each and downscaled to the phone budget.
	Textures.Reset();
	for (const TArray<uint8>& Encoded : Model.Images)
	{
		Textures.Add(UGFCCacheSubsystem::DecodeTexture(Encoded, MaxTextureSize));
	}

	UMaterialInterface* Base = BaseMaterial();
	Materials.Reset();
	for (const FGFCModelMaterial& Src : Model.Materials)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("BaseColorFactor"), Src.BaseColor);
		MID->SetVectorParameterValue(TEXT("Color"), Src.BaseColor); // fallback material's parameter
		MID->SetScalarParameterValue(TEXT("Metallic"), Src.Metallic);
		MID->SetScalarParameterValue(TEXT("Roughness"), Src.Roughness);
		if (Textures.IsValidIndex(Src.BaseColorImage) && Textures[Src.BaseColorImage])
		{
			MID->SetTextureParameterValue(TEXT("BaseColorTexture"), Textures[Src.BaseColorImage]);
		}
		Materials.Add(MID);
	}
	UMaterialInstanceDynamic* DefaultMID = UMaterialInstanceDynamic::Create(Base, this);
	DefaultMID->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(0.7f, 0.72f, 0.78f));
	DefaultMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.7f, 0.72f, 0.78f));

	Mesh->ClearAllMeshSections();
	for (int32 i = 0; i < Model.Sections.Num(); ++i)
	{
		const FGFCModelSection& S = Model.Sections[i];
		TArray<FVector> Positions;
		Positions.Reserve(S.Positions.Num());
		for (const FVector3f& P : S.Positions) Positions.Add(FVector(P));
		TArray<FVector> Normals;
		Normals.Reserve(S.Normals.Num());
		for (const FVector3f& N : S.Normals) Normals.Add(FVector(N));
		TArray<FVector2D> UVs;
		UVs.Reserve(S.UVs.Num());
		for (const FVector2f& U : S.UVs) UVs.Add(FVector2D(U));

		Mesh->CreateMeshSection_LinearColor(i, Positions, S.Indices, Normals, UVs, {}, {}, false);
		Mesh->SetMaterial(i, Materials.IsValidIndex(S.Material) ? Materials[S.Material].Get() : DefaultMID);
	}

	// Rough memory estimate for the debug overlay: vertex buffers, indices, textures (+mips), render target.
	TriangleCount = Model.Triangles;
	EstimatedBytes = int64(RenderSize) * RenderSize * 4;
	for (const FGFCModelSection& S : Model.Sections)
	{
		EstimatedBytes += int64(S.Positions.Num()) * (12 + 12 + 8 + 16) + int64(S.Indices.Num()) * 4;
	}
	for (const UTexture2D* Tex : Textures)
	{
		EstimatedBytes += Tex ? int64(Tex->GetSizeX()) * Tex->GetSizeY() * 4 * 4 / 3 : 0;
	}

	Center = FVector(Model.Bounds.GetCenter());
	Radius = FMath::Max(1.f, float(Model.Bounds.GetExtent().Size()));
	// The mesh's render proxy appears a frame or two later: keep capturing briefly.
	WarmupCaptures = 20;

	Animated.Reset();
	AnimTime = 0.f;
	if (Model.IsAnimated())
	{
		// Keep what skinning needs (not the images: those are textures now).
		Animated = MakeShared<FGFCModelData>();
		Animated->Sections = Model.Sections;
		Animated->Nodes = Model.Nodes;
		Animated->NodeOrder = Model.NodeOrder;
		Animated->Skins = Model.Skins;
		Animated->Animations = Model.Animations;
		UpdateAnimation();
	}
	ResetView();
}
void AGFCModelViewer::UpdateAnimation()
{
	FGFCModelPose::Evaluate(*Animated, 0, AnimTime, PoseScratch);
	TArray<FVector> Positions, Normals;
	for (int32 i = 0; i < Animated->Sections.Num(); ++i)
	{
		const FGFCModelSection& S = Animated->Sections[i];
		if (S.Skin < 0)
		{
			continue;
		}
		FGFCModelPose::SkinSection(*Animated, S, PoseScratch, Positions, Normals);
		Mesh->UpdateMeshSection_LinearColor(i, Positions, Normals, {}, {}, {});
	}
	bDirty = true;
}

void AGFCModelViewer::ResetView()
{
	Yaw = 35.f;
	Pitch = -20.f;
	PanOffset = FVector::ZeroVector;
	// Radius is the box's half-diagonal, which overstates most shapes: 0.85 fills the frame without clipping.
	Distance = Radius / FMath::Tan(FMath::DegreesToRadians(FovDegrees * 0.5f)) * 0.85f;
	IdleSeconds = AutoSpinResumeSeconds; // turn straight away
	UpdateCamera();
}

void AGFCModelViewer::SetHeld(bool bInHeld)
{
	bHeld = bInHeld;
	NoteInteraction();
}

void AGFCModelViewer::Orbit(float DeltaYawDegrees, float DeltaPitchDegrees)
{
	Yaw += DeltaYawDegrees;
	Pitch = FMath::Clamp(Pitch + DeltaPitchDegrees, -85.f, 85.f);
	NoteInteraction();
	UpdateCamera();
}

void AGFCModelViewer::Zoom(float Factor)
{
	const float Fit = Radius / FMath::Tan(FMath::DegreesToRadians(FovDegrees * 0.5f));
	Distance = FMath::Clamp(Distance * Factor, Fit * 0.15f, Fit * 6.f);
	NoteInteraction();
	UpdateCamera();
}

void AGFCModelViewer::Pan(float DeltaX, float DeltaY)
{
	const FRotator Rot(Pitch, Yaw, 0.f);
	const float ViewSize = 2.f * Distance * FMath::Tan(FMath::DegreesToRadians(FovDegrees * 0.5f));
	PanOffset += (-FRotationMatrix(Rot).GetUnitAxis(EAxis::Y) * DeltaX + FRotationMatrix(Rot).GetUnitAxis(EAxis::Z) * DeltaY) * ViewSize;
	PanOffset = PanOffset.GetClampedToMaxSize(Radius * 2.f);
	NoteInteraction();
	UpdateCamera();
}

void AGFCModelViewer::UpdateCamera()
{
	const FRotator Rot(Pitch, Yaw, 0.f);
	const FVector Target = Center + PanOffset;
	Capture->SetRelativeLocationAndRotation(Target - Rot.Vector() * Distance, Rot);
	UpdateLights();
	bDirty = true;
}

void AGFCModelViewer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RenderTarget)
	{
		const float Dt = FMath::Min(DeltaSeconds, 0.1f);
		if (!bHeld) IdleSeconds += DeltaSeconds;
		const bool bSpin = !bHeld && IdleSeconds >= AutoSpinResumeSeconds;
		if (bSpin)
		{
			// The turntable: steady and slow.
			Yaw = FMath::Fmod(Yaw + AutoSpinDegreesPerSecond * Dt, 360.f);
		}
		if (Animated.IsValid())
		{
			AnimTime += Dt; // keeps walking while the user turns it
		}
		// Motion renders at a capped rate to keep the phone cool.
		if (bSpin || Animated.IsValid())
		{
			SpinCaptureTimer += DeltaSeconds;
			if (SpinCaptureTimer >= 1.f / AutoSpinFps)
			{
				SpinCaptureTimer = 0.f;
				if (Animated.IsValid()) UpdateAnimation();
				UpdateCamera();
			}
		}
	}
	if ((bDirty || WarmupCaptures > 0) && RenderTarget)
	{
		bDirty = false;
		WarmupCaptures = FMath::Max(0, WarmupCaptures - 1);
		Capture->CaptureScene();
	}
}

TArray<uint8> AGFCModelViewer::SnapshotJpeg(int32 Quality)
{
	if (!RenderTarget)
	{
		return {};
	}
	Capture->CaptureScene(); // the current pose and angle, not the last throttled frame
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	TArray<FColor> Pixels;
	if (!Resource || !Resource->ReadPixels(Pixels) || Pixels.Num() != RenderTarget->SizeX * RenderTarget->SizeY)
	{
		return {};
	}
	for (FColor& P : Pixels) P.A = 255;
	IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Jpeg = IW.CreateImageWrapper(EImageFormat::JPEG);
	if (!Jpeg.IsValid() || !Jpeg->SetRaw(Pixels.GetData(), int64(Pixels.Num()) * 4, RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::BGRA, 8))
	{
		return {};
	}
	const TArray64<uint8>& Out = Jpeg->GetCompressed(Quality);
	return TArray<uint8>(Out.GetData(), int32(Out.Num()));
}

void AGFCModelViewer::Release()
{
	Animated.Reset();
	PoseScratch.Empty();
	Mesh->ClearAllMeshSections();
	Capture->TextureTarget = nullptr;
	Capture->ShowOnlyComponents.Reset();
	if (RenderTarget)
	{
		RenderTarget->ReleaseResource();
	}
	for (UTexture2D* T : Textures)
	{
		if (T) T->ReleaseResource();
	}
	Textures.Reset();
	Materials.Reset();
	RenderTarget = nullptr;
	Destroy();
	if (GEngine)
	{
		GEngine->ForceGarbageCollection(true); // hand the memory back now, not minutes later
	}
}
