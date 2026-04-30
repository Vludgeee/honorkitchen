// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicPostProcess.generated.h"

class UPostProcessComponent;
class USceneComponent;

/**
 * Глобальный пост-процесс через UPostProcessComponent (bUnbound): Bloom, Vignette, Color Grading.
 */
UCLASS(Blueprintable)
class MYPROJECT_API ADynamicPostProcess : public AActor
{
	GENERATED_BODY()

public:
	ADynamicPostProcess();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PostProcess")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PostProcess")
	TObjectPtr<UPostProcessComponent> PostProcessVolume;

	/** Интенсивность Bloom (Lens). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PostProcess|Defaults", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float BloomIntensity = 0.8f;

	/** Сила виньетки (0–1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PostProcess|Defaults", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VignetteIntensity = 0.4f;

	/** Глобальная насыщенность по каналам (холодный тон: чуть ниже G/B). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PostProcess|Defaults")
	FVector4 ColorSaturation = FVector4(1.f, 0.9f, 0.8f, 1.f);

	void ApplyPostProcessSettings();
};
