// Copyright Epic Games, Inc. All Rights Reserved.

#include "Portal.h"
#include "MyProjectCharacter.h"
#include "MyProjectGameMode.h"
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (SphereMesh.Succeeded())
	{
		PortalVisual->SetStaticMesh(SphereMesh.Object);
		PortalVisual->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.8f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		PortalVisual->SetMaterial(0, ShapeMaterial.Object);
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

