// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InventoryItemTypes.h"
#include "Logging/LogMacros.h"
#include "MyProjectCharacter.generated.h"

class AController;
class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UAIPerceptionStimuliSourceComponent;
class USoundBase;
struct FInputActionValue;
struct FDamageEvent;

class ACrumbProjectile;
class APickupBase;
class APortal;

USTRUCT(BlueprintType)
struct FHotbarSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	EInventoryItemType ItemType = EInventoryItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Amount = 0;

	bool IsEmpty() const { return ItemType == EInventoryItemType::None || Amount <= 0; }
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AMyProjectCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Регистрация в AI Perception (зрение врагов видит игрока). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	UAIPerceptionStimuliSourceComponent* PerceptionStimuliSource;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;
	
public:
	AMyProjectCharacter();

	/** Здоровье (урон от томато-завра и т.д.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.f;

	/** Ниже этой высоты (мир Z) — автоматический рестарт уровня (падение в пустоту). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Safety")
	float FallOffMapRestartZ = -5000.f;

	/** Для HUD: время (сек мира), до которого показывать всплеск урона. */
	double DamageHudExpiresTime = -1.0;

	/** Последняя величина урона для подписи на HUD. */
	float DamageHudLastAmount = 0.f;

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Крошки (подбор APickupBase / ACrumbPickup, бросок ThrowCrumbAction). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crumbs")
	int32 MaxCrumbs = 12;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crumbs")
	int32 CrumbCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FHotbarSlot> HotbarSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 ActiveHotbarIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crumbs", meta = (AllowPrivateAccess = "true"))
	UInputAction* ThrowCrumbAction;

	UPROPERTY(EditDefaultsOnly, Category = "Crumbs")
	TSubclassOf<ACrumbProjectile> CrumbProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Medkit", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float MedkitHealAmount = 35.f;

	UFUNCTION(BlueprintCallable, Category = "Crumbs")
	void AddCrumbs(int32 Amount);

	/** Fallback-вызов броска (для клавиши в PlayerController, если IA_Throw не настроен). */
	UFUNCTION(BlueprintCallable, Category = "Crumbs")
	void TryThrowCrumb();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItemToHotbar(EInventoryItemType ItemType, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryPickupNearbyItem();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteractPortal();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool DropActiveItem();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ThrowActiveItem();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetActiveHotbarSlot(int32 SlotIndex);

	const TArray<FHotbarSlot>& GetHotbarSlots() const { return HotbarSlots; }
	int32 GetActiveHotbarIndex() const { return ActiveHotbarIndex; }

	/** Звук при получении урона (опционально). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USoundBase> DamageTakenSound;

protected:
	virtual void BeginPlay();

public:
		
	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Bool for AnimBP to switch to another animation set */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bHasRifle;

	/** Setter to set the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void SetHasRifle(bool bNewHasRifle);

	/** Getter for the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	bool GetHasRifle();

protected:
	bool bDead = false;

public:
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	/** Горизонтальная скорость ≥ порога — «бег» для Vilokhvost (вибрации 4 м по ТЗ). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|AI")
	float RunSpeedThresholdForVilokhvost = 500.f;

	UFUNCTION(BlueprintPure, Category = "Movement|AI")
	bool IsRunningForVilokhvostAI() const;

protected:
	void HandleDeath();

	void PlayDamageFeedback(float DamageAmount);

	void ThrowCrumb();
	void RefreshLegacyCrumbCount();
	static FString ItemTypeToDisplayName(EInventoryItemType ItemType);

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

