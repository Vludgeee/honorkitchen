// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectCharacter.h"
#include "PickupBase.h"
#include "CrumbPickup.h"
#include "MedkitPickup.h"
#include "SaltPickup.h"
#include "WaterPickup.h"
#include "CrumbProjectile.h"
#include "Portal.h"
#include "MyProjectProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MyProjectGameMode.h"
#include "Engine/Engine.h"
#include "Sound/SoundBase.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

bool AMyProjectCharacter::IsRunningForVilokhvostAI() const
{
	if (bDead)
	{
		return false;
	}
	const UCharacterMovementComponent* M = GetCharacterMovement();
	if (!M)
	{
		return false;
	}
	return M->Velocity.Size2D() >= RunSpeedThresholdForVilokhvost;
}

//////////////////////////////////////////////////////////////////////////
// AMyProjectCharacter

AMyProjectCharacter::AMyProjectCharacter()
{
	CurrentHealth = MaxHealth;
	HotbarSlots.SetNum(9);

	// Character doesnt have a rifle at start
	bHasRifle = false;
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
		
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	PerceptionStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSource"));
}

float AMyProjectCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (const AMyProjectGameMode* GM = GetWorld() ? Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		if (GM->IsRoundWon())
		{
			return 0.f;
		}
	}

	if (bDead || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	CurrentHealth -= ActualDamage;
	if (UWorld* W = GetWorld())
	{
		DamageHudLastAmount = ActualDamage;
		DamageHudExpiresTime = W->GetTimeSeconds() + 1.35;
	}
	PlayDamageFeedback(ActualDamage);
	if (CurrentHealth <= 0.f)
	{
		HandleDeath();
	}
	return ActualDamage;
}

void AMyProjectCharacter::PlayDamageFeedback(float DamageAmount)
{
	if (DamageTakenSound)
	{
		UGameplayStatics::PlaySound2D(this, DamageTakenSound, 0.85f);
	}
}

void AMyProjectCharacter::AddCrumbs(int32 Amount)
{
	TryAddItemToHotbar(EInventoryItemType::Crumb, Amount);
}

void AMyProjectCharacter::TryThrowCrumb()
{
	ThrowActiveItem();
}

bool AMyProjectCharacter::TryAddItemToHotbar(EInventoryItemType ItemType, int32 Amount)
{
	if (ItemType == EInventoryItemType::None || Amount <= 0)
	{
		return false;
	}

	int32 Remaining = Amount;
	for (FHotbarSlot& Slot : HotbarSlots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (!Slot.IsEmpty())
		{
			continue;
		}
		Slot.ItemType = ItemType;
		Slot.Amount = 1;
		--Remaining;
	}

	if (Remaining > 0)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::Orange, TEXT("Хотбар заполнен"));
		}
#endif
	}

	const int32 AddedAmount = Amount - Remaining;
	if (AddedAmount > 0 && ItemType == EInventoryItemType::Battery)
	{
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			GM->NotifyBatteryCollected(AddedAmount);
		}
	}

	RefreshLegacyCrumbCount();
	return Remaining < Amount;
}

bool AMyProjectCharacter::TryPickupNearbyItem()
{
	if (!GetWorld())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemplateCharacter, Warning, TEXT("TryPickupNearbyItem: no world"));
#endif
		return false;
	}

	TArray<AActor*> Pickups;
	UGameplayStatics::GetAllActorsOfClass(this, APickupBase::StaticClass(), Pickups);

	APickupBase* Best = nullptr;
	float BestDistSq = FLT_MAX;
	const FVector Origin = GetActorLocation();
	const float MaxDistSq = FMath::Square(320.f);
	float NearestAnyDistSq = FLT_MAX;
	for (AActor* A : Pickups)
	{
		APickupBase* P = Cast<APickupBase>(A);
		if (!P || P->IsActorBeingDestroyed())
		{
			continue;
		}
		const float D = FVector::DistSquared(Origin, P->GetActorLocation());
		NearestAnyDistSq = FMath::Min(NearestAnyDistSq, D);
		if (D <= MaxDistSq && D < BestDistSq)
		{
			BestDistSq = D;
			Best = P;
		}
	}

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		const float Nearest = (NearestAnyDistSq < FLT_MAX) ? FMath::Sqrt(NearestAnyDistSq) : -1.f;
		const FString Info = FString::Printf(TEXT("Pickup scan: actors=%d nearest=%.0f"), Pickups.Num(), Nearest);
		GEngine->AddOnScreenDebugMessage(-1, 1.4f, FColor::Silver, Info);
	}
#endif

	if (!Best)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemplateCharacter, Warning, TEXT("TryPickupNearbyItem: nothing in radius %.0f"), FMath::Sqrt(MaxDistSq));
#endif
		return false;
	}

	const bool bCollected = Best->TryCollect(this);
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.2f, bCollected ? FColor::Green : FColor::Red, bCollected ? TEXT("Pickup success") : TEXT("Pickup failed (slot full?)"));
	}
#endif
	return bCollected;
}

bool AMyProjectCharacter::TryInteractPortal()
{
	if (!GetWorld())
	{
		return false;
	}
	TArray<AActor*> Portals;
	UGameplayStatics::GetAllActorsOfClass(this, APortal::StaticClass(), Portals);
	APortal* Best = nullptr;
	float BestDistSq = FLT_MAX;
	const FVector Origin = GetActorLocation();
	for (AActor* A : Portals)
	{
		APortal* P = Cast<APortal>(A);
		if (!P)
		{
			continue;
		}
		const float D = FVector::DistSquared(Origin, P->GetActorLocation());
		if (D < BestDistSq)
		{
			BestDistSq = D;
			Best = P;
		}
	}
	if (!Best)
	{
		return false;
	}
	return Best->TryActivate(this);
}

bool AMyProjectCharacter::DropActiveItem()
{
	if (!GetWorld() || !HotbarSlots.IsValidIndex(ActiveHotbarIndex))
	{
		return false;
	}
	FHotbarSlot& Slot = HotbarSlots[ActiveHotbarIndex];
	if (Slot.IsEmpty())
	{
		return false;
	}
	if (Slot.ItemType == EInventoryItemType::Battery)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::Yellow, TEXT("Батарейку нельзя выбрасывать"));
		}
#endif
		return false;
	}

	const FVector Forward = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetForwardVector() : GetActorForwardVector();
	const FVector SpawnLoc = GetActorLocation() + Forward * 80.f + FVector(0.f, 0.f, 24.f);

	UClass* PickupClass = APickupBase::StaticClass();
	switch (Slot.ItemType)
	{
	case EInventoryItemType::Crumb:
		PickupClass = ACrumbPickup::StaticClass();
		break;
	case EInventoryItemType::Medkit:
		PickupClass = AMedkitPickup::StaticClass();
		break;
	case EInventoryItemType::Salt:
		PickupClass = ASaltPickup::StaticClass();
		break;
	case EInventoryItemType::Water:
		PickupClass = AWaterPickup::StaticClass();
		break;
	default:
		break;
	}

	if (APickupBase* Pickup = GetWorld()->SpawnActor<APickupBase>(PickupClass, SpawnLoc, FRotator::ZeroRotator))
	{
		Pickup->ConfigurePickup(Slot.ItemType, 1);
		Pickup->NotifyDroppedFromInventory(this);
	}
	Slot.Amount -= 1;
	if (Slot.Amount <= 0)
	{
		Slot = FHotbarSlot();
	}
	RefreshLegacyCrumbCount();
	return true;
}

bool AMyProjectCharacter::ThrowActiveItem()
{
	if (bDead || !GetWorld() || !HotbarSlots.IsValidIndex(ActiveHotbarIndex))
	{
		return false;
	}
	FHotbarSlot& Slot = HotbarSlots[ActiveHotbarIndex];
	if (Slot.IsEmpty())
	{
		return false;
	}

	if (Slot.ItemType == EInventoryItemType::Medkit)
	{
		if (!APickupBase::DispatchHotbarUse(EInventoryItemType::Medkit, this))
		{
			return false;
		}
		Slot.Amount -= 1;
		if (Slot.Amount <= 0)
		{
			Slot = FHotbarSlot();
		}
		RefreshLegacyCrumbCount();
		return true;
	}

	if (Slot.ItemType == EInventoryItemType::Salt || Slot.ItemType == EInventoryItemType::Water)
	{
		if (!APickupBase::DispatchHotbarUse(Slot.ItemType, this))
		{
			return false;
		}
		Slot.Amount -= 1;
		if (Slot.Amount <= 0)
		{
			Slot = FHotbarSlot();
		}
		RefreshLegacyCrumbCount();
		return true;
	}

	if (Slot.ItemType != EInventoryItemType::Crumb || !FirstPersonCameraComponent)
	{
		return false;
	}
	UClass* Clss = CrumbProjectileClass ? *CrumbProjectileClass : ACrumbProjectile::StaticClass();
	const FVector Dir = FirstPersonCameraComponent->GetForwardVector();
	const FVector Start = FirstPersonCameraComponent->GetComponentLocation() + Dir * 48.f;
	const FRotator Rot = Dir.Rotation();

	FActorSpawnParameters Sp;
	Sp.Owner = this;
	Sp.Instigator = this;
	GetWorld()->SpawnActor<ACrumbProjectile>(Clss, Start, Rot, Sp);

	Slot.Amount -= 1;
	if (Slot.Amount <= 0)
	{
		Slot = FHotbarSlot();
	}
	RefreshLegacyCrumbCount();
	if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->NotifyCrumbThrown();
	}
	return true;
}

void AMyProjectCharacter::SetActiveHotbarSlot(int32 SlotIndex)
{
	ActiveHotbarIndex = FMath::Clamp(SlotIndex, 0, HotbarSlots.Num() - 1);
}

void AMyProjectCharacter::RefreshLegacyCrumbCount()
{
	int32 Total = 0;
	for (const FHotbarSlot& Slot : HotbarSlots)
	{
		if (Slot.ItemType == EInventoryItemType::Crumb)
		{
			Total += FMath::Max(0, Slot.Amount);
		}
	}
	CrumbCount = Total;
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			GM->NotifyCrumbsCollected(PC, CrumbCount);
		}
	}
}

FString AMyProjectCharacter::ItemTypeToDisplayName(EInventoryItemType ItemType)
{
	switch (ItemType)
	{
	case EInventoryItemType::Crumb: return TEXT("Крошка");
	case EInventoryItemType::Medkit: return TEXT("Аптечка");
	case EInventoryItemType::Battery: return TEXT("Батарейка");
	case EInventoryItemType::Salt: return TEXT("Соль");
	case EInventoryItemType::Water: return TEXT("Вода");
	case EInventoryItemType::Magnet: return TEXT("Магнит");
	default: return TEXT("-");
	}
}

void AMyProjectCharacter::ThrowCrumb()
{
	if (!ThrowActiveItem())
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Выбери слот с крошкой"));
		}
#endif
	}
}

void AMyProjectCharacter::HandleDeath()
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()))
		{
			GM->NotifyPlayerDied(PC);
			return;
		}
	}

	// Если GameMode не наш C++ — полная перезагрузка уровня
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void AMyProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	bDead = false;
	CurrentHealth = MaxHealth;
	HotbarSlots.SetNum(9);
	for (FHotbarSlot& Slot : HotbarSlots)
	{
		Slot = FHotbarSlot();
	}
	ActiveHotbarIndex = 0;
	RefreshLegacyCrumbCount();

	if (PerceptionStimuliSource)
	{
		PerceptionStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
		PerceptionStimuliSource->RegisterWithPerceptionSystem();
	}

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (DefaultMappingContext)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////// Input

void AMyProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// Moving
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Move);
		}

		// Looking
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Look);
		}

		if (ThrowCrumbAction)
		{
			EnhancedInputComponent->BindAction(ThrowCrumbAction, ETriggerEvent::Started, this, &AMyProjectCharacter::ThrowCrumb);
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AMyProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AMyProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AMyProjectCharacter::SetHasRifle(bool bNewHasRifle)
{
	bHasRifle = bNewHasRifle;
}

bool AMyProjectCharacter::GetHasRifle()
{
	return bHasRifle;
}