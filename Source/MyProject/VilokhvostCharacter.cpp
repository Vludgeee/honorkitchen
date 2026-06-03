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
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HonorKitchenEnemySpriteComponent.h"
#include "HonorKitchenEnemySpriteCatalog.h"
#include "HonorKitchenDevDebug.h"
#include "HonorKitchenEnemySoundCatalog.h"
#include "HonorKitchenEnemyIdleAudioComponent.h"

AVilokhvostCharacter::AVilokhvostCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(28.f, 48.f);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	ForkVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForkVisual"));
	ForkVisual->SetupAttachment(GetCapsuleComponent());
	ForkVisual->SetRelativeLocation(FVector(0.f, 0.f, -24.f));
	ForkVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	EnemySprite = CreateDefaultSubobject<UHonorKitchenEnemySpriteComponent>(TEXT("EnemySprite"));
	EnemySprite->SetupAttachment(GetCapsuleComponent());
	EnemySprite->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
	EnemySprite->SpriteWorldSizeUU = 280.f;
	EnemySprite->bBillboardFaceCamera = true;
	EnemySprite->SpriteFacingYawOffset = 0.f;
	EnemySprite->bFlipSpriteVertical = true;
	EnemySprite->SetSpriteFrames(HonorKitchenEnemySpriteCatalog::MakeVilokhvostFrames());
	EnemySprite->SetLegacyVisualToHide(ForkVisual);

	CreateDefaultSubobject<UHonorKitchenEnemyIdleAudioComponent>(TEXT("EnemyIdleAudio"));

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
	if (EnemySprite)
	{
		EnemySprite->RefreshSpriteVisual();
	}
	if (ForkVisual && EnemySprite && EnemySprite->IsSpriteActive())
	{
		return;
	}
	if (ForkVisual)
	{
		ForkVisual->SetRelativeScale3D(FVector(0.09f, 0.62f, 0.05f));
		if (UMaterialInterface* BaseShapeMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			ForkVisual->SetMaterial(0, BaseShapeMaterial);
			if (UMaterialInstanceDynamic* Mid = ForkVisual->CreateAndSetMaterialInstanceDynamic(0))
			{
				const FLinearColor ForkColor(0.46f, 0.08f, 0.56f, 1.f);
				Mid->SetVectorParameterValue(TEXT("Color"), ForkColor);
				Mid->SetVectorParameterValue(TEXT("BaseColor"), ForkColor);
				Mid->SetVectorParameterValue(TEXT("EmissiveColor"), ForkColor * 0.18f);
			}
		}
	}
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
	HonorKitchenEnemySoundCatalog::PlayAt(
		this, GetActorLocation(), EHonorKitchenEnemySpecies::Vilokhvost, EHonorKitchenEnemySoundEvent::Detected, 1.f);

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

	HonorKitchenEnemySoundCatalog::PlayAt(
		this, GetActorLocation(), EHonorKitchenEnemySpecies::Vilokhvost, EHonorKitchenEnemySoundEvent::Punch, 0.95f);

	UGameplayStatics::ApplyDamage(Player, AttackDamage, nullptr, this, UDamageType::StaticClass());
	if (UWorld* W = GetWorld())
	{
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(W->GetAuthGameMode()))
		{
			GM->NotifyAIDetectedPlayer();
		}
	}
}

float AVilokhvostCharacter::GetAtmosphereThreatWeight() const
{
	switch (State)
	{
	case EVilokhvostState::AttackLunge:
		return 0.92f;
	case EVilokhvostState::ReturningHome:
		return 0.38f;
	default:
		return 0.f;
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

	if (UWorld* W = GetWorld())
	{
		const TCHAR* StateText = TEXT("Unknown");
		switch (State)
		{
		case EVilokhvostState::IdleHover: StateText = TEXT("Idle"); break;
		case EVilokhvostState::AttackLunge: StateText = TEXT("Attack"); break;
		case EVilokhvostState::ReturningHome: StateText = TEXT("Return"); break;
		default: break;
		}
		const FString Label = FString::Printf(TEXT("Vilokhvost | %s"), StateText);
		HonorKitchenDevDebug::DrawWorldString(W, GetActorLocation(), Label, FColor::Magenta, 120.f);
	}
}

bool AVilokhvostCharacter::GetSpritePlayerAware() const
{
	return State != EVilokhvostState::IdleHover;
}

bool AVilokhvostCharacter::GetSpriteAttackFrame() const
{
	return State == EVilokhvostState::AttackLunge;
}

bool AVilokhvostCharacter::GetSpriteChaseFrame() const
{
	return false;
}
