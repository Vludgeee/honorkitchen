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
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

ATomatoSaurusCharacter::ATomatoSaurusCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
	// Баланс: зрение / потеря цели (см)
	SightConfig->SightRadius = 1600.f;
	SightConfig->LoseSightRadius = 2000.f;
	SightConfig->PeripheralVisionAngleDegrees = 72.f;
	SightConfig->PointOfViewBackwardOffset = 0.f;
	// 0: иначе цель «исчезает» вблизи и AI сбрасывает преследование (типичный порог ~50uu).
	SightConfig->NearClippingRadius = 0.f;
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	PerceptionComponent->ConfigureSense(*SightConfig);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 1700.f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	PerceptionComponent->ConfigureSense(*HearingConfig);

	PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());

	TomatoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TomatoMesh"));
	TomatoMesh->SetupAttachment(GetCapsuleComponent());
	TomatoMesh->SetRelativeLocation(FVector(0.f, 0.f, -96.f + 50.f));
	TomatoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
		case ATomatoSaurusAIController::ETomatoAIState::MeleeApproach:
			Base = ChaseMoveSpeed;
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

void ATomatoSaurusCharacter::BeginPlay()
{
	Super::BeginPlay();
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ATomatoSaurusCharacter::OnTargetPerceptionUpdated);

	if (DamageSphere)
	{
		DamageSphere->OnComponentBeginOverlap.AddDynamic(this, &ATomatoSaurusCharacter::OnDamageOverlapBegin);
		DamageSphere->OnComponentEndOverlap.AddDynamic(this, &ATomatoSaurusCharacter::OnDamageOverlapEnd);
	}
}

void ATomatoSaurusCharacter::OnDamageOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AMyProjectCharacter* P = Cast<AMyProjectCharacter>(OtherActor);
	if (!P || !GetWorld())
	{
		return;
	}

	DamageTarget = P;
	AController* Inst = GetController();
	UGameplayStatics::ApplyDamage(P, DamagePerTick, Inst, this, UDamageType::StaticClass());

	GetWorldTimerManager().ClearTimer(DamageTickTimer);
	GetWorldTimerManager().SetTimer(DamageTickTimer, this, &ATomatoSaurusCharacter::ApplyDamageTick, DamageTickInterval, true, DamageTickInterval);
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
	AI->NotifySightLost(LastSeenLocation);
}
