// Copyright Epic Games, Inc. All Rights Reserved.

#include "TomatoSaurusAIController.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"

namespace
{
	const TCHAR* TomatoStateToText(ATomatoSaurusAIController::ETomatoAIState State)
	{
		switch (State)
		{
		case ATomatoSaurusAIController::ETomatoAIState::IdlePatrol: return TEXT("Idle");
		case ATomatoSaurusAIController::ETomatoAIState::InvestigateNoise: return TEXT("Investigate");
		case ATomatoSaurusAIController::ETomatoAIState::ChaseTarget: return TEXT("Chase");
		case ATomatoSaurusAIController::ETomatoAIState::MeleeApproach: return TEXT("Melee");
		default: return TEXT("Unknown");
		}
	}
}

ATomatoSaurusAIController::ATomatoSaurusAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
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
	bHasHomeLocation = false;
	bDirectMeleeApproach = false;
	bDirectMoveToIdle = false;
	CurrentState = ETomatoAIState::IdlePatrol;
	Super::OnUnPossess();
}

void ATomatoSaurusAIController::SetChaseTarget(AActor* Target)
{
	CurrentTarget = Target;
	bDirectMoveToIdle = false;
	CurrentState = Target ? ETomatoAIState::ChaseTarget : ETomatoAIState::IdlePatrol;
	if (Target && GetWorld())
	{
		LastKnownTargetLocation = Target->GetActorLocation();
		TargetMemoryUntil = GetWorld()->GetTimeSeconds() + TargetMemorySeconds;
	}
}

void ATomatoSaurusAIController::NotifyHeardNoise(FVector NoiseWorldLocation, float PursueSeconds)
{
	if (!GetWorld())
	{
		return;
	}
	NoisePursuitLocation = NoiseWorldLocation;
	NoisePursuitUntil = GetWorld()->GetTimeSeconds() + PursueSeconds;
	bDirectMoveToIdle = false;
	CurrentState = ETomatoAIState::InvestigateNoise;
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
		TargetMemoryUntil = GetWorld()->GetTimeSeconds() + TargetMemorySeconds;
	}
}

void ATomatoSaurusAIController::ClearChaseTarget()
{
	CurrentTarget = nullptr;
	bDirectMeleeApproach = false;
	bDirectMoveToIdle = false;
	CurrentState = ETomatoAIState::IdlePatrol;
}

void ATomatoSaurusAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Now = W->GetTimeSeconds();
	if (Now < NoisePursuitUntil)
	{
		return;
	}

	if (CurrentTarget.IsValid() && TargetMemorySeconds > 0.f && Now > TargetMemoryUntil)
	{
		CurrentTarget = nullptr;
		bDirectMeleeApproach = false;
		InvestigateLastSeen(LastKnownTargetLocation, InvestigateAfterLostTargetSeconds);
	}

	APawn* P = GetPawn();
	ACharacter* C = P ? Cast<ACharacter>(P) : nullptr;
	if (!C)
	{
		return;
	}

	if (bDirectMeleeApproach && CurrentTarget.IsValid())
	{
		CurrentState = ETomatoAIState::MeleeApproach;
		const FVector ToTarget = CurrentTarget->GetActorLocation() - P->GetActorLocation();
		const float DistXY = ToTarget.Size2D();
		if (DistXY <= MeleeDirectStopDistanceXY)
		{
			return;
		}

		const FVector Dir = FVector(ToTarget.X, ToTarget.Y, 0.f).GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			C->AddMovementInput(Dir, 1.f);
		}
		return;
	}

	if (bDirectMoveToIdle)
	{
		CurrentState = ETomatoAIState::IdlePatrol;
		const FVector ToIdle = IdleDestination - P->GetActorLocation();
		const float DistXY = ToIdle.Size2D();
		if (DistXY <= FMath::Max(AcceptanceRadius, 50.f))
		{
			bDirectMoveToIdle = false;
			return;
		}

		const FVector Dir = FVector(ToIdle.X, ToIdle.Y, 0.f).GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			C->AddMovementInput(Dir, 0.9f);
		}
	}

#if !UE_BUILD_SHIPPING
	if (bShowStateDebug && P)
	{
		DrawDebugString(W, P->GetActorLocation() + FVector(0.f, 0.f, 140.f), TomatoStateToText(CurrentState), nullptr, FColor::Cyan, 0.f, true);
	}
#endif
}

void ATomatoSaurusAIController::ChaseTick()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const float Now = W->GetTimeSeconds();
	if (Now < NoisePursuitUntil)
	{
		CurrentState = ETomatoAIState::InvestigateNoise;
		bDirectMeleeApproach = false;
		bDirectMoveToIdle = false;
		// UE 5.3: Dest, Radius, StopOnOverlap, Pathfinding, ProjectToNav, CanStrafe, Filter, PartialPath
		const EPathFollowingRequestResult::Type Req = MoveToLocation(NoisePursuitLocation, AcceptanceRadius, true, true, false, false, nullptr, true);
		if (Req == EPathFollowingRequestResult::Failed)
		{
			IdleDestination = NoisePursuitLocation;
			bDirectMoveToIdle = true;
		}
		return;
	}

	if (!CurrentTarget.IsValid())
	{
		CurrentState = ETomatoAIState::IdlePatrol;
		bDirectMeleeApproach = false;
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
			const EPathFollowingRequestResult::Type Req = MoveToLocation(IdleDestination, FMath::Max(18.f, AcceptanceRadius * 0.5f), true, true, false, false, nullptr, true);
			bDirectMoveToIdle = (Req == EPathFollowingRequestResult::Failed);
			NextIdleDecisionTime = Now + FMath::FRandRange(IdleDecisionIntervalMin, IdleDecisionIntervalMax);
		}
		return;
	}

	AActor* Target = CurrentTarget.Get();
	APawn* P = GetPawn();
	if (!Target || !P)
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - P->GetActorLocation();
	LastKnownTargetLocation = Target->GetActorLocation();
	const float DistXY = ToTarget.Size2D();

	if (DistXY > MeleeHandoffDistanceXY)
	{
		CurrentState = ETomatoAIState::ChaseTarget;
		bDirectMeleeApproach = false;
		bDirectMoveToIdle = false;
		const EPathFollowingRequestResult::Type Req = MoveToActor(Target, AcceptanceRadius, true, true, false, nullptr, true);
		if (Req == EPathFollowingRequestResult::Failed)
		{
			bDirectMeleeApproach = true;
		}
		return;
	}

	if (DistXY > MeleeDirectStopDistanceXY)
	{
		if (!bDirectMeleeApproach)
		{
			StopMovement();
			bDirectMeleeApproach = true;
			CurrentState = ETomatoAIState::MeleeApproach;
		}
		return;
	}

	bDirectMeleeApproach = false;
	CurrentState = ETomatoAIState::ChaseTarget;
}
