// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionTypes.h"
#include "TomatoSaurusCharacter.generated.h"

class UAIPerceptionComponent;
class AMyProjectCharacter;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
struct FHitResult;

/**
 * Томато-завр: зрение / слух настраиваются в C++ (см. SightRadius, HearingRange), преследование через NavMesh.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Томато-завр (AI враг)"))
class MYPROJECT_API ATomatoSaurusCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATomatoSaurusCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** Обновить MaxWalkSpeed по состоянию AI и CrowdControl (соль). */
	void RefreshAIMovementSpeed();

	/** Соль: временное замедление (множитель к базовой скорости состояния). */
	void ApplySaltSlowDebuff(float DurationSeconds, float SpeedMultiplier);

	/** Вода: отключить зрение на время. */
	void ApplyWaterBlindDebuff(float DurationSeconds);

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

	/** Сколько секунд идти к точке шума (Investigate по слуху), сек. */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float NoisePursuitSeconds = 4.5f;

	/** Скорости передвижения, см/с: патруль / расследование / погоня. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "50.0"))
	float IdleMoveSpeed = 220.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "50.0"))
	float InvestigateMoveSpeed = 290.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "50.0"))
	float ChaseMoveSpeed = 460.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> TomatoMesh;

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

	/** Реакция на игрока в зоне зрения (для переопределения, напр. Karavaychik Alert). */
	virtual void OnPlayerSightGained(AMyProjectCharacter* Player);

	virtual void OnPlayerSightLost(FVector LastSeenLocation);

	UFUNCTION()
	void OnDamageOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDamageOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void ApplyDamageTick();
};
