#include "SaltBarrierActor.h"

#include "TomatoSaurusCharacter.h"
#include "HonorKitchenEnemySoundCatalog.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

ASaltBarrierActor::ASaltBarrierActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BarrierTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrierTrigger"));
	BarrierTrigger->SetupAttachment(SceneRoot);
	BarrierTrigger->SetBoxExtent(FVector(140.f, 22.f, 90.f));
	BarrierTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BarrierTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	BarrierTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BarrierTrigger->SetGenerateOverlapEvents(true);

	BarrierVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierVisual"));
	BarrierVisual->SetupAttachment(SceneRoot);
	BarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierVisual->SetCastShadow(false);
	BarrierVisual->SetRelativeLocation(FVector(0.f, 0.f, -44.f));
	BarrierVisual->SetRelativeScale3D(FVector(2.8f, 0.08f, 0.02f));
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		BarrierVisual->SetStaticMesh(Cube);

		// Несколько низких полос с легким смещением выглядят ближе к "рассыпанной соли".
		for (int32 i = 0; i < 2; ++i)
		{
			const FName Name = (i == 0) ? TEXT("BarrierVisualSideA") : TEXT("BarrierVisualSideB");
			UStaticMeshComponent* SideStrip = CreateDefaultSubobject<UStaticMeshComponent>(Name);
			SideStrip->SetupAttachment(SceneRoot);
			SideStrip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SideStrip->SetCastShadow(false);
			SideStrip->SetStaticMesh(Cube);
			const float OffsetY = (i == 0) ? 7.f : -7.f;
			const float OffsetZ = (i == 0) ? -44.8f : -43.2f;
			SideStrip->SetRelativeLocation(FVector(0.f, OffsetY, OffsetZ));
			SideStrip->SetRelativeScale3D(FVector(2.65f, 0.045f, 0.012f));
		}
	}
}

void ASaltBarrierActor::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* BaseShapeMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		TArray<UStaticMeshComponent*> MeshComponents;
		GetComponents<UStaticMeshComponent>(MeshComponents);
		const FLinearColor SaltColor(0.96f, 0.96f, 0.92f, 1.f);
		for (UStaticMeshComponent* MeshComp : MeshComponents)
		{
			if (!MeshComp)
			{
				continue;
			}
			MeshComp->SetMaterial(0, BaseShapeMaterial);
			if (UMaterialInstanceDynamic* Mid = MeshComp->CreateAndSetMaterialInstanceDynamic(0))
			{
				Mid->SetVectorParameterValue(TEXT("Color"), SaltColor);
				Mid->SetVectorParameterValue(TEXT("BaseColor"), SaltColor);
				Mid->SetVectorParameterValue(TEXT("EmissiveColor"), SaltColor * 0.18f);
			}
		}
	}

	BarrierTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASaltBarrierActor::OnBarrierOverlap);
	SetLifeSpan(LifetimeSeconds);
}

void ASaltBarrierActor::OnBarrierOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	ATomatoSaurusCharacter* Tomato = Cast<ATomatoSaurusCharacter>(OtherActor);
	if (!Tomato)
	{
		return;
	}

	// Нормаль барьера = forward actor; томата всегда выталкиваем на "дальнюю" сторону от линии.
	const FVector BarrierForward = GetActorForwardVector().GetSafeNormal2D();
	if (BarrierForward.IsNearlyZero())
	{
		return;
	}
	const FVector TomatoLoc = Tomato->GetActorLocation();
	const FVector ToTomato = TomatoLoc - GetActorLocation();
	const float Side = FVector::DotProduct(FVector(ToTomato.X, ToTomato.Y, 0.f), BarrierForward);
	const FVector PushDir = (Side >= 0.f ? 1.f : -1.f) * BarrierForward;
	const FVector NewLoc = TomatoLoc + PushDir * PushBackDistance;
	Tomato->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
}

