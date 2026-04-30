// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TomatoSaurusAIController.generated.h"

/**
 * Контроллер томато-завра: периодически движется к цели, заданной персонажем (после срабатывания зрения).
 */
UCLASS(Blueprintable)
class MYPROJECT_API ATomatoSaurusAIController : public AAIController
{
	GENERATED_BODY()

public:
	enum class ETomatoAIState : uint8
	{
		IdlePatrol,
		InvestigateNoise,
		ChaseTarget,
		MeleeApproach
	};
	ATomatoSaurusAIController(const FObjectInitializer& ObjectInitializer);

	/** Вызывается из персонажа при потере цели. */
	void ClearChaseTarget();

	/** Установить цель преследования (игрок). */
	void SetChaseTarget(AActor* Target);

	ETomatoAIState GetTomatoAIState() const { return CurrentState; }

	/** Отвлечь на шум крошки / «кнопки». */
	void NotifyHeardNoise(FVector NoiseWorldLocation, float PursueSeconds = 5.f);
	void InvestigateLastSeen(FVector LastSeenLocation, float InvestigateSeconds = 3.f);
	void NotifySightLost(FVector LastSeenLocation);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	FVector NoisePursuitLocation = FVector::ZeroVector;
	float NoisePursuitUntil = 0.f;
	float TargetMemoryUntil = 0.f;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	FVector HomeLocation = FVector::ZeroVector;
	bool bHasHomeLocation = false;
	FVector IdleDestination = FVector::ZeroVector;
	float NextIdleDecisionTime = 0.f;
	bool bDirectMoveToIdle = false;
	ETomatoAIState CurrentState = ETomatoAIState::IdlePatrol;

	FTimerHandle ChaseTimerHandle;

	/** После NavMesh враг часто останавливается из‑за радиуса агента; добиваем шагом без pathfinding. */
	bool bDirectMeleeApproach = false;

	void ChaseTick();

	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ChaseRefreshInterval = 0.25f;

	/** Насколько близко считает MoveTo цель достигнутой (дальняя фаза). */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "10.0", ClampMax = "300.0"))
	float AcceptanceRadius = 45.f;

	/**
	 * Если дистанция XY до цели меньше этого — NavMove не запускаем, идём напрямую (Tick).
	 * Должно быть больше типичного «раннего» успеха MoveTo (~100–150 uu).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Melee", meta = (ClampMin = "80.0", ClampMax = "800.0"))
	float MeleeHandoffDistanceXY = 320.f;

	/** Ниже этого XY в Tick перестаём подталкивать (контакт / зона урона). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Melee", meta = (ClampMin = "5.0", ClampMax = "200.0"))
	float MeleeDirectStopDistanceXY = 42.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Idle", meta = (ClampMin = "0.0", ClampMax = "3000.0"))
	float IdlePatrolRadius = 420.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Idle", meta = (ClampMin = "0.3", ClampMax = "8.0"))
	float IdleDecisionIntervalMin = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Idle", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float IdleDecisionIntervalMax = 3.4f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
	bool bShowStateDebug = true;

	/** Сколько секунд помнить цель после потери зрения (LoseSight), затем — расследование последней точки. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Chase", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float TargetMemorySeconds = 2.5f;

	/** Длительность фазы «иду к последней точке / шуму» после потери цели (Investigate). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Investigate", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float InvestigateAfterLostTargetSeconds = 3.0f;
};
