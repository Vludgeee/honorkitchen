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

	/** Краткий всплеск виньетки (урон игроку). */
	UFUNCTION(BlueprintCallable, Category = "PostProcess")
	void PlayDamagePulse(float Strength01 = 0.85f);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

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

	/** Доп. виньетка от урона (накладывается поверх базовой). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PostProcess|Damage", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float DamagePulseVignetteBoost = 1.15f;

	/** Секунд до затухания DamagePulse до нуля. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PostProcess|Damage", meta = (ClampMin = "0.05", ClampMax = "3.0"))
	float DamagePulseDecaySeconds = 0.35f;

	/** Глобальная насыщенность по каналам (холодный тон: чуть ниже G/B). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PostProcess|Defaults")
	FVector4 ColorSaturation = FVector4(1.f, 0.9f, 0.8f, 1.f);

public:
	/** Доп. напряжение 0..1 (угроза рядом / низкое HP): накладывается поверх базовых настроек. */
	void SetThreatStress(float Stress01);

protected:
	float CurrentThreatStress = 0.f;
	float CurrentDamagePulse = 0.f;

	void PushSettingsToVolume();
};
