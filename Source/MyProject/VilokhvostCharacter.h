// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HonorKitchenSpriteSubject.h"
#include "VilokhvostCharacter.generated.h"

class UHonorKitchenEnemySpriteComponent;
class UStaticMeshComponent;
class AMyProjectCharacter;

/**
 * Вилохвост (вилка): парит на месте, реагирует на бег в радиусе 4 м (вибрация), узкое зрение 3 м / 30°.
 * После атаки возвращается в исходную точку. Магнит — заглушка под диплом.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Вилохвост (вилка)"))
class MYPROJECT_API AVilokhvostCharacter : public ACharacter, public IHonorKitchenSpriteSubject
{
	GENERATED_BODY()

public:
	AVilokhvostCharacter();

	virtual bool GetSpritePlayerAware() const override;
	virtual bool GetSpriteAttackFrame() const override;
	virtual bool GetSpriteChaseFrame() const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	bool IsIdleHoveringForAmbientAudio() const { return State == EVilokhvostState::IdleHover; }

	/** 0..1 для пост-процесса / HUD при угрозе от вилохвоста. */
	float GetAtmosphereThreatWeight() const;

protected:
	enum class EVilokhvostState : uint8
	{
		IdleHover,
		AttackLunge,
		ReturningHome
	};

	EVilokhvostState State = EVilokhvostState::IdleHover;

	FVector HomeLocation = FVector::ZeroVector;

	/** Куда «выпад» на атаку (кратковременный сдвиг от Home). */
	FVector LungeEndLocation = FVector::ZeroVector;

	float AttackPhaseTimer = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vilokhvost")
	TObjectPtr<UStaticMeshComponent> ForkVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vilokhvost|Sprite")
	TObjectPtr<UHonorKitchenEnemySpriteComponent> EnemySprite;

	/** Радиус чувствительности к бегу (вибрация), см — 4 м. */
	UPROPERTY(EditDefaultsOnly, Category = "Vilokhvost|Sense", meta = (ClampMin = "100.0"))
	float RunVibrationRadiusUU = 400.f;

	/** Зрение: дальность, см — 3 м (для отладки / расширения логики). */
	UPROPERTY(EditDefaultsOnly, Category = "Vilokhvost|Sense", meta = (ClampMin = "50.0"))
	float SightRadiusUU = 300.f;

	/** Половина угла конуса зрения (полный конус = 2×), градусы — 30° суммарно → 15°. */
	UPROPERTY(EditDefaultsOnly, Category = "Vilokhvost|Sense", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float SightHalfAngleDegrees = 15.f;

	/** Длина «выпада» при атаке, см. */
	UPROPERTY(EditDefaultsOnly, Category = "Vilokhvost|Combat", meta = (ClampMin = "10.0"))
	float AttackLungeDistanceUU = 130.f;

	UPROPERTY(EditDefaultsOnly, Category = "Vilokhvost|Combat", meta = (ClampMin = "1.0"))
	float AttackDamage = 40.f;

	/** Скорость возврата на Home (интерполяция), см/с. */
	UPROPERTY(EditDefaultsOnly, Category = "Vilokhvost|Combat", meta = (ClampMin = "50.0"))
	float ReturnSpeedUU = 420.f;

	/** Длительность фазы выпада перед уроном, сек. */
	UPROPERTY(EditDefaultsOnly, Category = "Vilokhvost|Combat", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float AttackWindupSeconds = 0.12f;

	bool bDamageAppliedThisAttack = false;

	AMyProjectCharacter* FindPlayerCharacter() const;

	bool IsPlayerRunningInVibrationRange(const AMyProjectCharacter* Player) const;
	bool IsPlayerInSightCone(const AMyProjectCharacter* Player) const;

	void StartAttackTowards(const FVector& PlayerLocation);
	void ApplyForkDamage(AMyProjectCharacter* Player);
};
