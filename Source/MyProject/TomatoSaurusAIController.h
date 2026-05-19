// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TomatoSaurusAIController.generated.h"

class UWorld;
class ACharacter;
class APawn;

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
		ChaseTarget
	};

	enum class ETomatoLocomotionMode : uint8
	{
		NavFollow,
		DirectApproach,
		Recover
	};
	ATomatoSaurusAIController(const FObjectInitializer& ObjectInitializer);

	/** Вызывается из персонажа при потере цели. */
	void ClearChaseTarget();

	/** Установить цель преследования (игрок). */
	void SetChaseTarget(AActor* Target);

	ETomatoAIState GetTomatoAIState() const { return CurrentState; }

	/** Пауза преследования после удара по игроку («перезарядка»). */
	void BeginPostAttackRecharge(float DurationSeconds = -1.f);

	bool IsAttackRecharging() const;

	UPROPERTY(EditDefaultsOnly, Category = "Audio|AI")
	TObjectPtr<class USoundBase> ChaseStartSound;

	/** Отвлечь на шум крошки / «кнопки». */
	void NotifyHeardNoise(FVector NoiseWorldLocation, float PursueSeconds = 5.f);
	void InvestigateLastSeen(FVector LastSeenLocation, float InvestigateSeconds = 3.f);
	void NotifySightLost(FVector LastSeenLocation);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;
	void TransitionToState(ETomatoAIState NewState, bool bCriticalTransition = false);

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
	bool bDirectChaseClose = false;

	void ChaseTick();

	/** Таймерный тик: расследование шума без цели (nav + fallback). */
	void TickLocomotionInvestigateNoise(float Now);

	/** Таймерный тик: патруль без цели. */
	void TickLocomotionIdlePatrol(UWorld* World, float Now);

	/** Таймерный тик: преследование/миля по цели. */
	void TickLocomotionChaseTarget(APawn* Pawn, AActor* Target, float Now);

	/**
	 * Прямой ввод по дельте к цели на плоскости XY.
	 * @return true если дистанция XY <= ArrivalXY (дошли, движение не добавляется).
	 */
	bool ConsumeDirectMovementToGoalXY(ACharacter* MoveCharacter, const FVector& ToGoalDeltaWorld, float ArrivalXY, float InputScale) const;

	bool HasDirectLOSOnTarget(const AActor* Target) const;
	bool ShouldIssueChaseMoveRequest(const FVector& TargetLocation, float Now) const;
	void MarkChaseMoveRequested(const FVector& TargetLocation, float Now);

	/** Одна точка для MoveToLocation: при ошибке включается direct через IdleDestination и bDirectMoveToIdle. */
	bool TryNavMoveToPoint(const FVector& Goal, float GoalAcceptanceRadius);

	/** Реакция на провал MoveToActor в Chase (fallback melee / дроп цели → investigate). */
	void HandleChaseMoveActorFailed(AActor* Target, float Now, float DistXY);

	/** Watchdog без прогресса: retry / direct melee для средней дистанции. */
	void RecoverChaseAfterNoProgress(AActor* Target, float Now, float DistXY);
	void EnterNavLocomotionMode(bool bClearDirectIdle = true);
	void EnterDirectChaseCloseMode();
	void ResetLocomotionProgressWatchdog();
	float ComputeDynamicProgressThreshold(const APawn* ControlledPawn) const;
	bool UpdateLocomotionProgressWatchdog(const FVector& PawnLocation, float Now, float RequiredProgressXY, float NoProgressTimeout);

	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ChaseRefreshInterval = 0.25f;

	/** Насколько близко считает MoveTo цель достигнутой (дальняя фаза). */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "10.0", ClampMax = "300.0"))
	float AcceptanceRadius = 45.f;

	/**
	 * Если дистанция XY до цели меньше этого — NavMove не запускаем, идём напрямую (Tick).
	 * Должно быть больше типичного «раннего» успеха MoveTo (~100–150 uu).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Chase", meta = (ClampMin = "80.0", ClampMax = "800.0"))
	float ChaseDirectHandoffDistanceXY = 320.f;

	/** При провале MoveToActor в этой зоне используем прямое сближение вместо сброса в Investigate. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Chase", meta = (ClampMin = "200.0", ClampMax = "2000.0"))
	float DirectChaseFallbackDistanceXY = 900.f;

	/** Ниже этого XY в Tick перестаём подталкивать (контакт / зона урона). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Chase", meta = (ClampMin = "5.0", ClampMax = "200.0"))
	float ChaseDirectStopDistanceXY = 42.f;

	/** Остановка врага после нанесения урона (сек). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Chase", meta = (ClampMin = "0.2", ClampMax = "6.0"))
	float PostAttackRechargeSeconds = 1.35f;

	float AttackRechargeUntil = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Idle", meta = (ClampMin = "0.0", ClampMax = "3000.0"))
	float IdlePatrolRadius = 420.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Idle", meta = (ClampMin = "0.3", ClampMax = "8.0"))
	float IdleDecisionIntervalMin = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Idle", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float IdleDecisionIntervalMax = 3.4f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
	bool bShowStateDebug = true;

	/** Минимальное время удержания не-критичных состояний для защиты от дребезга. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MinStateHoldTime = 0.45f;

	/** Запрет быстрого возврата в только что покинутое состояние. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float TransitionDebounceTime = 0.3f;

	/** Пауза между принятием шумовых интентов от одного источника. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float NoiseCooldownPerSource = 0.45f;

	/** Короткое окно, в котором шум игнорируется после входа/потери визуального контакта с целью. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float ChaseNoiseSuppressSeconds = 0.9f;

	/** Сколько секунд терпим провалы MoveTo в преследовании до ухода в Investigate. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float ChaseFailGraceSeconds = 1.2f;

	/** Количество подряд провалов MoveTo в Chase, после которого разрешаем fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxConsecutiveChaseFails = 4;

	/** Интервал проверки прогресса движения во время преследования. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.2", ClampMax = "2.0"))
	float ChaseProgressCheckInterval = 0.6f;

	/** Минимальный сдвиг pawn за интервал, иначе считаем что преследование застряло. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "1.0", ClampMax = "200.0"))
	float MinChaseProgressDistance = 18.f;

	/** Сколько секунд терпим отсутствие прогресса в Nav режиме до recovery. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.2", ClampMax = "5.0"))
	float NavNoProgressTimeoutSeconds = 1.6f;

	/** Сколько секунд терпим отсутствие прогресса в Direct режиме до recovery. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.2", ClampMax = "5.0"))
	float DirectNoProgressTimeoutSeconds = 1.0f;

	/** Минимальный интервал между переизданием MoveToActor в chase. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ChaseRepathCooldownSeconds = 0.35f;

	/** Укороченный cooldown перепути при застревании/серии провалов MoveTo (BUG-011). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ChaseRepathCooldownStuckSeconds = 0.12f;

	/** Минимальный сдвиг цели, после которого можно форсировать новый MoveToActor. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "10.0", ClampMax = "800.0"))
	float ChaseRepathMinTargetShiftXY = 120.f;

	/** После стольких recovery-циклов без прогресса разрешаем принудительный выход из Chase. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Stability", meta = (ClampMin = "2", ClampMax = "20"))
	int32 MaxNoProgressRecoveriesBeforeInvestigate = 4;

	float StateEnteredTime = 0.f;
	float LastStateChangeTime = -1000.f;
	float LastNoiseAcceptedTime = -1000.f;
	float NoiseSuppressedUntil = 0.f;
	float FirstChaseFailTime = -1.f;
	float LastChaseMoveRequestTime = -1000.f;
	float LastChaseProgressSampleTime = -1.f;
	float LocomotionNoProgressAccumulated = 0.f;
	ETomatoAIState PreviousState = ETomatoAIState::IdlePatrol;
	ETomatoLocomotionMode CurrentLocomotionMode = ETomatoLocomotionMode::NavFollow;
	FVector LastChasePawnLocation = FVector::ZeroVector;
	FVector LastLocomotionProgressSampleLocation = FVector::ZeroVector;
	FVector LastRequestedChaseTargetLocation = FVector::ZeroVector;
	int32 ConsecutiveChaseMoveFails = 0;
	int32 ConsecutiveNoProgressSamples = 0;
	int32 RecoveryAttempts = 0;

	/** Сколько секунд помнить цель после потери зрения (LoseSight), затем — расследование последней точки. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Chase", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float TargetMemorySeconds = 5.5f;

	/** Длительность фазы «иду к последней точке / шуму» после потери цели (Investigate). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Investigate", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float InvestigateAfterLostTargetSeconds = 7.0f;

	/** Нижняя оценка скорости для расчёта длительности investigate по шуму. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Investigate", meta = (ClampMin = "80.0", ClampMax = "600.0"))
	float MinInvestigateSpeedForTiming = 220.f;

	/** Запас времени к оценке прибытия в точку investigate. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Investigate", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float InvestigateArrivalPaddingSeconds = 1.2f;
};
