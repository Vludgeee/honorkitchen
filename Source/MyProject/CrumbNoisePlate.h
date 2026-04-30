// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrumbNoisePlate.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

/**
 * «Кнопка» под бросок крошки: при физическом ударе крошкой — сильный шум, тянет ИИ с дальности PlateNoiseRange.
 */
UCLASS(Blueprintable)
class MYPROJECT_API ACrumbNoisePlate : public AActor
{
	GENERATED_BODY()

public:
	ACrumbNoisePlate();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NoisePlate")
	TObjectPtr<UBoxComponent> HitBox;

	UPROPERTY(EditDefaultsOnly, Category = "NoisePlate", meta = (ClampMin = "100.0"))
	float PlateNoiseRange = 1400.f;

	UPROPERTY(EditDefaultsOnly, Category = "NoisePlate", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float PlateLoudness = 3.f;

	UFUNCTION()
	void OnBoxHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
