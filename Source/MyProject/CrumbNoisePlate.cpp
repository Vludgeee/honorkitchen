// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrumbNoisePlate.h"
#include "CrumbProjectile.h"
#include "Components/BoxComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Engine/EngineTypes.h"

ACrumbNoisePlate::ACrumbNoisePlate()
{
	PrimaryActorTick.bCanEverTick = false;

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	RootComponent = HitBox;
	HitBox->InitBoxExtent(FVector(40.f, 40.f, 8.f));
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HitBox->SetCollisionObjectType(ECC_WorldStatic);
	HitBox->SetCollisionResponseToAllChannels(ECR_Block);
	HitBox->SetNotifyRigidBodyCollision(true);
}

void ACrumbNoisePlate::BeginPlay()
{
	Super::BeginPlay();
	HitBox->OnComponentHit.AddDynamic(this, &ACrumbNoisePlate::OnBoxHit);
}

void ACrumbNoisePlate::OnBoxHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!Cast<ACrumbProjectile>(OtherActor))
	{
		return;
	}

	AActor* Inst = OtherActor ? OtherActor->GetInstigator() : nullptr;
	UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation(), PlateLoudness, Inst, PlateNoiseRange, FName("CrumbButton"));
}
