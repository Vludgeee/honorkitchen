// Copyright Epic Games, Inc. All Rights Reserved.

#include "VilokhvostCharacter.h"
#include "MyProjectCharacter.h"
#include "MyProjectGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AVilokhvostCharacter::AVilokhvostCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(28.f, 48.f);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	ForkVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForkVisual"));
	ForkVisual->SetupAttachment(GetCapsuleComponent());
	ForkVisual->SetRelativeLocation(FVector(0.f, 0.f, -24.f));
	ForkVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ForkVisual->SetStaticMesh(CubeMesh.Object);
		ForkVisual->SetRelativeScale3D(FVector(0.12f, 0.45f, 0.06f));
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
		Move->GravityScale = 0.f;
		Move->MaxFlySpeed = 0.f;
		Move->BrakingDecelerationFlying = 2400.f;
		Move->bOrientRotationToMovement = false;
	}
}

void AVilokhvostCharacter::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetActorLocation();
}

AMyProjectCharacter* AVilokhvostCharacter::FindPlayerCharacter() const
{
	if (UWorld* W = GetWorld())
	{
		if (APawn* P = UGameplayStatics::GetPlayerPawn(W, 0))
		{
			return Cast<AMyProjectCharacter>(P);
		}
	}
	return nullptr;
}

bool AVilokhvostCharacter::IsPlayerRunningInVibrationRange(const AMyProjectCharacter* Player) const
{
	if (!Player || Player->IsDead())
	{
		return false;
	}
	const float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
	if (Dist > RunVibrationRadiusUU)
	{
		return false;
	}
	return Player->IsRunningForVilokhvostAI();
}

bool AVilokhvostCharacter::IsPlayerInSightCone(const AMyProjectCharacter* Player) const
{
	if (!Player)
	{
		return false;
	}
	const FVector ToTarget = Player->GetActorLocation() - GetActorLocation();
	const float Dist = ToTarget.Size();
	if (Dist > SightRadiusUU + KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector Forward = GetActorForwardVector();
	const FVector ToNorm = ToTarget.GetSafeNormal();
	const float CosAngle = FVector::DotProduct(Forward, ToNorm);
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(SightHalfAngleDegrees));
	return CosAngle >= CosLimit;
}

void AVilokhvostCharacter::StartAttackTowards(const FVector& PlayerLocation)
{
	State = EVilokhvostState::AttackLunge;
	AttackPhaseTimer = AttackWindupSeconds;
	bDamageAppliedThisAttack = false;

	FVector FlatDir = PlayerLocation - HomeLocation;
	FlatDir.Z = 0.f;
	FlatDir = FlatDir.GetSafeNormal();
	if (FlatDir.IsNearlyZero())
	{
		FlatDir = GetActorForwardVector();
	}
	LungeEndLocation = HomeLocation + FlatDir * AttackLungeDistanceUU;
	SetActorLocation(LungeEndLocation);
}

void AVilokhvostCharacter::ApplyForkDamage(AMyProjectCharacter* Player)
{
	if (!Player || Player->IsDead())
	{
		return;
	}
	UGameplayStatics::ApplyDamage(Player, AttackDamage, nullptr, this, UDamageType::StaticClass());
	if (UWorld* W = GetWorld())
	{
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(W->GetAuthGameMode()))
		{
			GM->NotifyAIDetectedPlayer();
		}
	}
}

void AVilokhvostCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AMyProjectCharacter* Player = FindPlayerCharacter();
	if (!Player)
	{
		return;
	}

	switch (State)
	{
	case EVilokhvostState::IdleHover:
	{
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		SetActorLocation(HomeLocation);

		// ТЗ: атака при беге в радиусе вибрации 4 м; зрение 3 м / 30° — доп. условие «видит бег»
		const bool bRunVibration = IsPlayerRunningInVibrationRange(Player);
		const bool bSeeRun = IsPlayerInSightCone(Player) && Player->IsRunningForVilokhvostAI();
		if (bRunVibration || bSeeRun)
		{
			StartAttackTowards(Player->GetActorLocation());
		}
		break;
	}
	case EVilokhvostState::AttackLunge:
	{
		AttackPhaseTimer -= DeltaSeconds;
		if (!bDamageAppliedThisAttack && AttackPhaseTimer <= 0.f)
		{
			ApplyForkDamage(Player);
			bDamageAppliedThisAttack = true;
			State = EVilokhvostState::ReturningHome;
		}
		break;
	}
	case EVilokhvostState::ReturningHome:
	{
		const FVector Cur = GetActorLocation();
		const FVector Next = FMath::VInterpConstantTo(Cur, HomeLocation, DeltaSeconds, ReturnSpeedUU);
		SetActorLocation(Next);
		if (FVector::Dist(Next, HomeLocation) < 8.f)
		{
			SetActorLocation(HomeLocation);
			State = EVilokhvostState::IdleHover;
		}
		break;
	}
	default:
		break;
	}
}
