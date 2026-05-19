// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenPickupBillboardComponent.h"
#include "HonorKitchenPickupIconCatalog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

namespace HonorKitchenPickupBillboardPrivate
{
	static const FName SpriteTextureParam(TEXT("SpriteTexture"));
	static const FName ExtraTextureParamNames[] = {
		TEXT("Texture"),
		TEXT("BaseTexture"),
		TEXT("DiffuseTexture"),
		TEXT("Sprite"),
	};

	static TObjectPtr<UMaterialInterface> GCachedParent;

	static bool IsSpriteParentMaterial(const UMaterialInterface* Mat)
	{
		if (!Mat)
		{
			return false;
		}
		return Mat->GetPathName().Contains(TEXT("M_EnemySpriteUnlit"), ESearchCase::IgnoreCase);
	}

	static void RegisterParentMaterial()
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> Finder(
			TEXT("/Game/Materials/M_EnemySpriteUnlit.M_EnemySpriteUnlit"));
		if (Finder.Succeeded() && IsSpriteParentMaterial(Finder.Object))
		{
			GCachedParent = Finder.Object;
		}
	}

	static UMaterialInterface* ResolveParentMaterial()
	{
		if (GCachedParent && IsSpriteParentMaterial(GCachedParent))
		{
			return GCachedParent;
		}
		if (UMaterialInterface* Loaded =
				LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_EnemySpriteUnlit.M_EnemySpriteUnlit")))
		{
			if (IsSpriteParentMaterial(Loaded))
			{
				GCachedParent = Loaded;
				return Loaded;
			}
		}
		return nullptr;
	}

	static void BindTexture(UMaterialInstanceDynamic* MID, UTexture2D* Tex)
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

UHonorKitchenPickupBillboardComponent::UHonorKitchenPickupBillboardComponent()
{
	using namespace HonorKitchenPickupBillboardPrivate;
	RegisterParentMaterial();

	PrimaryComponentTick.bCanEverTick = true;
}

void UHonorKitchenPickupBillboardComponent::EnsureSpritePlane()
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
		TEXT("PickupSpritePlaneMesh"));
	SpritePlane = NewObject<UStaticMeshComponent>(Owner, PlaneName);
	if (!SpritePlane)
	{
		return;
	}

	SpritePlane->SetupAttachment(this);
	SpritePlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpritePlane->SetCastShadow(false);
	SpritePlane->SetHiddenInGame(true);
	SpritePlane->SetVisibility(false);

	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		SpritePlane->SetStaticMesh(PlaneMesh);
	}

	SpritePlane->RegisterComponent();
}

bool UHonorKitchenPickupBillboardComponent::ActivateSpriteIfAvailable(EInventoryItemType Type)
{
	ActiveType = Type;
	bSpriteActive = false;

	EnsureSpritePlane();
	if (!SpritePlane)
	{
		return false;
	}

	if (!SpritePlane->GetStaticMesh())
	{
		return false;
	}

	UTexture2D* Tex = HonorKitchenPickupIconCatalog::GetPickupIcon(Type);
	if (!HonorKitchenPickupIconCatalog::IsRenderable(Tex) || !EnsureMaterial(Tex))
	{
		SetHiddenInGame(true, true);
		SpritePlane->SetHiddenInGame(true);
		SpritePlane->SetVisibility(false);
		return false;
	}

	HonorKitchenPickupBillboardPrivate::BindTexture(SpriteMID, Tex);
	UpdatePlaneScale(Tex);

	SetHiddenInGame(false, true);
	SetVisibility(true, true);
	SpritePlane->SetHiddenInGame(false);
	SpritePlane->SetVisibility(true);

	bSpriteActive = true;
	FaceViewerCamera();
	return true;
}

bool UHonorKitchenPickupBillboardComponent::EnsureMaterial(UTexture2D* Tex)
{
	using namespace HonorKitchenPickupBillboardPrivate;
	EnsureSpritePlane();
	if (!SpritePlane || !Tex)
	{
		return false;
	}

	UMaterialInterface* Parent = ResolveParentMaterial();
	if (!Parent)
	{
		return false;
	}

	if (!SpriteMID)
	{
		SpriteMID = UMaterialInstanceDynamic::Create(Parent, this);
		if (!SpriteMID)
		{
			return false;
		}
		SpritePlane->SetMaterial(0, SpriteMID);
	}

	BindTexture(SpriteMID, Tex);
	return true;
}

void UHonorKitchenPickupBillboardComponent::UpdatePlaneScale(UTexture2D* Tex)
{
	if (!SpritePlane || !Tex)
	{
		return;
	}

	const float RefPlaneSize = 100.f;
	const int32 SX = FMath::Max(1, Tex->GetSizeX());
	const int32 SY = FMath::Max(1, Tex->GetSizeY());
	const float MaxDim = static_cast<float>(FMath::Max(SX, SY));
	const float Uniform = SpriteWorldSizeUU / MaxDim;
	float ScaleX = Uniform * static_cast<float>(SX);
	float ScaleY = Uniform * static_cast<float>(SY);

	ScaleX /= RefPlaneSize;
	ScaleY /= RefPlaneSize;

	SpritePlane->SetRelativeScale3D(FVector(ScaleX, ScaleY, 1.f));
}

void UHonorKitchenPickupBillboardComponent::FaceViewerCamera()
{
	if (!bSpriteActive || !SpritePlane)
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
	if (!ToCam.Normalize())
	{
		return;
	}

	FRotator FaceRot = FRotationMatrix::MakeFromZ(ToCam).Rotator();
	SpritePlane->SetWorldRotation(FaceRot);
}

void UHonorKitchenPickupBillboardComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSpriteActive || !SpritePlane || !SpriteMID)
	{
		return;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || World->bIsTearingDown || Owner->IsActorBeingDestroyed())
	{
		return;
	}

	if (!HonorKitchenPickupIconCatalog::IsRenderable(HonorKitchenPickupIconCatalog::GetPickupIcon(ActiveType)))
	{
		bSpriteActive = false;
		SpritePlane->SetHiddenInGame(true);
		SpritePlane->SetVisibility(false);
		return;
	}

	FaceViewerCamera();
}
