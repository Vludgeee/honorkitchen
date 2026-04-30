// Copyright Epic Games, Inc. All Rights Reserved.

#include "PickupBase.h"
#include "MyProjectCharacter.h"
#include "TomatoSaurusCharacter.h"
#include "KaravaychikCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

namespace PickupEffectConstants
{
	static constexpr float SaltWaterRadiusUU = 500.f;
	static constexpr float SaltSlowDurationSec = 3.f;
	static constexpr float SaltSpeedMultiplier = 0.5f;
	static constexpr float WaterBlindDurationSec = 2.f;
}

APickupBase::APickupBase()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(SceneRoot);
	PickupSphere->SetRelativeLocation(FVector(0.f, 0.f, PickupSphereRadius));
	PickupSphere->InitSphereRadius(PickupSphereRadius);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetGenerateOverlapEvents(false);

	PickupVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupVisual"));
	PickupVisual->SetupAttachment(PickupSphere);
	PickupVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupVisual->SetCastShadow(true);
	PickupVisual->SetVisibility(true);
	PickupVisual->SetHiddenInGame(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMesh.Succeeded())
	{
		PickupVisual->SetStaticMesh(SphereMesh.Object);
		PickupVisual->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.35f));
		if (ShapeMaterial.Succeeded())
		{
			PickupVisual->SetMaterial(0, ShapeMaterial.Object);
		}
	}
}

void APickupBase::BeginPlay()
{
	Super::BeginPlay();
	PickupSphere->SetRelativeLocation(FVector(0.f, 0.f, PickupSphereRadius));
	PickupSphere->SetSphereRadius(PickupSphereRadius);

	if (PickupMaterialOverride)
	{
		PickupVisual->SetMaterial(0, PickupMaterialOverride);
	}
	else
	{
		ApplyDefaultVisual();
	}

#if !UE_BUILD_SHIPPING
	if (UWorld* W = GetWorld())
	{
		DrawDebugSphere(W, GetActorLocation(), 90.f, 14, FColor::Cyan, false, 15.f, 0, 2.f);
	}
#endif
}

bool APickupBase::ApplyDefaultMaterialIfSpecified()
{
	if (!PickupVisual || !DefaultMaterial)
	{
		return false;
	}
	PickupVisual->SetMaterial(0, DefaultMaterial);
	return true;
}

void APickupBase::ApplyDefaultVisual()
{
	if (ApplyDefaultMaterialIfSpecified())
	{
		return;
	}
	if (!PickupVisual)
	{
		return;
	}
	const int32 NumMats = PickupVisual->GetNumMaterials();
	for (int32 i = 0; i < NumMats; ++i)
	{
		if (UMaterialInstanceDynamic* MID = PickupVisual->CreateAndSetMaterialInstanceDynamic(i))
		{
			const FLinearColor C(0.7f, 0.7f, 0.75f, 1.f);
			MID->SetVectorParameterValue(FName(TEXT("Color")), C);
			MID->SetVectorParameterValue(FName(TEXT("BaseColor")), C);
			MID->SetVectorParameterValue(FName(TEXT("EmissiveColor")), C * 2.f);
		}
	}
}

bool APickupBase::TryCollect(AMyProjectCharacter* Collector)
{
	if (!Collector || ItemAmount <= 0 || ItemType == EInventoryItemType::None)
	{
		return false;
	}
	if (!Collector->TryAddItemToHotbar(ItemType, ItemAmount))
	{
		return false;
	}
	OnPickup(Collector);
	Destroy();
	return true;
}

void APickupBase::ConfigurePickup(EInventoryItemType NewType, int32 NewAmount)
{
	ItemType = NewType;
	ItemAmount = FMath::Max(1, NewAmount);
}

void APickupBase::OnPickup(AMyProjectCharacter* Collector)
{
}

void APickupBase::OnDrop(AMyProjectCharacter* Dropper)
{
}

bool APickupBase::OnUse(AMyProjectCharacter* User)
{
	return false;
}

bool APickupBase::DispatchHotbarUse(EInventoryItemType Type, AMyProjectCharacter* User)
{
	if (!User || User->IsDead())
	{
		return false;
	}
	switch (Type)
	{
	case EInventoryItemType::Medkit:
	{
		const float Heal = User->MedkitHealAmount;
		const float Prev = User->CurrentHealth;
		const float NewH = FMath::Clamp(Prev + Heal, 0.f, User->MaxHealth);
		if (NewH <= Prev + KINDA_SMALL_NUMBER)
		{
			return false;
		}
		User->CurrentHealth = NewH;
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::Green,
				FString::Printf(TEXT("Аптечка: +%.0f HP"), NewH - Prev));
		}
#endif
		return true;
	}
	case EInventoryItemType::Salt:
	{
		UWorld* W = User->GetWorld();
		if (!W)
		{
			return false;
		}
		const FVector Origin = User->GetActorLocation();
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(W, ATomatoSaurusCharacter::StaticClass(), Found);
		for (AActor* A : Found)
		{
			ATomatoSaurusCharacter* E = Cast<ATomatoSaurusCharacter>(A);
			if (E && FVector::Dist(Origin, E->GetActorLocation()) <= PickupEffectConstants::SaltWaterRadiusUU)
			{
				E->ApplySaltSlowDebuff(PickupEffectConstants::SaltSlowDurationSec, PickupEffectConstants::SaltSpeedMultiplier);
			}
		}
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::White, TEXT("Соль: замедление врагов (500 см)"));
		}
#endif
		return true;
	}
	case EInventoryItemType::Water:
	{
		UWorld* W = User->GetWorld();
		if (!W)
		{
			return false;
		}
		const FVector Origin = User->GetActorLocation();
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(W, ATomatoSaurusCharacter::StaticClass(), Found);
		for (AActor* A : Found)
		{
			if (!A || FVector::Dist(Origin, A->GetActorLocation()) > PickupEffectConstants::SaltWaterRadiusUU)
			{
				continue;
			}
			if (AKaravaychikCharacter* Bread = Cast<AKaravaychikCharacter>(A))
			{
				Bread->ApplyKaravaychikWaterDebuff();
				continue;
			}
			if (ATomatoSaurusCharacter* E = Cast<ATomatoSaurusCharacter>(A))
			{
				E->ApplyWaterBlindDebuff(PickupEffectConstants::WaterBlindDurationSec);
			}
		}
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::Cyan, TEXT("Вода: ослепление врагов (зрение откл.)"));
		}
#endif
		return true;
	}
	default:
		return false;
	}
}
