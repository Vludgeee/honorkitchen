// Copyright Epic Games, Inc. All Rights Reserved.

#include "TomatoSaurusCharacter.h"
#include "TomatoSaurusAIController.h"
#include "MyProjectCharacter.h"
#include "DrawDebugHelpers.h"
#include "MyProjectGameMode.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "TomatoSaurusAIController.h"
#include "HonorKitchenAudioDefaults.h"
#include "HonorKitchenAudioSettings.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "HonorKitchenEnemySpriteComponent.h"
#include "HonorKitchenEnemySpriteCatalog.h"
#include "HonorKitchenEnemyIdleAudioComponent.h"

ATomatoSaurusCharacter::ATomatoSaurusCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CreateDefaultSubobject<UHonorKitchenEnemyIdleAudioComponent>(TEXT("EnemyIdleAudio"));

	AIControllerClass = ATomatoSaurusAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	DamageSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DamageSphere"));
	DamageSphere->SetupAttachment(GetCapsuleComponent());
	DamageSphere->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	DamageSphere->InitSphereRadius(90.f);
	DamageSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageSphere->SetCollisionObjectType(ECC_WorldDynamic);
	DamageSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageSphere->SetGenerateOverlapEvents(true);

	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	ApplyPerceptionSettings();

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	ApplyPerceptionSettings();

	PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());

	TomatoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TomatoMesh"));
	TomatoMesh->SetupAttachment(GetCapsuleComponent());
	TomatoMesh->SetRelativeLocation(FVector(0.f, 0.f, -96.f + 50.f));
	TomatoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	EnemySprite = CreateDefaultSubobject<UHonorKitchenEnemySpriteComponent>(TEXT("EnemySprite"));
	EnemySprite->SetupAttachment(GetCapsuleComponent());
	EnemySprite->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	EnemySprite->SpriteWorldSizeUU = 340.f;
	EnemySprite->bBillboardFaceCamera = true;
	EnemySprite->SpriteFacingYawOffset = 0.f;
	EnemySprite->SpriteFacingRollOffset = 180.f;
	EnemySprite->bFlipSpriteHorizontal = false;
	EnemySprite->bFlipSpriteVertical = false;
	EnemySprite->SetSpriteFrames(HonorKitchenEnemySpriteCatalog::MakeTomatosaurFrames());
	EnemySprite->SetLegacyVisualToHide(TomatoMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		TomatoMesh->SetStaticMesh(SphereMesh.Object);
		TomatoMesh->SetRelativeScale3D(BodyMeshScale);
	}

	GetMesh()->SetVisibility(false, true);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = IdleMoveSpeed;
		Move->RotationRate = FRotator(0.f, 540.f, 0.f);
		Move->bOrientRotationToMovement = true;
		bUseControllerRotationYaw = false;
	}
}

void ATomatoSaurusCharacter::ApplyPerceptionSettings()
{
	if (SightConfig)
	{
		SightConfig->SightRadius = FMath::Max(100.f, SightRadiusUU);
		SightConfig->LoseSightRadius = FMath::Max(SightConfig->SightRadius + 1.f, LoseSightRadiusUU);
		SightConfig->PeripheralVisionAngleDegrees = FMath::Clamp(SightHalfAngleDegrees, 1.f, 179.f);
		SightConfig->PointOfViewBackwardOffset = 0.f;
		// 0: иначе цель «исчезает» вблизи и AI сбрасывает преследование (типичный порог ~50uu).
		SightConfig->NearClippingRadius = 0.f;
		SightConfig->AutoSuccessRangeFromLastSeenLocation = FMath::Max(0.f, AutoSuccessRangeFromLastSeenUU);
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		if (PerceptionComponent)
		{
			PerceptionComponent->ConfigureSense(*SightConfig);
		}
	}

	if (HearingConfig)
	{
		HearingConfig->HearingRange = FMath::Max(100.f, HearingRangeUU);
		HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
		HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
		HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
		if (PerceptionComponent)
		{
			PerceptionComponent->ConfigureSense(*HearingConfig);
		}
	}
}

void ATomatoSaurusCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!TomatoMesh)
	{
		return;
	}

	if (BodyMesh)
	{
		TomatoMesh->SetStaticMesh(BodyMesh);
	}
	else
	{
		// FObjectFinder только в конструкторе; в OnConstruction — LoadObject.
		if (UStaticMesh* DefaultSphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		{
			TomatoMesh->SetStaticMesh(DefaultSphere);
		}
	}
	TomatoMesh->SetRelativeScale3D(BodyMeshScale);

	const int32 NumSlots = TomatoMesh->GetNumMaterials();
	for (int32 i = 0; i < BodyMaterialOverrides.Num() && i < NumSlots; ++i)
	{
		if (BodyMaterialOverrides[i])
		{
			TomatoMesh->SetMaterial(i, BodyMaterialOverrides[i]);
		}
	}
}

void ATomatoSaurusCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshAIMovementSpeed();

	// Страховка от "потерял в упор": если игрок рядом и не строго сзади — продолжаем преследование.
	AMyProjectCharacter* Player = Cast<AMyProjectCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Player && CanSeePlayerByFallback(Player))
	{
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		if (!bHasSightOnPlayer || (Now - LastFallbackSightNotifyTime) >= FallbackSightRefreshCooldown)
		{
			LastFallbackSightNotifyTime = Now;
			OnPlayerSightGained(Player);
		}
	}

#if !UE_BUILD_SHIPPING && 0
	if (UWorld* W = GetWorld())
	{
		const FVector Loc = GetActorLocation();
		if (SightConfig)
		{
			DrawDebugSphere(W, Loc, SightConfig->SightRadius, 28, FColor::Yellow, false, 0.f, 0, 1.5f);
			DrawDebugSphere(W, Loc, SightConfig->LoseSightRadius, 28, FColor::Orange, false, 0.f, 0, 1.f);
		}
		if (HearingConfig)
		{
			DrawDebugSphere(W, Loc, HearingConfig->HearingRange, 28, FColor::Blue, false, 0.f, 0, 1.f);
		}
	}
#endif
}

bool ATomatoSaurusCharacter::CanSeePlayerByFallback(const AMyProjectCharacter* Player) const
{
	if (!Player || Player->IsDead())
	{
		return false;
	}

	const FVector SelfLoc = GetActorLocation();
	const FVector PlayerLoc = Player->GetActorLocation();
	const FVector ToPlayer = PlayerLoc - SelfLoc;
	const float Dist = ToPlayer.Size2D();

	// В ближнем контакте цель всегда должна детектиться.
	if (Dist <= CloseRangeAutoDetectUU)
	{
		return true;
	}
	if (Dist > SightRadiusUU + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TomatoFallbackSight), true, this);
	QueryParams.AddIgnoredActor(this);

	// Двойная проверка LOS (корпус/голова), чтобы уменьшить ложную "слепоту" у мебели.
	FHitResult MidHit;
	const FVector TraceStartMid = SelfLoc + FVector(0.f, 0.f, 55.f);
	const FVector TraceEndMid = PlayerLoc + FVector(0.f, 0.f, 55.f);
	const bool bMidBlocked = World->LineTraceSingleByChannel(MidHit, TraceStartMid, TraceEndMid, ECC_Visibility, QueryParams);
	const bool bMidVisible = !bMidBlocked || MidHit.GetActor() == Player;
	if (bMidVisible)
	{
		return true;
	}

	FHitResult HeadHit;
	const FVector TraceStartHead = SelfLoc + FVector(0.f, 0.f, 80.f);
	const FVector TraceEndHead = PlayerLoc + FVector(0.f, 0.f, 95.f);
	const bool bHeadBlocked = World->LineTraceSingleByChannel(HeadHit, TraceStartHead, TraceEndHead, ECC_Visibility, QueryParams);
	return !bHeadBlocked || HeadHit.GetActor() == Player;
}

void ATomatoSaurusCharacter::RefreshAIMovementSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	float Base = IdleMoveSpeed;
	if (const ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController()))
	{
		switch (AI->GetTomatoAIState())
		{
		case ATomatoSaurusAIController::ETomatoAIState::InvestigateNoise:
			Base = InvestigateMoveSpeed;
			break;
		case ATomatoSaurusAIController::ETomatoAIState::ChaseTarget:
			Base = bAttackRecharging ? IdleMoveSpeed * 0.15f : ChaseMoveSpeed;
			break;
		default:
			Base = IdleMoveSpeed;
			break;
		}
	}

	Move->MaxWalkSpeed = Base * CrowdControlSpeedMultiplier;
}

void ATomatoSaurusCharacter::ApplySaltSlowDebuff(float DurationSeconds, float SpeedMultiplier)
{
	CrowdControlSpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.05f, 1.f);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(SaltSlowTimerHandle);
		W->GetTimerManager().SetTimer(SaltSlowTimerHandle, this, &ATomatoSaurusCharacter::ClearSaltSlowDebuff, DurationSeconds, false);
	}
	RefreshAIMovementSpeed();
}

void ATomatoSaurusCharacter::ClearSaltSlowDebuff()
{
	CrowdControlSpeedMultiplier = 1.f;
	RefreshAIMovementSpeed();
}

void ATomatoSaurusCharacter::ApplyWaterBlindDebuff(float DurationSeconds)
{
	if (!PerceptionComponent)
	{
		return;
	}
	PerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(WaterBlindTimerHandle);
		W->GetTimerManager().SetTimer(WaterBlindTimerHandle, this, &ATomatoSaurusCharacter::ClearWaterBlindDebuff, DurationSeconds, false);
	}
}

void ATomatoSaurusCharacter::ClearWaterBlindDebuff()
{
	if (PerceptionComponent)
	{
		PerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	}
}

void ATomatoSaurusCharacter::SyncChaseSpeedToPlayer()
{
	if (ChaseSpeedVsPlayerRatio <= 0.f)
	{
		return;
	}
	if (const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (const UCharacterMovementComponent* PlayerMove = Player->GetCharacterMovement())
		{
			ChaseMoveSpeed = FMath::Max(IdleMoveSpeed + 10.f, PlayerMove->MaxWalkSpeed * ChaseSpeedVsPlayerRatio);
		}
	}
}

void ATomatoSaurusCharacter::BeginPlay()
{
	Super::BeginPlay();
	HonorKitchenAudioDefaults::AssignIfNull(AttackHitSound, HonorKitchenAudioDefaults::GetAttackHitSound());
	SyncChaseSpeedToPlayer();
	ApplyPerceptionSettings();

	// Если враг заспавнился мимо NavMesh (например на столе), сдвигаем на ближайшую нав-точку.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(this))
	{
		FNavLocation NavLocation;
		const FVector Start = GetActorLocation();
		if (NavSys->ProjectPointToNavigation(Start, NavLocation, FVector(500.f, 500.f, 400.f)))
		{
			const float DistToNav = FVector::Dist2D(Start, NavLocation.Location);
			if (DistToNav > 5.f)
			{
				SetActorLocation(NavLocation.Location, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}

	SetActorHiddenInGame(false);
	const bool bUsingSpriteVisual = SetupEnemySpriteVisual();
	if (TomatoMesh && !bUsingSpriteVisual)
	{
		if (!TomatoMesh->GetStaticMesh())
		{
			if (UStaticMesh* DefaultSphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
			{
				TomatoMesh->SetStaticMesh(DefaultSphere);
			}
		}
		TomatoMesh->SetVisibility(true, true);
		TomatoMesh->SetHiddenInGame(false, true);
		TomatoMesh->SetRelativeScale3D(BodyMeshScale);
		TomatoMesh->MarkRenderStateDirty();

		if (UMaterialInterface* BaseShapeMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			TomatoMesh->SetMaterial(0, BaseShapeMaterial);
			if (UMaterialInstanceDynamic* Mid = TomatoMesh->CreateAndSetMaterialInstanceDynamic(0))
			{
				const FLinearColor TomatoColor(0.78f, 0.14f, 0.10f, 1.f);
				Mid->SetVectorParameterValue(TEXT("Color"), TomatoColor);
				Mid->SetVectorParameterValue(TEXT("BaseColor"), TomatoColor);
				Mid->SetVectorParameterValue(TEXT("EmissiveColor"), TomatoColor * 0.12f);
			}
		}
	}

	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ATomatoSaurusCharacter::OnTargetPerceptionUpdated);

	if (DamageSphere)
	{
		DamageSphere->OnComponentBeginOverlap.AddDynamic(this, &ATomatoSaurusCharacter::OnDamageOverlapBegin);
		DamageSphere->OnComponentEndOverlap.AddDynamic(this, &ATomatoSaurusCharacter::OnDamageOverlapEnd);
	}

}

void ATomatoSaurusCharacter::SetAttackRecharging(bool bRecharging)
{
	bAttackRecharging = bRecharging;
	if (DamageSphere)
	{
		DamageSphere->SetCollisionEnabled(bRecharging ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	}
	RefreshAIMovementSpeed();
}

void ATomatoSaurusCharacter::OnDamageOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bAttackRecharging)
	{
		return;
	}
	AMyProjectCharacter* P = Cast<AMyProjectCharacter>(OtherActor);
	if (!P || !GetWorld())
	{
		return;
	}

	DamageTarget = P;
	AController* Inst = GetController();
	UGameplayStatics::ApplyDamage(P, DamagePerTick, Inst, this, UDamageType::StaticClass());
	const EHonorKitchenEnemySpecies Species = IsA(AKaravaychikCharacter::StaticClass())
		? EHonorKitchenEnemySpecies::Karavaychik
		: EHonorKitchenEnemySpecies::TomatoSaurus;
	HonorKitchenEnemySoundCatalog::PlayAt(this, GetActorLocation(), Species, EHonorKitchenEnemySoundEvent::Punch, 0.9f);
	if (ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController()))
	{
		AI->BeginPostAttackRecharge();
	}
	GetWorldTimerManager().ClearTimer(DamageTickTimer);
}

void ATomatoSaurusCharacter::OnDamageOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (DamageTarget.Get() != Cast<AMyProjectCharacter>(OtherActor))
	{
		return;
	}
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(DamageTickTimer);
	}
	DamageTarget = nullptr;
}

void ATomatoSaurusCharacter::ApplyDamageTick()
{
	if (bAttackRecharging)
	{
		return;
	}
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	if (!DamageTarget.IsValid())
	{
		W->GetTimerManager().ClearTimer(DamageTickTimer);
		return;
	}

	AController* Inst = GetController();
	UGameplayStatics::ApplyDamage(DamageTarget.Get(), DamagePerTick, Inst, this, UDamageType::StaticClass());
}

void ATomatoSaurusCharacter::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController());
	if (!AI)
	{
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID(UAISense_Hearing::StaticClass()))
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			AI->NotifyHeardNoise(Stimulus.StimulusLocation, NoisePursuitSeconds);
		}
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID(UAISense_Sight::StaticClass()))
	{
		AMyProjectCharacter* PC = Cast<AMyProjectCharacter>(Actor);
		if (!PC)
		{
			return;
		}
		if (Stimulus.WasSuccessfullySensed())
		{
			OnPlayerSightGained(PC);
		}
		else
		{
			OnPlayerSightLost(Stimulus.StimulusLocation);
		}
	}
}

void ATomatoSaurusCharacter::OnPlayerSightGained(AMyProjectCharacter* Player)
{
	ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController());
	if (!AI || !Player)
	{
		return;
	}

	if (!bHasSightOnPlayer)
	{
		if (AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			GM->NotifyAIDetectedPlayer();
		}
	}
	bHasSightOnPlayer = true;
	AI->SetChaseTarget(Player);
}

void ATomatoSaurusCharacter::OnPlayerSightLost(FVector LastSeenLocation)
{
	ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController());
	if (!AI)
	{
		return;
	}
	bHasSightOnPlayer = false;
	LastFallbackSightNotifyTime = -1000.f;
	AI->NotifySightLost(LastSeenLocation);
}

bool ATomatoSaurusCharacter::SetupEnemySpriteVisual()
{
	if (!EnemySprite)
	{
		return false;
	}

	EnemySprite->RefreshSpriteVisual();
	return EnemySprite->IsSpriteActive();
}

bool ATomatoSaurusCharacter::GetSpritePlayerAware() const
{
	if (bHasSightOnPlayer)
	{
		return true;
	}
	if (const ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController()))
	{
		const ATomatoSaurusAIController::ETomatoAIState State = AI->GetTomatoAIState();
		return State == ATomatoSaurusAIController::ETomatoAIState::ChaseTarget
			|| State == ATomatoSaurusAIController::ETomatoAIState::InvestigateNoise;
	}
	return false;
}

bool ATomatoSaurusCharacter::GetSpriteAttackFrame() const
{
	// Кадр удара — только короткая фаза перезарядки; в Chase используется Chase→attack в каталоге.
	if (bAttackRecharging)
	{
		return true;
	}
	if (const ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController()))
	{
		return AI->IsAttackRecharging();
	}
	return false;
}

bool ATomatoSaurusCharacter::GetSpriteChaseFrame() const
{
	if (bAttackRecharging)
	{
		return false;
	}
	if (const ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(GetController()))
	{
		return AI->GetTomatoAIState() == ATomatoSaurusAIController::ETomatoAIState::ChaseTarget;
	}
	return false;
}
