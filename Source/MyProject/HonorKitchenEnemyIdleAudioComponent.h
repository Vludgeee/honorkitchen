// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HonorKitchenEnemySoundCatalog.h"
#include "Components/ActorComponent.h"
#include "HonorKitchenEnemyIdleAudioComponent.generated.h"

class UAudioComponent;

/** Фоновый idle-луп врага, слышен игроку в радиусе, пока враг «спокоен». */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UHonorKitchenEnemyIdleAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHonorKitchenEnemyIdleAudioComponent();

	/** Макс. дистанция до игрока, на которой слышен idle (см). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Idle Audio", meta = (ClampMin = "500.0"))
	float MaxAudibleDistanceUU = 3200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Idle Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleVolume = 0.42f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> IdleAudioComponent;

	bool bWasPlayingIdle = false;

	EHonorKitchenEnemySpecies ResolveSpecies() const;
	bool IsOwnerCalm() const;
	bool IsPlayerInAudibleRange(float& OutDistanceSq) const;
	void RefreshIdlePlayback();
	void StartIdleLoop();
	void StopIdleLoop();

	UFUNCTION()
	void OnIdleLoopFadeOutFinished();

	bool bIdleStopFadePending = false;
};
