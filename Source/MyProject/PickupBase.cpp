// Copyright Epic Games, Inc. All Rights Reserved.

#include "PickupBase.h"
#include "HonorKitchenPickupBillboardComponent.h"
#include "HonorKitchenAudioDefaults.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenDevDebug.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "MyProjectCharacter.h"
#include "SaltBarrierActor.h"
#include "KaravaychikCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace PickupEffectConstants
{
	static constexpr float SaltWaterRadiusUU = 500.f;
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

	PickupBillboard = CreateDefaultSubobject<UHonorKitchenPickupBillboardComponent>(TEXT("PickupBillboard"));
	PickupBillboard->SetupAttachment(PickupSphere);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMesh.Succeeded())
	{
		PickupVisual->SetStaticMesh(SphereMesh.Object);
		PickupVisual->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.05f));
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

	// Активация спрайта на следующий тик: к BeginPlay RHI-ресурс текстуры может быть ещё не готов.
	if (PickupBillboard)
	{
		if (UWorld* World = GetWorld())
		{
			const EInventoryItemType Type = ItemType;
			TWeakObjectPtr<APickupBase> WeakPickup(this);
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [WeakPickup, Type]()
			{
				APickupBase* Pickup = WeakPickup.Get();
				if (!Pickup || !Pickup->PickupBillboard)
				{
					return;
				}
				if (Pickup->PickupBillboard->ActivateSpriteIfAvailable(Type) && Pickup->PickupVisual)
				{
					Pickup->PickupVisual->SetHiddenInGame(true);
					Pickup->PickupVisual->SetVisibility(false);
				}
			}));
		}
	}
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
	(void)Collector;
	if (USoundBase* S = PickupSound ? PickupSound.Get() : HonorKitchenAudioDefaults::GetPickupSound())
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			S,
			GetActorLocation(),
			FRotator::ZeroRotator,
			HonorKitchenAudioSettings::ScaleVolume(0.8f));
	}
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
		HonorKitchenDevDebug::OnScreen(1.2f, FColor::Green, FString::Printf(TEXT("Аптечка: +%.0f HP"), NewH - Prev));
		return true;
	}
	case EInventoryItemType::Salt:
	{
		UWorld* W = User->GetWorld();
		if (!W)
		{
			return false;
		}
		const FVector Forward = User->GetActorForwardVector().GetSafeNormal2D();
		const FVector SpawnDir = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward;
		const FVector SpawnLoc = User->GetActorLocation() + SpawnDir * 140.f + FVector(0.f, 0.f, 4.f);
		const FRotator SpawnRot = (SpawnDir.Rotation() + FRotator(0.f, 90.f, 0.f));
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASaltBarrierActor* Barrier = W->SpawnActor<ASaltBarrierActor>(ASaltBarrierActor::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
		if (!Barrier)
		{
			return false;
		}
		HonorKitchenDevDebug::OnScreen(1.2f, FColor::White, TEXT("Соль: барьер установлен"));
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
		for (TActorIterator<AKaravaychikCharacter> It(W); It; ++It)
		{
			AKaravaychikCharacter* Bread = *It;
			if (!Bread || FVector::Dist(Origin, Bread->GetActorLocation()) > PickupEffectConstants::SaltWaterRadiusUU)
			{
				continue;
			}
			Bread->ApplyKaravaychikWaterDebuff();
		}
		HonorKitchenDevDebug::OnScreen(1.2f, FColor::Cyan, TEXT("Вода: дебафф только Каравайчику"));
		return true;
	}
	default:
		return false;
	}
}
