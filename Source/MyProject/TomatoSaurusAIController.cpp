// Copyright Epic Games, Inc. All Rights Reserved.

#include "TomatoSaurusAIController.h"
#include "HonorKitchenAudioDefaults.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenEnemySoundCatalog.h"
#include "KaravaychikCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "HonorKitchenDevDebug.h"
#include "TomatoSaurusCharacter.h"
#include "KaravaychikCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const TCHAR* TomatoStateToText(ATomatoSaurusAIController::ETomatoAIState State)
	{
		switch (State)
		{
		case ATomatoSaurusAIController::ETomatoAIState::IdlePatrol: return TEXT("Idle");
		case ATomatoSaurusAIController::ETomatoAIState::InvestigateNoise: return TEXT("Investigate");
		case ATomatoSaurusAIController::ETomatoAIState::ChaseTarget: return TEXT("Chase");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* EnemyTypeToText(const APawn* Pawn)
	{
		if (!Pawn)
		{
			return TEXT("Enemy");
		}
		if (Pawn->IsA(AKaravaychikCharacter::StaticClass()))
		{
			return TEXT("Karavaychik");
		}
		if (Pawn->IsA(ATomatoSaurusCharacter::StaticClass()))
		{
			return TEXT("TomatoSaurus");
		}
		return TEXT("Enemy");
	}
}

ATomatoSaurusAIController::ATomatoSaurusAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATomatoSaurusAIController::TransitionToState(ETomatoAIState NewState, bool bCriticalTransition)
{
	if (NewState == CurrentState)
	{
		return;
	}

	UWorld* W = GetWorld();
	const float Now = W ? W->GetTimeSeconds() : 0.f;
	if (!bCriticalTransition)
	{
		const bool bHoldActive = (Now - StateEnteredTime) < MinStateHoldTime;
		const bool bBounceBack = (NewState == PreviousState) && ((Now - LastStateChangeTime) < TransitionDebounceTime);
		if (bHoldActive || bBounceBack)
		{
			return;
		}
	}

	const ETomatoAIState OldState = CurrentState;
	PreviousState = CurrentState;
	CurrentState = NewState;
	LastStateChangeTime = Now;
	StateEnteredTime = Now;

	if (NewState == ETomatoAIState::ChaseTarget && OldState != ETomatoAIState::ChaseTarget)
	{
		if (APawn* P = GetPawn())
		{
			const EHonorKitchenEnemySpecies Species = P->IsA(AKaravaychikCharacter::StaticClass())
				? EHonorKitchenEnemySpecies::Karavaychik
				: EHonorKitchenEnemySpecies::TomatoSaurus;
			HonorKitchenEnemySoundCatalog::PlayAt(P, P->GetActorLocation(), Species, EHonorKitchenEnemySoundEvent::Chase, 0.55f, 0.82f);
		}
	}
}

void ATomatoSaurusAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (InPawn)
	{
		HomeLocation = InPawn->GetActorLocation();
		bHasHomeLocation = true;
		IdleDestination = HomeLocation;
		NextIdleDecisionTime = 0.f;
		bDirectMoveToIdle = false;
		CurrentState = ETomatoAIState::IdlePatrol;
		CurrentLocomotionMode = ETomatoLocomotionMode::NavFollow;
		StateEnteredTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		LastStateChangeTime = StateEnteredTime;
		PreviousState = ETomatoAIState::IdlePatrol;
		ResetLocomotionProgressWatchdog();
	}
	if (GetWorld())
	{
		GetWorldTimerManager().SetTimer(ChaseTimerHandle, this, &ATomatoSaurusAIController::ChaseTick, ChaseRefreshInterval, true);
	}
}

void ATomatoSaurusAIController::OnUnPossess()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(ChaseTimerHandle);
	}
	CurrentTarget = nullptr;
	NoisePursuitUntil = 0.f;
	NoiseSuppressedUntil = 0.f;
	FirstChaseFailTime = -1.f;
	LastChaseMoveRequestTime = -1000.f;
	LastChaseProgressSampleTime = -1.f;
	ConsecutiveChaseMoveFails = 0;
	ConsecutiveNoProgressSamples = 0;
	RecoveryAttempts = 0;
	CurrentLocomotionMode = ETomatoLocomotionMode::NavFollow;
	bHasHomeLocation = false;
	bDirectChaseClose = false;
	bDirectMoveToIdle = false;
	CurrentState = ETomatoAIState::IdlePatrol;
	Super::OnUnPossess();
}

void ATomatoSaurusAIController::SetChaseTarget(AActor* Target)
{
	UWorld* W = GetWorld();
	CurrentTarget = Target;
	bDirectMoveToIdle = false;
	TransitionToState(Target ? ETomatoAIState::ChaseTarget : ETomatoAIState::IdlePatrol, true);
	if (Target && W)
	{
		// Прямой контакт с целью имеет более высокий приоритет, чем шум.
		// Сбрасываем расследование шума, иначе возникает дребезг Chase <-> Investigate.
		NoisePursuitUntil = 0.f;
		NoisePursuitLocation = FVector::ZeroVector;

		LastKnownTargetLocation = Target->GetActorLocation();
		TargetMemoryUntil = W->GetTimeSeconds() + TargetMemorySeconds;
		NoiseSuppressedUntil = W->GetTimeSeconds() + ChaseNoiseSuppressSeconds;
		FirstChaseFailTime = -1.f;
		LastChaseMoveRequestTime = -1000.f;
		ConsecutiveChaseMoveFails = 0;
		ConsecutiveNoProgressSamples = 0;
		RecoveryAttempts = 0;
		LastChaseProgressSampleTime = -1.f;
		CurrentLocomotionMode = ETomatoLocomotionMode::NavFollow;
		if (APawn* P = GetPawn())
		{
			LastChasePawnLocation = P->GetActorLocation();
			LastLocomotionProgressSampleLocation = LastChasePawnLocation;
		}
		LastRequestedChaseTargetLocation = LastKnownTargetLocation;
		ResetLocomotionProgressWatchdog();
	}
}

void ATomatoSaurusAIController::NotifyHeardNoise(FVector NoiseWorldLocation, float PursueSeconds)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const float Now = W->GetTimeSeconds();
	if ((Now - LastNoiseAcceptedTime) < NoiseCooldownPerSource)
	{
		return;
	}

	if (Now < NoiseSuppressedUntil)
	{
		return;
	}

	// Активная погона или добивание вплотную: крошка/шум не перебивают цель.
	if (CurrentTarget.IsValid()
		&& (CurrentState == ETomatoAIState::ChaseTarget || CurrentState == ETomatoAIState::ChaseTarget))
	{
		return;
	}

	// Пока есть цель погони (в т.ч. в окне памяти цели), шум не должен
	// перебивать преследование и вызывать переключение состояний.
	if (CurrentTarget.IsValid() && Now <= TargetMemoryUntil)
	{
		return;
	}

	NoisePursuitLocation = NoiseWorldLocation;
	float EffectivePursueSeconds = FMath::Max(0.5f, PursueSeconds);
	if (const APawn* ControlledPawn = GetPawn())
	{
		const float Dist2D = FVector::Dist2D(ControlledPawn->GetActorLocation(), NoiseWorldLocation);

		float SpeedForEstimate = MinInvestigateSpeedForTiming;
		if (const ACharacter* Char = Cast<ACharacter>(ControlledPawn))
		{
			if (const UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				SpeedForEstimate = FMath::Max(SpeedForEstimate, Move->MaxWalkSpeed * 0.8f);
			}
		}

		const float TravelSecondsEstimate = Dist2D / FMath::Max(1.f, SpeedForEstimate);
		EffectivePursueSeconds = FMath::Max(EffectivePursueSeconds, TravelSecondsEstimate + InvestigateArrivalPaddingSeconds);
	}
	NoisePursuitUntil = Now + EffectivePursueSeconds;
	LastNoiseAcceptedTime = Now;
	bDirectMoveToIdle = false;
	TransitionToState(ETomatoAIState::InvestigateNoise, false);
}

void ATomatoSaurusAIController::InvestigateLastSeen(FVector LastSeenLocation, float InvestigateSeconds)
{
	NotifyHeardNoise(LastSeenLocation, InvestigateSeconds);
}

void ATomatoSaurusAIController::NotifySightLost(FVector LastSeenLocation)
{
	LastKnownTargetLocation = LastSeenLocation;
	if (GetWorld())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		TargetMemoryUntil = Now + TargetMemorySeconds;
		NoiseSuppressedUntil = Now + ChaseNoiseSuppressSeconds;
	}
}

void ATomatoSaurusAIController::ClearChaseTarget()
{
	CurrentTarget = nullptr;
	StopMovement();
	bDirectChaseClose = false;
	bDirectMoveToIdle = false;
	CurrentLocomotionMode = ETomatoLocomotionMode::NavFollow;
	RecoveryAttempts = 0;
	ResetLocomotionProgressWatchdog();
	TransitionToState(ETomatoAIState::IdlePatrol, true);
}

void ATomatoSaurusAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	if (bShowStateDebug)
	{
		if (APawn* DebugPawn = GetPawn())
		{
			const FString Label = FString::Printf(TEXT("%s | %s"), EnemyTypeToText(DebugPawn), TomatoStateToText(CurrentState));
			HonorKitchenDevDebug::DrawWorldString(W, DebugPawn->GetActorLocation(), Label, FColor::Cyan, 140.f);
		}
	}

	const float Now = W->GetTimeSeconds();
	if (CurrentTarget.IsValid() && TargetMemorySeconds > 0.f && Now > TargetMemoryUntil)
	{
		AActor* MemTarget = CurrentTarget.Get();
		if (MemTarget && HasDirectLOSOnTarget(MemTarget))
		{
			// Память по таймеру истекла, но цель в LOS — не сбрасываем Chase в Investigate (BUG-015/019).
			TargetMemoryUntil = Now + TargetMemorySeconds;
		}
		else
		{
			CurrentTarget = nullptr;
			bDirectChaseClose = false;
			CurrentLocomotionMode = ETomatoLocomotionMode::Recover;
			InvestigateLastSeen(LastKnownTargetLocation, InvestigateAfterLostTargetSeconds);
		}
	}

	APawn* P = GetPawn();
	ACharacter* C = P ? Cast<ACharacter>(P) : nullptr;
	if (!C)
	{
		return;
	}

	if (IsAttackRecharging())
	{
		StopMovement();
		return;
	}

	if (bDirectChaseClose && CurrentTarget.IsValid())
	{
		EnterDirectChaseCloseMode();
		TransitionToState(ETomatoAIState::ChaseTarget, true);
		const FVector ToTarget = CurrentTarget->GetActorLocation() - P->GetActorLocation();
		const float DynamicMinProgress = ComputeDynamicProgressThreshold(P);
		if (UpdateLocomotionProgressWatchdog(P->GetActorLocation(), Now, DynamicMinProgress, DirectNoProgressTimeoutSeconds))
		{
			RecoverChaseAfterNoProgress(CurrentTarget.Get(), Now, ToTarget.Size2D());
			return;
		}
		ConsumeDirectMovementToGoalXY(C, ToTarget, ChaseDirectStopDistanceXY, 1.f);
		return;
	}

	if (bDirectMoveToIdle)
	{
		CurrentLocomotionMode = ETomatoLocomotionMode::DirectApproach;
		const bool bNoiseInvestigateActive = !CurrentTarget.IsValid() && (Now < NoisePursuitUntil);
		TransitionToState(bNoiseInvestigateActive ? ETomatoAIState::InvestigateNoise : ETomatoAIState::IdlePatrol, false);
		const FVector ToIdle = IdleDestination - P->GetActorLocation();
		if (ConsumeDirectMovementToGoalXY(C, ToIdle, FMath::Max(AcceptanceRadius, 50.f), 0.9f))
		{
			bDirectMoveToIdle = false;
		}
	}

}

bool ATomatoSaurusAIController::ConsumeDirectMovementToGoalXY(ACharacter* MoveCharacter, const FVector& ToGoalDeltaWorld, float ArrivalXY, float InputScale) const
{
	if (!MoveCharacter)
	{
		return true;
	}
	const float DistXY = FVector(ToGoalDeltaWorld.X, ToGoalDeltaWorld.Y, 0.f).Size();
	if (DistXY <= ArrivalXY)
	{
		return true;
	}
	const FVector Dir = FVector(ToGoalDeltaWorld.X, ToGoalDeltaWorld.Y, 0.f).GetSafeNormal();
	if (!Dir.IsNearlyZero())
	{
		MoveCharacter->AddMovementInput(Dir, InputScale);
	}
	return false;
}

void ATomatoSaurusAIController::TickLocomotionInvestigateNoise(float Now)
{
	TransitionToState(ETomatoAIState::InvestigateNoise, false);
	EnterNavLocomotionMode(false);
	if (!bDirectMoveToIdle)
	{
		TryNavMoveToPoint(NoisePursuitLocation, AcceptanceRadius);
	}
}

void ATomatoSaurusAIController::TickLocomotionIdlePatrol(UWorld* W, float Now)
{
	TransitionToState(ETomatoAIState::IdlePatrol, false);
	EnterNavLocomotionMode(false);
	if (!bHasHomeLocation || IdlePatrolRadius <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (Now >= NextIdleDecisionTime)
	{
		FVector Candidate = HomeLocation;
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
		{
			FNavLocation NavLoc;
			if (NavSys->GetRandomReachablePointInRadius(HomeLocation, IdlePatrolRadius, NavLoc))
			{
				Candidate = NavLoc.Location;
			}
		}
		IdleDestination = Candidate;
		const float IdleAccept = FMath::Max(18.f, AcceptanceRadius * 0.5f);
		bDirectMoveToIdle = !TryNavMoveToPoint(IdleDestination, IdleAccept);
		NextIdleDecisionTime = Now + FMath::FRandRange(IdleDecisionIntervalMin, IdleDecisionIntervalMax);
	}
}

void ATomatoSaurusAIController::TickLocomotionChaseTarget(APawn* P, AActor* Target, float Now)
{
	const FVector ToTarget = Target->GetActorLocation() - P->GetActorLocation();
	const FVector PawnLocation = P->GetActorLocation();
	LastKnownTargetLocation = Target->GetActorLocation();
	const float DistXY = ToTarget.Size2D();

	if (DistXY > ChaseDirectHandoffDistanceXY)
	{
		TransitionToState(ETomatoAIState::ChaseTarget, true);
		EnterNavLocomotionMode();
		if (ShouldIssueChaseMoveRequest(Target->GetActorLocation(), Now))
		{
			MarkChaseMoveRequested(Target->GetActorLocation(), Now);
			// bStopOnOverlap=false — скольжение вдоль препятствий вместо частых остановок (BUG-011).
			const EPathFollowingRequestResult::Type Req = MoveToActor(Target, AcceptanceRadius, false, true, false, nullptr, true);
			if (Req == EPathFollowingRequestResult::Failed)
			{
				HandleChaseMoveActorFailed(Target, Now, DistXY);
			}
			else
			{
				FirstChaseFailTime = -1.f;
				ConsecutiveChaseMoveFails = 0;
			}
		}
		if (DistXY > (AcceptanceRadius + 30.f))
		{
			const float DynamicMinProgress = ComputeDynamicProgressThreshold(P);
			if (UpdateLocomotionProgressWatchdog(PawnLocation, Now, DynamicMinProgress, NavNoProgressTimeoutSeconds))
			{
				RecoverChaseAfterNoProgress(Target, Now, DistXY);
			}
		}
		return;
	}

	if (DistXY > ChaseDirectStopDistanceXY)
	{
		if (!bDirectChaseClose)
		{
			EnterDirectChaseCloseMode();
			TransitionToState(ETomatoAIState::ChaseTarget, true);
		}
		return;
	}

	EnterNavLocomotionMode();
	ConsecutiveNoProgressSamples = 0;
	FirstChaseFailTime = -1.f;
	ConsecutiveChaseMoveFails = 0;
	RecoveryAttempts = 0;
	ResetLocomotionProgressWatchdog();
	TransitionToState(ETomatoAIState::ChaseTarget, false);
}

void ATomatoSaurusAIController::BeginPostAttackRecharge(float DurationSeconds)
{
	if (DurationSeconds < 0.f)
	{
		DurationSeconds = PostAttackRechargeSeconds;
	}
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	AttackRechargeUntil = W->GetTimeSeconds() + FMath::Max(0.2f, DurationSeconds);
	StopMovement();
	bDirectChaseClose = false;
	if (ATomatoSaurusCharacter* Tomato = Cast<ATomatoSaurusCharacter>(GetPawn()))
	{
		Tomato->SetAttackRecharging(true);
	}
}

bool ATomatoSaurusAIController::IsAttackRecharging() const
{
	const UWorld* W = GetWorld();
	return W && W->GetTimeSeconds() < AttackRechargeUntil;
}

void ATomatoSaurusAIController::ChaseTick()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const float Now = W->GetTimeSeconds();
	if (IsAttackRecharging())
	{
		StopMovement();
		TransitionToState(ETomatoAIState::ChaseTarget, false);
		return;
	}
	if (ATomatoSaurusCharacter* Tomato = Cast<ATomatoSaurusCharacter>(GetPawn()))
	{
		Tomato->SetAttackRecharging(false);
	}
	if (!CurrentTarget.IsValid() && Now < NoisePursuitUntil)
	{
		TickLocomotionInvestigateNoise(Now);
		return;
	}

	if (!CurrentTarget.IsValid())
	{
		TickLocomotionIdlePatrol(W, Now);
		return;
	}

	AActor* Target = CurrentTarget.Get();
	APawn* P = GetPawn();
	if (!Target || !P)
	{
		return;
	}

	TickLocomotionChaseTarget(P, Target, Now);
}

bool ATomatoSaurusAIController::ShouldIssueChaseMoveRequest(const FVector& TargetLocation, float Now) const
{
	const bool bStruggling = (ConsecutiveChaseMoveFails > 0) || (ConsecutiveNoProgressSamples > 0);
	const float EffectiveCooldown = bStruggling
		? FMath::Min(ChaseRepathCooldownSeconds, ChaseRepathCooldownStuckSeconds)
		: ChaseRepathCooldownSeconds;
	const bool bCooldownElapsed = (Now - LastChaseMoveRequestTime) >= EffectiveCooldown;
	const bool bTargetMovedEnough = FVector::Dist2D(TargetLocation, LastRequestedChaseTargetLocation) >= ChaseRepathMinTargetShiftXY;
	return bCooldownElapsed || bTargetMovedEnough;
}

void ATomatoSaurusAIController::MarkChaseMoveRequested(const FVector& TargetLocation, float Now)
{
	LastRequestedChaseTargetLocation = TargetLocation;
	LastChaseMoveRequestTime = Now;
}

bool ATomatoSaurusAIController::TryNavMoveToPoint(const FVector& Goal, float GoalAcceptanceRadius)
{
	// UE 5.3: Dest, Radius, StopOnOverlap, Pathfinding, ProjectToNav, CanStrafe, Filter, PartialPath
	const EPathFollowingRequestResult::Type Req = MoveToLocation(Goal, GoalAcceptanceRadius, true, true, false, false, nullptr, true);
	if (Req == EPathFollowingRequestResult::Failed)
	{
		IdleDestination = Goal;
		bDirectMoveToIdle = true;
		return false;
	}
	return true;
}

void ATomatoSaurusAIController::HandleChaseMoveActorFailed(AActor* Target, float Now, float DistXY)
{
	if (!Target)
	{
		return;
	}
	++ConsecutiveChaseMoveFails;
	if (FirstChaseFailTime < 0.f)
	{
		FirstChaseFailTime = Now;
	}

	if (DistXY <= DirectChaseFallbackDistanceXY * 1.4f)
	{
		EnterDirectChaseCloseMode();
		TransitionToState(ETomatoAIState::ChaseTarget, true);
	}

	const bool bGraceExpired = (FirstChaseFailTime >= 0.f) && ((Now - FirstChaseFailTime) >= ChaseFailGraceSeconds);
	const bool bTooManyFails = ConsecutiveChaseMoveFails >= MaxConsecutiveChaseFails;
	const bool bCanDropTarget = bGraceExpired && bTooManyFails && !HasDirectLOSOnTarget(Target);
	if (bCanDropTarget)
	{
		LastKnownTargetLocation = Target->GetActorLocation();
		CurrentTarget = nullptr;
		bDirectChaseClose = false;
		CurrentLocomotionMode = ETomatoLocomotionMode::Recover;
		InvestigateLastSeen(LastKnownTargetLocation, InvestigateAfterLostTargetSeconds);
	}
}

void ATomatoSaurusAIController::RecoverChaseAfterNoProgress(AActor* Target, float Now, float DistXY)
{
	if (!Target)
	{
		return;
	}

	CurrentLocomotionMode = ETomatoLocomotionMode::Recover;
	++RecoveryAttempts;
	ConsecutiveNoProgressSamples = 0;
	ResetLocomotionProgressWatchdog();

	const bool bInDirectBand = DistXY <= DirectChaseFallbackDistanceXY * 1.5f;
	if (bInDirectBand)
	{
		EnterDirectChaseCloseMode();
		TransitionToState(ETomatoAIState::ChaseTarget, true);
	}
	else
	{
		EnterNavLocomotionMode();
		const EPathFollowingRequestResult::Type RetryReq = MoveToActor(Target, AcceptanceRadius, false, true, false, nullptr, true);
		MarkChaseMoveRequested(Target->GetActorLocation(), Now);
		if (RetryReq == EPathFollowingRequestResult::Failed)
		{
			HandleChaseMoveActorFailed(Target, Now, DistXY);
		}
	}

	// Дальний "залипший" chase: вместо вечного стояния принудительно выходим в investigate last seen.
	if (RecoveryAttempts >= MaxNoProgressRecoveriesBeforeInvestigate)
	{
		LastKnownTargetLocation = Target->GetActorLocation();
		CurrentTarget = nullptr;
		bDirectChaseClose = false;
		CurrentLocomotionMode = ETomatoLocomotionMode::Recover;
		RecoveryAttempts = 0;
		InvestigateLastSeen(LastKnownTargetLocation, InvestigateAfterLostTargetSeconds);
	}
}

void ATomatoSaurusAIController::EnterNavLocomotionMode(bool bClearDirectIdle)
{
	bDirectChaseClose = false;
	if (bClearDirectIdle)
	{
		bDirectMoveToIdle = false;
	}
	CurrentLocomotionMode = ETomatoLocomotionMode::NavFollow;
}

void ATomatoSaurusAIController::EnterDirectChaseCloseMode()
{
	StopMovement();
	bDirectChaseClose = true;
	bDirectMoveToIdle = false;
	CurrentLocomotionMode = ETomatoLocomotionMode::DirectApproach;
}

void ATomatoSaurusAIController::ResetLocomotionProgressWatchdog()
{
	LastChaseProgressSampleTime = -1.f;
	LocomotionNoProgressAccumulated = 0.f;
	LastLocomotionProgressSampleLocation = FVector::ZeroVector;
}

float ATomatoSaurusAIController::ComputeDynamicProgressThreshold(const APawn* ControlledPawn) const
{
	float DynamicMinProgress = MinChaseProgressDistance;
	if (const ACharacter* CharPawn = Cast<ACharacter>(ControlledPawn))
	{
		if (const UCharacterMovementComponent* Move = CharPawn->GetCharacterMovement())
		{
			DynamicMinProgress = FMath::Clamp(Move->MaxWalkSpeed * ChaseProgressCheckInterval * 0.2f, 6.f, MinChaseProgressDistance);
		}
	}
	return DynamicMinProgress;
}

bool ATomatoSaurusAIController::UpdateLocomotionProgressWatchdog(
	const FVector& PawnLocation,
	float Now,
	float RequiredProgressXY,
	float NoProgressTimeout)
{
	if (LastChaseProgressSampleTime < 0.f)
	{
		LastChaseProgressSampleTime = Now;
		LastLocomotionProgressSampleLocation = PawnLocation;
		LocomotionNoProgressAccumulated = 0.f;
		return false;
	}

	const float Dt = Now - LastChaseProgressSampleTime;
	if (Dt < ChaseProgressCheckInterval)
	{
		return false;
	}

	const float ProgressXY = FVector::Dist2D(PawnLocation, LastLocomotionProgressSampleLocation);
	if (ProgressXY < RequiredProgressXY)
	{
		++ConsecutiveNoProgressSamples;
		LocomotionNoProgressAccumulated += Dt;
	}
	else
	{
		ConsecutiveNoProgressSamples = 0;
		LocomotionNoProgressAccumulated = 0.f;
		RecoveryAttempts = 0;
	}

	LastChaseProgressSampleTime = Now;
	LastLocomotionProgressSampleLocation = PawnLocation;
	return LocomotionNoProgressAccumulated >= NoProgressTimeout;
}

bool ATomatoSaurusAIController::HasDirectLOSOnTarget(const AActor* Target) const
{
	const APawn* P = GetPawn();
	const UWorld* W = GetWorld();
	if (!P || !W || !Target)
	{
		return false;
	}

	const FVector Start = P->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 45.f);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TomatoLOS), true, P);

	FHitResult Hit;
	const bool bHit = W->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);
	return !bHit || Hit.GetActor() == Target;
}
