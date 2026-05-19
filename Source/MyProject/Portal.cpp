// Copyright Epic Games, Inc. All Rights Reserved.

#include "Portal.h"
#include "MyProjectCharacter.h"
#include "MyProjectGameMode.h"
#include "HonorKitchenEnemySpriteComponent.h"
#include "HonorKitchenPortalSpriteCatalog.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APortal::APortal()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionSphere->InitSphereRadius(InteractionRadius);

	PortalVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalVisual"));
	PortalVisual->SetupAttachment(SceneRoot);
	PortalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PortalVisual->SetStaticMesh(CylinderMesh.Object);
		PortalVisual->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.5f));
		PortalVisual->SetHiddenInGame(true, true);
		PortalVisual->SetVisibility(false, true);
	}

	PortalSprite = CreateDefaultSubobject<UHonorKitchenEnemySpriteComponent>(TEXT("PortalSprite"));
	PortalSprite->SetupAttachment(SceneRoot);
	PortalSprite->bBillboardFaceCamera = false;
	PortalSprite->SpriteFacingYawOffset = 0.f;
	PortalSprite->SpriteFacingRollOffset = PortalSpriteRollOffset;
	PortalSprite->SetSpriteFrames(HonorKitchenPortalSpriteCatalog::MakePortalFrames());
	PortalSprite->SetLegacyVisualToHide(PortalVisual);
}

FRotator APortal::MakeWallMountRotation(const FVector& NormalIntoRoom)
{
	const FVector IntoRoom = NormalIntoRoom.GetSafeNormal2D();
	if (IntoRoom.IsNearlyZero())
	{
		return FRotator::ZeroRotator;
	}

	// Plane mesh: нормаль +Z — как у врагов (MakeFromZ к камере), не MakeFromXZ (тот кладёт ребром в стену).
	const FVector N = FVector(IntoRoom.X, IntoRoom.Y, 0.f).GetSafeNormal();
	return FRotationMatrix::MakeFromZ(N).Rotator();
}

void APortal::ApplyWallMount(const FVector& WorldLocation, const FVector& NormalIntoRoom)
{
	const FVector IntoRoom = NormalIntoRoom.GetSafeNormal2D();
	if (!IntoRoom.IsNearlyZero())
	{
		const FRotator WallRot = MakeWallMountRotation(IntoRoom);
		SetActorLocationAndRotation(WorldLocation, WallRot);
		if (PortalSprite)
		{
			PortalSprite->SpriteFacingRollOffset = PortalSpriteRollOffset;
			PortalSprite->SetExplicitWorldPlaneSize(PortalSpriteWidthUU, PortalSpriteHeightUU);
			PortalSprite->SetLockedSpriteOrientation(WallRot);
			PortalSprite->RefreshSpriteVisual();
		}
	}
	else
	{
		SetActorLocation(WorldLocation);
	}
}

bool APortal::TryActivate(AMyProjectCharacter* Interactor)
{
	if (!Interactor)
	{
		return false;
	}
	const float DistSq = FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation());
	if (DistSq > FMath::Square(InteractionRadius))
	{
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(Interactor->GetController());
	AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
	if (!PC || !GM)
	{
		return false;
	}
	const bool bOk = GM->TryActivatePortal(PC);
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		if (bOk)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.8f, FColor::Green, TEXT("Портал активирован! Победа."));
		}
		else
		{
			const FString Msg = FString::Printf(TEXT("Нужно батареек: %d/%d"), GM->GetCollectedBatteries(), GM->GetRequiredBatteries());
			GEngine->AddOnScreenDebugMessage(-1, 1.8f, FColor::Yellow, Msg);
		}
	}
#endif
	return bOk;
}
