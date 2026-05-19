// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrumbProjectile.h"
#include "TomatoSaurusAIController.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "HonorKitchenDevDebug.h"
#include "Components/StaticMeshComponent.h"

ACrumbProjectile::ACrumbProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	RootComponent = SphereCollision;
	SphereCollision->InitSphereRadius(7.f);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SphereCollision->SetCollisionObjectType(ECC_PhysicsBody);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SphereCollision->SetNotifyRigidBodyCollision(true);
	SphereCollision->BodyInstance.SetMassOverride(2.5f, true);

	ProjMove = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjMove"));
	ProjMove->UpdatedComponent = SphereCollision;
	ProjMove->InitialSpeed = 0.f;
	ProjMove->MaxSpeed = 5000.f;
	ProjMove->bRotationFollowsVelocity = true;
	ProjMove->bShouldBounce = true;
	ProjMove->Bounciness = 0.4f;
	ProjMove->Friction = 0.2f;
	ProjMove->ProjectileGravityScale = 1.15f;

	UStaticMeshComponent* Vis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrumbVis"));
	Vis->SetupAttachment(SphereCollision);
	Vis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Vis->SetStaticMesh(SphereMesh.Object);
		Vis->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.12f));
	}
}

void ACrumbProjectile::BeginPlay()
{
	Super::BeginPlay();
	SphereCollision->OnComponentHit.AddDynamic(this, &ACrumbProjectile::OnSphereHit);

	const FVector Dir = GetActorForwardVector();
	ProjMove->Velocity = Dir * InitialSpeed;
}

void ACrumbProjectile::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	ReportNoiseAt(Hit.ImpactPoint);
	ProjMove->StopMovementImmediately();
	SetLifeSpan(0.15f);
}

void ACrumbProjectile::ReportNoiseAt(const FVector& WorldLocation)
{
	if (bNoiseReported)
	{
		return;
	}
	bNoiseReported = true;

	AActor* Inst = GetInstigator();
	UAISense_Hearing::ReportNoiseEvent(this, WorldLocation, NoiseLoudness, Inst, NoiseMaxRange, FName("Crumb"));

	// Гарантированное отвлечение: дублируем событие напрямую в AI-контроллеры в радиусе.
	TArray<AActor*> AIActors;
	UGameplayStatics::GetAllActorsOfClass(this, ATomatoSaurusAIController::StaticClass(), AIActors);
	const float MaxDistSq = FMath::Square(NoiseMaxRange);
	int32 NotifiedCount = 0;
	for (AActor* A : AIActors)
	{
		ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(A);
		if (!AI || !AI->GetPawn())
		{
			continue;
		}
		if (FVector::DistSquared(AI->GetPawn()->GetActorLocation(), WorldLocation) > MaxDistSq)
		{
			continue;
		}
		AI->NotifyHeardNoise(WorldLocation, DistractSeconds);
		++NotifiedCount;
	}

	HonorKitchenDevDebug::OnScreen(
		1.4f,
		FColor::Yellow,
		FString::Printf(TEXT("Crumb impact: AI notified=%d"), NotifiedCount));
}
