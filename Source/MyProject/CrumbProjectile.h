// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrumbProjectile.generated.h"

class UPrimitiveComponent;
class UProjectileMovementComponent;
class USphereComponent;
struct FHitResult;

/**
 * Брошенная крошка: при ударе об уровень даёт шум слуху ИИ (≈ радиус NoiseMaxRange).
 */
UCLASS(Blueprintable)
class MYPROJECT_API ACrumbProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACrumbProjectile();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crumb")
	TObjectPtr<USphereComponent> SphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crumb")
	TObjectPtr<UProjectileMovementComponent> ProjMove;

	/** Радиус шума для AI (uu), по ТЗ крошка ~5 м → 500. */
	UPROPERTY(EditDefaultsOnly, Category = "Crumb", meta = (ClampMin = "100.0"))
	float NoiseMaxRange = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crumb", meta = (ClampMin = "100.0"))
	float InitialSpeed = 880.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crumb", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float NoiseLoudness = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Crumb", meta = (ClampMin = "1.0", ClampMax = "15.0"))
	float DistractSeconds = 4.5f;

	bool bNoiseReported = false;

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ReportNoiseAt(const FVector& WorldLocation);
};
