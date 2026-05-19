// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionTypes.h"
#include "HonorKitchenSpriteSubject.h"
#include "TomatoSaurusCharacter.generated.h"

class UHonorKitchenEnemySpriteComponent;
class UAIPerceptionComponent;
class AMyProjectCharacter;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;
struct FHitResult;

/**
 * Томато-завр: зрение / слух настраиваются в C++ (см. SightRadius, HearingRange), преследование через NavMesh.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Томато-завр (AI враг)"))
class MYPROJECT_API ATomatoSaurusCharacter : public ACharacter, public IHonorKitchenSpriteSubject
{
	GENERATED_BODY()

public:
	ATomatoSaurusCharacter();

	virtual bool GetSpritePlayerAware() const override;
	virtual bool GetSpriteAttackFrame() const override;
	virtual bool GetSpriteChaseFrame() const override;

	virtual void Tick(float DeltaSeconds) override;

	void SyncChaseSpeedToPlayer();

	/** Обновить MaxWalkSpeed по состоянию AI и CrowdControl (соль). */
	void RefreshAIMovementSpeed();

	/** Соль: временное замедление (множитель к базовой скорости состояния). */
	void ApplySaltSlowDebuff(float DurationSeconds, float SpeedMultiplier);

	/** Вода: отключить зрение на время. */
	void ApplyWaterBlindDebuff(float DurationSeconds);

	/** Fallback-детект: почти круговой обзор, кроме узкой зоны строго сзади. */
	bool CanSeePlayerByFallback(const AMyProjectCharacter* Player) const;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
	TObjectPtr<USoundBase> AttackHitSound;

	/** Во время «перезарядки» после удара зона урона отключена. */
	void SetAttackRecharging(bool bRecharging);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Свой меш (импорт FBX/OBJ в Content и перетащи сюда). Пусто — сфера из Engine. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> BodyMesh;

	/** Масштаб видимого меша (подгони под капсулу). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	FVector BodyMeshScale = FVector(0.5f, 0.52f, 0.42f);

	/**
	 * Материалы на слоты меша (Element 0, 1, …). Если импорт «серый» — задай сюда M_* или MI_*.
	 * Пустой массив = оставить то, что в ассете Static Mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UMaterialInterface>> BodyMaterialOverrides;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UAIPerceptionComponent> PerceptionComponent;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** Дальность зрения, см. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "100.0"))
	float SightRadiusUU = 2200.f;

	/** Дальность «потери» цели после выхода из SightRadius, см. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "100.0"))
	float LoseSightRadiusUU = 3000.f;

	/** Половина угла зрения, градусы (полный конус = 2x). 170 => слепая зона только сзади. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float SightHalfAngleDegrees = 170.f;

	/** Дальность слуха, см. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "100.0"))
	float HearingRangeUU = 2600.f;

	/** Вблизи последней замеченной позиции цель «авто-видима», чтобы не терять в упор. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
	float AutoSuccessRangeFromLastSeenUU = 450.f;

	/** Слепая зона сзади: если dot(Forward, ToPlayer) <= порога, считаем что цель "за спиной". */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "-0.999", ClampMax = "0.5"))
	float RearBlindDotThreshold = -0.96f;

	/**
	 * Вплотную цель должна детектиться независимо от направления взгляда,
	 * чтобы враг не "слеп" в ближнем контакте.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "0.0", ClampMax = "800.0"))
	float CloseRangeAutoDetectUU = 320.f;

	/** Минимальный интервал между повторными fallback-подтверждениями зрения. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float FallbackSightRefreshCooldown = 0.35f;

	/** Сколько секунд идти к точке шума (Investigate по слуху), сек. */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float NoisePursuitSeconds = 7.0f;

	/** Скорости передвижения, см/с: патруль / расследование / погоня. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "50.0"))
	float IdleMoveSpeed = 220.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "50.0"))
	float InvestigateMoveSpeed = 290.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "50.0"))
	float ChaseMoveSpeed = 670.f;

	/** Погоня: доля MaxWalkSpeed игрока (>1 — догоняет при прямой погоне). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.5", ClampMax = "1.35"))
	float ChaseSpeedVsPlayerRatio = 1.08f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> TomatoMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh|Sprite")
	TObjectPtr<UHonorKitchenEnemySpriteComponent> EnemySprite;

	/** Зона контакта с игроком: тикающий урон, пока персонаж внутри. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USphereComponent> DamageSphere;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float DamagePerTick = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float DamageTickInterval = 0.35f;

	FTimerHandle DamageTickTimer;

	TWeakObjectPtr<AMyProjectCharacter> DamageTarget;
	bool bHasSightOnPlayer = false;
	float LastFallbackSightNotifyTime = -1000.f;

	/** Множитель скорости от соли (1 = норма). */
	float CrowdControlSpeedMultiplier = 1.f;

	FTimerHandle SaltSlowTimerHandle;
	FTimerHandle WaterBlindTimerHandle;
	UFUNCTION()
	void ClearSaltSlowDebuff();

	UFUNCTION()
	void ClearWaterBlindDebuff();

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void ApplyPerceptionSettings();

	/** Реакция на игрока в зоне зрения (для переопределения, напр. Karavaychik Alert). */
	virtual void OnPlayerSightGained(AMyProjectCharacter* Player);

	virtual void OnPlayerSightLost(FVector LastSeenLocation);

	UFUNCTION()
	void OnDamageOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDamageOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void ApplyDamageTick();

	bool bAttackRecharging = false;

	bool SetupEnemySpriteVisual();
};
