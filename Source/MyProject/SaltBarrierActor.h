#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltBarrierActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Временный соляной барьер: томатозавры не проходят сквозь линию.
 */
UCLASS()
class MYPROJECT_API ASaltBarrierActor : public AActor
{
	GENERATED_BODY()

public:
	ASaltBarrierActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Salt Barrier")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Salt Barrier")
	TObjectPtr<UBoxComponent> BarrierTrigger;

	UPROPERTY(VisibleAnywhere, Category = "Salt Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierVisual;

	/** Время жизни барьера. */
	UPROPERTY(EditDefaultsOnly, Category = "Salt Barrier", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float LifetimeSeconds = 3.0f;

	/** Насколько отталкивать томатозавра назад от плоскости барьера. */
	UPROPERTY(EditDefaultsOnly, Category = "Salt Barrier", meta = (ClampMin = "20.0", ClampMax = "400.0"))
	float PushBackDistance = 120.f;

	UFUNCTION()
	void OnBarrierOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};

