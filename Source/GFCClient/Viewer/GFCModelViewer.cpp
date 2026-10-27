#include "Viewer/GFCModelViewer.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GFCCacheSubsystem.h"
#include "GFCModelParser.h"
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

	// The key light rides with the camera so the visible side is always lit.
	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Capture);
	KeyLight->SetRelativeRotation(FRotator(-25.f, 20.f, 0.f));
	KeyLight->SetIntensity(6.f);
	KeyLight->SetCastShadows(false);
	KeyLight->SetMobility(EComponentMobility::Movable);
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
	RenderTarget->ClearColor = FLinearColor(0.010f, 0.014f, 0.030f);
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

	Center = FVector(Model.Bounds.GetCenter());
	Radius = FMath::Max(1.f, float(Model.Bounds.GetExtent().Size()));
	// The mesh's render proxy appears a frame or two later: keep capturing briefly.
	WarmupCaptures = 20;
	ResetView();
}

void AGFCModelViewer::ResetView()
{
	Yaw = 35.f;
	Pitch = -20.f;
	PanOffset = FVector::ZeroVector;
	Distance = Radius / FMath::Tan(FMath::DegreesToRadians(FovDegrees * 0.5f)) * 1.15f;
	UpdateCamera();
}

void AGFCModelViewer::Orbit(float DeltaYawDegrees, float DeltaPitchDegrees)
{
	Yaw += DeltaYawDegrees;
	Pitch = FMath::Clamp(Pitch + DeltaPitchDegrees, -85.f, 85.f);
	UpdateCamera();
}

void AGFCModelViewer::Zoom(float Factor)
{
	const float Fit = Radius / FMath::Tan(FMath::DegreesToRadians(FovDegrees * 0.5f));
	Distance = FMath::Clamp(Distance * Factor, Fit * 0.15f, Fit * 6.f);
	UpdateCamera();
}

void AGFCModelViewer::Pan(float DeltaX, float DeltaY)
{
	const FRotator Rot(Pitch, Yaw, 0.f);
	const float ViewSize = 2.f * Distance * FMath::Tan(FMath::DegreesToRadians(FovDegrees * 0.5f));
	PanOffset += (-FRotationMatrix(Rot).GetUnitAxis(EAxis::Y) * DeltaX + FRotationMatrix(Rot).GetUnitAxis(EAxis::Z) * DeltaY) * ViewSize;
	PanOffset = PanOffset.GetClampedToMaxSize(Radius * 2.f);
	UpdateCamera();
}

void AGFCModelViewer::UpdateCamera()
{
	const FRotator Rot(Pitch, Yaw, 0.f);
	const FVector Target = Center + PanOffset;
	Capture->SetRelativeLocationAndRotation(Target - Rot.Vector() * Distance, Rot);
	bDirty = true;
}

void AGFCModelViewer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if ((bDirty || WarmupCaptures > 0) && RenderTarget)
	{
		bDirty = false;
		WarmupCaptures = FMath::Max(0, WarmupCaptures - 1);
		Capture->CaptureScene();
	}
}

void AGFCModelViewer::Release()
{
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
