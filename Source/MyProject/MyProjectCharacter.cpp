// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectCharacter.h"
#include "HonorKitchenPortalNavigatorComponent.h"
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
#include "Components/SpotLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MyProjectGameMode.h"
#include "DynamicPostProcess.h"
#include "HonorKitchenAudioDefaults.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenDevDebug.h"
#include "HonorKitchenSaveGame.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "GameFramework/Controller.h"

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

	FlashlightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	FlashlightComponent->SetupAttachment(FirstPersonCameraComponent);
	FlashlightComponent->SetRelativeLocation(FVector(12.f, 0.f, -4.f));
	FlashlightComponent->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	FlashlightComponent->SetVisibility(false);
	FlashlightComponent->Intensity = 36000.f;
	FlashlightComponent->AttenuationRadius = 3400.f;
	FlashlightComponent->InnerConeAngle = 16.f;
	FlashlightComponent->OuterConeAngle = 34.f;
	FlashlightComponent->VolumetricScatteringIntensity = 0.55f;
	FlashlightComponent->CastShadows = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	PerceptionStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSource"));

	PortalNavigator = CreateDefaultSubobject<UHonorKitchenPortalNavigatorComponent>(TEXT("PortalNavigator"));

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}
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
	const bool bLethal = CurrentHealth <= 0.f;
	if (bLethal)
	{
		PlayLethalDamagePresentation();
		HandleDeath();
	}
	else
	{
		PlayDamageFeedback(ActualDamage);
	}
	return ActualDamage;
}

ADynamicPostProcess* AMyProjectCharacter::ResolvePostProcessActor()
{
	for (TActorIterator<ADynamicPostProcess> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<ADynamicPostProcess>(ADynamicPostProcess::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Sp);
}

float AMyProjectCharacter::GetDamageScreenFlashAlpha() const
{
	if (bDeathVignetteHeld)
	{
		return 1.f;
	}
	const UWorld* W = GetWorld();
	if (!W || DamageScreenFlashEndTime <= 0.0)
	{
		return 0.f;
	}
	const float Remaining = static_cast<float>(DamageScreenFlashEndTime - W->GetTimeSeconds());
	if (Remaining <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(Remaining / 0.35f, 0.f, 1.f);
}

void AMyProjectCharacter::PlayDamageFeedback(float DamageAmount)
{
	(void)DamageAmount;
	if (UWorld* W = GetWorld())
	{
		DamageScreenFlashEndTime = W->GetTimeSeconds() + 0.35f;
	}
	// Удар слышен через HonorKitchenEnemySoundCatalog (Punch); отдельный звук — только если задан в Details.
	if (DamageTakenSound)
	{
		UGameplayStatics::PlaySound2D(this, DamageTakenSound, HonorKitchenAudioSettings::ScaleVolume(0.85f));
	}
	if (ADynamicPostProcess* PP = ResolvePostProcessActor())
	{
		PP->PlayDamagePulse(DamageVignettePulseStrength);
	}
}

void AMyProjectCharacter::PlayLethalDamagePresentation()
{
	bDeathVignetteHeld = true;
	if (UWorld* W = GetWorld())
	{
		DamageScreenFlashEndTime = W->GetTimeSeconds() + 99999.0;
	}
	if (ADynamicPostProcess* PP = ResolvePostProcessActor())
	{
		PP->HoldDamagePulseAtPeak(DamageVignettePulseStrength);
	}
}

void AMyProjectCharacter::ClearDeathVignetteHold()
{
	bDeathVignetteHeld = false;
	DamageScreenFlashEndTime = 0.0;
	if (ADynamicPostProcess* PP = ResolvePostProcessActor())
	{
		PP->ReleaseDamagePulseHold();
	}
}

void AMyProjectCharacter::RestoreStatsFromSave(float Health, float MaxHp, const TArray<FHonorKitchenSavedHotbarSlot>& Slots)
{
	MaxHealth = FMath::Max(1.f, MaxHp);
	CurrentHealth = FMath::Clamp(Health, 0.f, MaxHealth);
	bDead = CurrentHealth <= 0.f;
	HotbarSlots.SetNum(9);
	for (int32 i = 0; i < HotbarSlots.Num(); ++i)
	{
		HotbarSlots[i].ItemType = EInventoryItemType::None;
		HotbarSlots[i].Amount = 0;
	}
	for (int32 i = 0; i < Slots.Num() && i < HotbarSlots.Num(); ++i)
	{
		const EInventoryItemType T = static_cast<EInventoryItemType>(Slots[i].ItemType);
		if (T != EInventoryItemType::None && Slots[i].Amount > 0)
		{
			HotbarSlots[i].ItemType = T;
			HotbarSlots[i].Amount = Slots[i].Amount;
		}
	}
}

void AMyProjectCharacter::BuildHotbarForSave(TArray<FHonorKitchenSavedHotbarSlot>& Out) const
{
	Out.Reset();
	Out.Reserve(HotbarSlots.Num());
	for (const FHotbarSlot& Slot : HotbarSlots)
	{
		FHonorKitchenSavedHotbarSlot S;
		S.ItemType = static_cast<uint8>(Slot.ItemType);
		S.Amount = Slot.Amount;
		Out.Add(S);
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
	return TryAddItemToHotbarInternal(ItemType, Amount, false);
}

bool AMyProjectCharacter::TryAddItemToHotbarInternal(EInventoryItemType ItemType, int32 Amount, bool bBypassBatteryObjective)
{
	if (ItemType == EInventoryItemType::None || Amount <= 0)
	{
		return false;
	}

	// Батарейки — это цель уровня, а не предмет хотбара.
	if (!bBypassBatteryObjective && ItemType == EInventoryItemType::Battery)
	{
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			GM->NotifyBatteryCollected(Amount);
		}
		return true;
	}

	int32 Remaining = Amount;

	const int32 StackLimit = FMath::Max(1, MaxItemsPerSlot);
	for (FHotbarSlot& Slot : HotbarSlots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Slot.ItemType != ItemType || Slot.Amount <= 0)
		{
			continue;
		}
		const int32 FreeSpace = StackLimit - Slot.Amount;
		if (FreeSpace <= 0)
		{
			continue;
		}
		const int32 Added = FMath::Min(FreeSpace, Remaining);
		Slot.Amount += Added;
		Remaining -= Added;
	}

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
		const int32 Added = FMath::Min(StackLimit, Remaining);
		Slot.Amount = Added;
		Remaining -= Added;
	}

	if (Remaining > 0)
	{
		HonorKitchenDevDebug::OnScreen(1.2f, FColor::Orange, TEXT("Хотбар заполнен"));
	}

	RefreshLegacyCrumbCount();
	return Remaining < Amount;
}

void AMyProjectCharacter::GrantTestLoadout()
{
	if (!HonorKitchenAudioSettings::IsDeveloperMode())
	{
		return;
	}
	RefillItemTypeToAmount(EInventoryItemType::Medkit, 3);
	GrantTestItem(EInventoryItemType::Battery, 3);
	RefillItemTypeToAmount(EInventoryItemType::Water, 3);
	RefillItemTypeToAmount(EInventoryItemType::Crumb, 3);
	RefillItemTypeToAmount(EInventoryItemType::Salt, 3);
}

void AMyProjectCharacter::GrantTestItem(EInventoryItemType ItemType, int32 TargetAmount)
{
	if (!HonorKitchenAudioSettings::IsDeveloperMode())
	{
		return;
	}
	if (ItemType == EInventoryItemType::Battery)
	{
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			const int32 Goal = FMath::Max(TargetAmount, GM->GetRequiredBatteries());
			const int32 Need = FMath::Max(0, Goal - GM->GetCollectedBatteries());
			if (Need > 0)
			{
				GM->NotifyBatteryCollected(Need);
			}
		}
		return;
	}

	RefillItemTypeToAmount(ItemType, FMath::Max(1, TargetAmount));
}

int32 AMyProjectCharacter::CountItemInHotbar(EInventoryItemType ItemType) const
{
	int32 Total = 0;
	for (const FHotbarSlot& Slot : HotbarSlots)
	{
		if (Slot.ItemType == ItemType && Slot.Amount > 0)
		{
			Total += Slot.Amount;
		}
	}
	return Total;
}

void AMyProjectCharacter::RefillItemTypeToAmount(EInventoryItemType ItemType, int32 TargetAmount)
{
	if (ItemType == EInventoryItemType::None || TargetAmount <= 0)
	{
		return;
	}

	const int32 Current = CountItemInHotbar(ItemType);
	if (Current >= TargetAmount)
	{
		return;
	}

	TryAddItemToHotbarInternal(ItemType, TargetAmount - Current, true);
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

	APickupBase* Best = nullptr;
	float BestDistSq = FLT_MAX;
	const FVector Origin = GetActorLocation();
	const float MaxDistSq = FMath::Square(960.f);
	float NearestAnyDistSq = FLT_MAX;
	int32 ScannedCount = 0;
	for (TActorIterator<APickupBase> It(GetWorld()); It; ++It)
	{
		APickupBase* P = *It;
		if (!P || P->IsActorBeingDestroyed())
		{
			continue;
		}
		++ScannedCount;
		const float D = FVector::DistSquared(Origin, P->GetActorLocation());
		NearestAnyDistSq = FMath::Min(NearestAnyDistSq, D);
		if (D <= MaxDistSq && D < BestDistSq)
		{
			BestDistSq = D;
			Best = P;
		}
	}

	{
		const float Nearest = (NearestAnyDistSq < FLT_MAX) ? FMath::Sqrt(NearestAnyDistSq) : -1.f;
		const FString Info = FString::Printf(TEXT("Pickup scan: actors=%d nearest=%.0f"), ScannedCount, Nearest);
		HonorKitchenDevDebug::OnScreen(1.4f, FColor::Silver, Info);
	}

	if (!Best)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemplateCharacter, Warning, TEXT("TryPickupNearbyItem: nothing in radius %.0f"), FMath::Sqrt(MaxDistSq));
#endif
		return false;
	}

	const bool bCollected = Best->TryCollect(this);
	HonorKitchenDevDebug::OnScreen(
		1.2f,
		bCollected ? FColor::Green : FColor::Red,
		bCollected ? TEXT("Pickup success") : TEXT("Pickup failed (slot full?)"));
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
		HonorKitchenDevDebug::OnScreen(1.2f, FColor::Yellow, TEXT("Батарейку нельзя выбрасывать"));
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

void AMyProjectCharacter::CycleActiveHotbarSlot(int32 Delta)
{
	if (HotbarSlots.Num() <= 0 || Delta == 0)
	{
		return;
	}
	const int32 Count = HotbarSlots.Num();
	const int32 NormalizedDelta = Delta > 0 ? 1 : -1;
	ActiveHotbarIndex = (ActiveHotbarIndex + NormalizedDelta + Count) % Count;
}

void AMyProjectCharacter::ToggleFlashlight()
{
	if (!FlashlightComponent || bDead)
	{
		return;
	}
	FlashlightComponent->SetVisibility(!FlashlightComponent->IsVisible());
}

void AMyProjectCharacter::TogglePortalNavigator()
{
	if (!HonorKitchenAudioSettings::IsDeveloperMode() || bDead || !PortalNavigator)
	{
		return;
	}
	PortalNavigator->ToggleNavigator();
}

void AMyProjectCharacter::ForcePortalNavigatorOff()
{
	if (PortalNavigator)
	{
		PortalNavigator->SetNavigatorEnabled(false);
	}
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
		HonorKitchenDevDebug::OnScreen(1.f, FColor::Yellow, TEXT("Выбери слот с крошкой"));
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

namespace
{
	bool IsLegacyTemplateHitSound(const USoundBase* Sound)
	{
		if (!Sound)
		{
			return false;
		}
		const FString Path = Sound->GetPathName();
		return Path.Contains(TEXT("FirstPersonTemplateWeaponFire"), ESearchCase::IgnoreCase)
			|| Path.Contains(TEXT("/Engine/EditorSounds/"), ESearchCase::IgnoreCase);
	}
}

void AMyProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	if (IsLegacyTemplateHitSound(DamageTakenSound.Get()))
	{
		DamageTakenSound = nullptr;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}

	bDead = false;
	CurrentHealth = MaxHealth;
	HotbarSlots.SetNum(9);
	for (FHotbarSlot& Slot : HotbarSlots)
	{
		Slot = FHotbarSlot();
	}
	ActiveHotbarIndex = 0;
	RefreshLegacyCrumbCount();

	EnsureInputAssetsLoaded();

	if (PerceptionStimuliSource)
	{
		PerceptionStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
		PerceptionStimuliSource->RegisterWithPerceptionSystem();
	}

	// Если possession уже есть (игрок уже владеет пешкой), а BeginPlay выполнился позже — добавить контекст.
	RegisterDefaultMappingContext();
}

void AMyProjectCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	EnsureInputAssetsLoaded();
	RegisterDefaultMappingContext();
}

void AMyProjectCharacter::UnPossessed()
{
	RemoveDefaultMappingContext();
	Super::UnPossessed();
}

void AMyProjectCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveDefaultMappingContext();
	Super::EndPlay(EndPlayReason);
}

void AMyProjectCharacter::EnsureInputAssetsLoaded()
{
	if (!DefaultMappingContext)
	{
		DefaultMappingContext = Cast<UInputMappingContext>(
			FSoftObjectPath(TEXT("/Game/FirstPerson/Input/IMC_Default.IMC_Default")).TryLoad());
	}
	if (!JumpAction)
	{
		JumpAction = Cast<UInputAction>(
			FSoftObjectPath(TEXT("/Game/FirstPerson/Input/Actions/IA_Jump.IA_Jump")).TryLoad());
	}
	if (!MoveAction)
	{
		MoveAction =
			Cast<UInputAction>(FSoftObjectPath(TEXT("/Game/FirstPerson/Input/Actions/IA_Move.IA_Move")).TryLoad());
	}
	if (!LookAction)
	{
		LookAction =
			Cast<UInputAction>(FSoftObjectPath(TEXT("/Game/FirstPerson/Input/Actions/IA_Look.IA_Look")).TryLoad());
	}
	if (!LookAction)
	{
		LookAction =
			Cast<UInputAction>(FSoftObjectPath(TEXT("/Game/FirstPerson/Input/Actions/IA_MouseLook.IA_MouseLook")).TryLoad());
	}
	if (!ThrowCrumbAction)
	{
		ThrowCrumbAction =
			Cast<UInputAction>(FSoftObjectPath(TEXT("/Game/FirstPerson/Input/Actions/IA_Throw.IA_Throw")).TryLoad());
	}
}

void AMyProjectCharacter::RegisterDefaultMappingContext()
{
	if (bDefaultInputMappingRegistered || !DefaultMappingContext)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return;
	}

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
		bDefaultInputMappingRegistered = true;
	}
}

void AMyProjectCharacter::RemoveDefaultMappingContext()
{
	if (!DefaultMappingContext || !bDefaultInputMappingRegistered)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		bDefaultInputMappingRegistered = false;
		return;
	}

	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
		}
	}
	bDefaultInputMappingRegistered = false;
}

void AMyProjectCharacter::MoveForwardLegacy(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMyProjectCharacter::MoveRightLegacy(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}


//////////////////////////////////////////////////////////////////////////// Input

void AMyProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	EnsureInputAssetsLoaded();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const bool bEnhancedReady =
		EnhancedInputComponent && DefaultMappingContext && JumpAction && MoveAction && LookAction;

	if (bEnhancedReady)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Look);
		if (ThrowCrumbAction)
		{
			EnhancedInputComponent->BindAction(ThrowCrumbAction, ETriggerEvent::Started, this, &AMyProjectCharacter::ThrowCrumb);
		}
	}
	else
	{
		if (!EnhancedInputComponent)
		{
			UE_LOG(LogTemplateCharacter, Warning, TEXT("'%s' Enhanced Input недоступен — используются классические оси DefaultInput."), *GetNameSafe(this));
		}
		PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
		PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
		PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AMyProjectCharacter::MoveForwardLegacy);
		PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AMyProjectCharacter::MoveRightLegacy);
		PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
		PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	}

	// Независимо от MappingContext: прямой биндинг клавиши фонарика.
	// На русской раскладке это та же физическая клавиша (буква "А"), конфликтов с движением влево нет.
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AMyProjectCharacter::ToggleFlashlight);
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