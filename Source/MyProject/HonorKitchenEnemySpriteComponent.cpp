// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenEnemySpriteComponent.h"
#include "HonorKitchenSpriteSubject.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace HonorKitchenEnemySpriteComponentPrivate
{
	static const FName SpriteTextureParam(TEXT("SpriteTexture"));

	static const FName ExtraTextureParamNames[] = {
		TEXT("Texture"),
		TEXT("BaseTexture"),
		TEXT("DiffuseTexture"),
		TEXT("Sprite"),
	};

	static TObjectPtr<UMaterialInterface> GCachedEnemySpriteParent;

	static bool IsEnemySpriteParentMaterial(const UMaterialInterface* Mat)
	{
		if (!Mat)
		{
			return false;
		}
		const FString Path = Mat->GetPathName();
		return Path.Contains(TEXT("M_EnemySpriteUnlit"), ESearchCase::IgnoreCase);
	}

	/** FObjectFinder только в конструкторе компонента (иначе fatal в BeginPlay). */
	static void RegisterSpriteParentMaterialReference()
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnemyMatFinder(
			TEXT("/Game/Materials/M_EnemySpriteUnlit.M_EnemySpriteUnlit"));
		if (EnemyMatFinder.Succeeded() && IsEnemySpriteParentMaterial(EnemyMatFinder.Object))
		{
			GCachedEnemySpriteParent = EnemyMatFinder.Object;
		}
	}

	static UMaterialInterface* LoadSpriteParentMaterial()
	{
		if (GCachedEnemySpriteParent && IsEnemySpriteParentMaterial(GCachedEnemySpriteParent))
		{
			return GCachedEnemySpriteParent;
		}

		if (UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Materials/M_EnemySpriteUnlit.M_EnemySpriteUnlit")))
		{
			if (IsEnemySpriteParentMaterial(Loaded))
			{
				GCachedEnemySpriteParent = Loaded;
				return Loaded;
			}
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("M_EnemySpriteUnlit missing. Output Log: Py .../Tools/import_enemy_sprites.py"));
		return nullptr;
	}

	static void BindSpriteTexture(UMaterialInstanceDynamic* MID, UTexture2D* Tex)
	{
		if (!MID || !Tex)
		{
			return;
		}

		MID->SetTextureParameterValue(SpriteTextureParam, Tex);
		for (const FName& Name : ExtraTextureParamNames)
		{
			MID->SetTextureParameterValue(Name, Tex);
		}
	}
}

UHonorKitchenEnemySpriteComponent::UHonorKitchenEnemySpriteComponent()
{
	using namespace HonorKitchenEnemySpriteComponentPrivate;
	RegisterSpriteParentMaterialReference();

	PrimaryComponentTick.bCanEverTick = true;
}

void UHonorKitchenEnemySpriteComponent::PurgeLegacySpritePlaneChildren()
{
	TArray<USceneComponent*> Children;
	GetChildrenComponents(false, Children);
	for (USceneComponent* Child : Children)
	{
		if (Child && Child->GetFName() == TEXT("SpritePlane"))
		{
			if (SpritePlane == Child)
			{
				SpritePlane = nullptr;
			}
			Child->DestroyComponent(false);
		}
	}
}

void UHonorKitchenEnemySpriteComponent::OnRegister()
{
	PurgeLegacySpritePlaneChildren();
	Super::OnRegister();
}

void UHonorKitchenEnemySpriteComponent::EnsureSpritePlane()
{
	if (SpritePlane && IsValid(SpritePlane))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !IsRegistered())
	{
		return;
	}

	const FName PlaneName = MakeUniqueObjectName(
		Owner,
		UStaticMeshComponent::StaticClass(),
		TEXT("EnemySpritePlaneMesh"));
	SpritePlane = NewObject<UStaticMeshComponent>(Owner, PlaneName);
	if (!SpritePlane)
	{
		return;
	}

	SpritePlane->SetupAttachment(this);
	SpritePlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpritePlane->SetCastShadow(false);
	SpritePlane->SetHiddenInGame(true, true);
	SpritePlane->SetVisibility(false, true);

	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		SpritePlane->SetStaticMesh(PlaneMesh);
	}

	SpritePlane->RegisterComponent();
}

void UHonorKitchenEnemySpriteComponent::SetSpriteFrames(const FHonorKitchenEnemySpriteFrames& InFrames)
{
	SpriteFrames = InFrames;
	if (HasBegunPlay())
	{
		RefreshSpriteVisual();
	}
}

void UHonorKitchenEnemySpriteComponent::SetLegacyVisualToHide(USceneComponent* InLegacyVisual)
{
	LegacyVisual = InLegacyVisual;
}

void UHonorKitchenEnemySpriteComponent::SetLockedSpriteOrientation(const FRotator& WorldRotation)
{
	bLockSpriteOrientation = true;
	LockedSpriteRotation = WorldRotation;
	if (SpritePlane)
	{
		SpritePlane->SetWorldRotation(LockedSpriteRotation);
	}
}

void UHonorKitchenEnemySpriteComponent::SetExplicitWorldPlaneSize(float WidthUU, float HeightUU)
{
	ExplicitPlaneWidthUU = FMath::Max(0.f, WidthUU);
	ExplicitPlaneHeightUU = FMath::Max(0.f, HeightUU);
	if (HasBegunPlay())
	{
		RefreshSpriteVisual();
	}
}

bool UHonorKitchenEnemySpriteComponent::HasLoadedSpriteTextures() const
{
	return SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Front) != nullptr
		|| SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Back) != nullptr
		|| SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Attack) != nullptr
		|| SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Chase) != nullptr;
}

bool UHonorKitchenEnemySpriteComponent::EnsureSpriteMaterial()
{
	using namespace HonorKitchenEnemySpriteComponentPrivate;

	if (SpriteMID)
	{
		return IsEnemySpriteParentMaterial(SpriteMID->Parent);
	}

	EnsureSpritePlane();
	if (!SpritePlane)
	{
		return false;
	}

	UMaterialInterface* Parent = LoadSpriteParentMaterial();
	if (!Parent)
	{
		return false;
	}

	SpriteMID = UMaterialInstanceDynamic::Create(Parent, this);
	if (!SpriteMID)
	{
		return false;
	}

	SpriteMID->TwoSided = true;
	SpritePlane->SetMaterial(0, SpriteMID);
	return true;
}

void UHonorKitchenEnemySpriteComponent::UpdatePlaneScaleForTexture(
	const UTexture2D* Tex,
	EHonorKitchenEnemySpriteView View)
{
	if (!SpritePlane || !Tex)
	{
		return;
	}

	const UTexture2D* FrontTex = SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Front);
	const UTexture2D* SizeRef = FrontTex ? FrontTex : Tex;

	const float RefPlaneSize = 100.f;
	const int32 RefSizeX = SizeRef->GetSizeX();
	const int32 RefSizeY = SizeRef->GetSizeY();
	const float RefMaxDim = FMath::Max(1, FMath::Max(RefSizeX, RefSizeY));
	const float Uniform = SpriteWorldSizeUU / RefPlaneSize;
	float ScaleX = 0.f;
	float ScaleY = 0.f;

	if (ExplicitPlaneHeightUU > 1.f)
	{
		ScaleY = ExplicitPlaneHeightUU / RefPlaneSize;
		if (ExplicitPlaneWidthUU > 1.f)
		{
			ScaleX = ExplicitPlaneWidthUU / RefPlaneSize;
		}
		else
		{
			const float RefSizeYF = static_cast<float>(FMath::Max(1, RefSizeY));
			ScaleX = ScaleY * (static_cast<float>(RefSizeX) / RefSizeYF);
		}
	}
	else if (bScaleSpriteByHeight)
	{
		const float RefSizeYF = static_cast<float>(FMath::Max(1, RefSizeY));
		ScaleY = Uniform;
		ScaleX = Uniform * (static_cast<float>(RefSizeX) / RefSizeYF);
	}
	else
	{
		ScaleX = Uniform * (static_cast<float>(RefSizeX) / RefMaxDim);
		ScaleY = Uniform * (static_cast<float>(RefSizeY) / RefMaxDim);
	}

	// Attack/Chase: если текстура больше front (старый 1024 импорт), увеличиваем плоскость до визуала front.
	if (ExplicitPlaneHeightUU <= 1.f
		&& (View == EHonorKitchenEnemySpriteView::Attack || View == EHonorKitchenEnemySpriteView::Chase))
	{
		float ViewScale = AttackChaseViewScaleMultiplier;
		if (FrontTex)
		{
			const int32 TexMaxDim = FMath::Max(Tex->GetSizeX(), Tex->GetSizeY());
			if (TexMaxDim > RefMaxDim)
			{
				ViewScale *= static_cast<float>(TexMaxDim) / static_cast<float>(RefMaxDim);
			}
		}
		ScaleX *= ViewScale;
		ScaleY *= ViewScale;
	}

	if (bFlipSpriteHorizontal)
	{
		ScaleX = -ScaleX;
	}
	if (bFlipSpriteVertical)
	{
		ScaleY = -ScaleY;
	}
	SpritePlane->SetRelativeScale3D(FVector(ScaleX, ScaleY, 1.f));

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("%s view %d plane %.1fx%.1f uu (front %dx%d, tex %dx%d)"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(View),
		FMath::Abs(ScaleX) * RefPlaneSize,
		FMath::Abs(ScaleY) * RefPlaneSize,
		RefSizeX,
		RefSizeY,
		Tex->GetSizeX(),
		Tex->GetSizeY());
}

void UHonorKitchenEnemySpriteComponent::SetLegacyVisualVisible(bool bShowLegacyMesh)
{
	if (USceneComponent* Legacy = LegacyVisual.Get())
	{
		Legacy->SetHiddenInGame(!bShowLegacyMesh, true);
		Legacy->SetVisibility(bShowLegacyMesh, true);
	}
}

void UHonorKitchenEnemySpriteComponent::RefreshSpriteVisual()
{
	using namespace HonorKitchenEnemySpriteComponentPrivate;

	if (IsRegistered())
	{
		EnsureSpritePlane();
	}

	SpriteFrames.PreloadAll();
	const bool bHadTextures = HasLoadedSpriteTextures();
	const bool bHadMaterial = EnsureSpriteMaterial();
	bSpriteActive = bHadTextures && bHadMaterial;

	if (bSpriteActive)
	{
		SetLegacyVisualVisible(false);
		ApplyView(PickView(GetOwner(), UGameplayStatics::GetPlayerPawn(this, 0)));
		UE_LOG(LogTemp, Log, TEXT("%s: sprite ON (parent=%s)"), *GetNameSafe(GetOwner()), *GetNameSafe(SpriteMID->Parent));
	}
	else
	{
		SetLegacyVisualVisible(true);
		if (SpritePlane)
		{
			SpritePlane->SetHiddenInGame(true, true);
			SpritePlane->SetVisibility(false, true);
		}
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: sprite OFF (textures=%d material=%d) — run import_enemy_sprites.py"),
			*GetNameSafe(GetOwner()),
			bHadTextures ? 1 : 0,
			bHadMaterial ? 1 : 0);
	}
}

void UHonorKitchenEnemySpriteComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureSpritePlane();
	RefreshSpriteVisual();
}

void UHonorKitchenEnemySpriteComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSpriteActive)
	{
		if (SpriteActivateRetries < MaxSpriteActivateRetries)
		{
			++SpriteActivateRetries;
			RefreshSpriteVisual();
		}
		return;
	}

	if (bLockSpriteOrientation)
	{
		if (SpritePlane)
		{
			FRotator R = LockedSpriteRotation;
			R.Roll = FMath::UnwindDegrees(R.Roll + SpriteFacingRollOffset);
			SpritePlane->SetWorldRotation(R);
		}
	}
	else
	{
		FaceViewerCamera();
	}

	const EHonorKitchenEnemySpriteView NewView = PickView(GetOwner(), UGameplayStatics::GetPlayerPawn(this, 0));
	// Каждый тик: иначе до первой смены кадра MID без текстуры → белый квадрат.
	ApplyView(NewView);
}

void UHonorKitchenEnemySpriteComponent::FaceViewerCamera()
{
	if (!SpritePlane)
	{
		return;
	}

	const APawn* Viewer = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Viewer)
	{
		return;
	}

	FVector CamLoc = Viewer->GetActorLocation();
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			CamLoc = PC->PlayerCameraManager->GetCameraLocation();
		}
	}

	const FVector PlaneLoc = SpritePlane->GetComponentLocation();
	FVector ToCam = CamLoc - PlaneLoc;

	if (bBillboardFaceCamera)
	{
		if (!ToCam.Normalize())
		{
			return;
		}
		FRotator FaceRot = FRotationMatrix::MakeFromZ(ToCam).Rotator();
		FaceRot.Yaw = FMath::UnwindDegrees(FaceRot.Yaw + SpriteFacingYawOffset);
		FaceRot.Roll = FMath::UnwindDegrees(FaceRot.Roll + SpriteFacingRollOffset);
		SpritePlane->SetWorldRotation(FaceRot);
		return;
	}

	ToCam.Z = 0.f;
	if (!ToCam.Normalize())
	{
		return;
	}
	const float YawToCamera = FMath::UnwindDegrees(ToCam.Rotation().Yaw + SpriteFacingYawOffset);
	SpritePlane->SetWorldRotation(FRotator(0.f, YawToCamera, 0.f));
}

EHonorKitchenEnemySpriteView UHonorKitchenEnemySpriteComponent::PickView(const AActor* OwnerActor, const APawn* ViewerPawn) const
{
	const IHonorKitchenSpriteSubject* Subject = OwnerActor ? Cast<IHonorKitchenSpriteSubject>(OwnerActor) : nullptr;
	if (!Subject || !ViewerPawn)
	{
		return EHonorKitchenEnemySpriteView::Front;
	}

	if (Subject->GetSpriteChaseFrame())
	{
		return EHonorKitchenEnemySpriteView::Chase;
	}
	if (Subject->GetSpriteAttackFrame())
	{
		return EHonorKitchenEnemySpriteView::Attack;
	}

	if (Subject->GetSpritePlayerAware())
	{
		return EHonorKitchenEnemySpriteView::Front;
	}

	// В покое всегда Front — иначе Back может не подгрузиться и спрайт пропадает до удара.
	if (SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Front))
	{
		return EHonorKitchenEnemySpriteView::Front;
	}

	const FVector OwnerLoc = OwnerActor->GetActorLocation();
	const FVector ToViewer = ViewerPawn->GetActorLocation() - OwnerLoc;
	const FVector Fwd = OwnerActor->GetActorForwardVector();
	const float Dot = FVector::DotProduct(Fwd.GetSafeNormal2D(), ToViewer.GetSafeNormal2D());
	return Dot >= 0.f ? EHonorKitchenEnemySpriteView::Front : EHonorKitchenEnemySpriteView::Back;
}

void UHonorKitchenEnemySpriteComponent::ApplyView(EHonorKitchenEnemySpriteView View)
{
	using namespace HonorKitchenEnemySpriteComponentPrivate;

	if (!bSpriteActive || !SpriteMID)
	{
		return;
	}

	ActiveView = View;
	UTexture2D* Tex = SpriteFrames.Resolve(View);
	if (!Tex)
	{
		Tex = SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Front);
	}
	if (!Tex)
	{
		Tex = SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Chase);
	}
	if (!Tex)
	{
		Tex = SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Back);
	}
	if (!Tex)
	{
		Tex = SpriteFrames.Resolve(EHonorKitchenEnemySpriteView::Attack);
	}
	if (!Tex)
	{
		if (SpritePlane)
		{
			SpritePlane->SetHiddenInGame(true, true);
			SpritePlane->SetVisibility(false, true);
		}
		UE_LOG(LogTemp, Warning, TEXT("%s: no texture for view"), *GetNameSafe(GetOwner()));
		return;
	}

	BindSpriteTexture(SpriteMID, Tex);
	UpdatePlaneScaleForTexture(Tex, View);
	if (SpritePlane)
	{
		SpritePlane->SetMaterial(0, SpriteMID);
		SpritePlane->SetHiddenInGame(false, true);
		SpritePlane->SetVisibility(true, true);
		SpritePlane->MarkRenderStateDirty();
	}
}
