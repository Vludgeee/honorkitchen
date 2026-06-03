// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicPostProcess.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Scene.h"

ADynamicPostProcess::ADynamicPostProcess()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PostProcessVolume = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessVolume"));
	PostProcessVolume->SetupAttachment(SceneRoot);
}

void ADynamicPostProcess::BeginPlay()
{
	Super::BeginPlay();
	CurrentThreatStress = 0.f;
	PushSettingsToVolume();
}

void ADynamicPostProcess::PlayDamagePulse(float Strength01)
{
	CurrentDamagePulse = FMath::Max(CurrentDamagePulse, FMath::Clamp(Strength01, 0.f, 1.5f));
	PushSettingsToVolume();
}

void ADynamicPostProcess::HoldDamagePulseAtPeak(float Strength01)
{
	bHoldDamagePulse = true;
	CurrentDamagePulse = FMath::Max(CurrentDamagePulse, FMath::Clamp(Strength01, 0.f, 1.5f));
	PushSettingsToVolume();
}

void ADynamicPostProcess::ReleaseDamagePulseHold()
{
	bHoldDamagePulse = false;
	PushSettingsToVolume();
}

void ADynamicPostProcess::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	bool bNeedsPush = false;
	if (!bHoldDamagePulse && CurrentDamagePulse > KINDA_SMALL_NUMBER)
	{
		const float Prev = CurrentDamagePulse;
		CurrentDamagePulse = FMath::Max(0.f, CurrentDamagePulse - (DeltaSeconds / FMath::Max(0.05f, DamagePulseDecaySeconds)));
		bNeedsPush = !FMath::IsNearlyEqual(Prev, CurrentDamagePulse, 0.001f);
	}

	if (bNeedsPush)
	{
		PushSettingsToVolume();
	}
}

void ADynamicPostProcess::SetThreatStress(float Stress01)
{
	Stress01 = FMath::Clamp(Stress01, 0.f, 1.f);
	if (FMath::IsNearlyEqual(CurrentThreatStress, Stress01, KINDA_SMALL_NUMBER))
	{
		return;
	}
	CurrentThreatStress = Stress01;
	PushSettingsToVolume();
}

void ADynamicPostProcess::PushSettingsToVolume()
{
	if (!PostProcessVolume)
	{
		return;
	}

	PostProcessVolume->bEnabled = true;
	PostProcessVolume->bUnbound = true;
	PostProcessVolume->BlendWeight = 1.f;
	PostProcessVolume->BlendRadius = 0.f;
	PostProcessVolume->Priority = 9999.f;

	FPostProcessSettings& S = PostProcessVolume->Settings;

	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = BloomIntensity;

	const float ThreatVignette = CurrentThreatStress * 0.55f;
	const float DamageVignette = CurrentDamagePulse * DamagePulseVignetteBoost;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = FMath::Clamp(VignetteIntensity + ThreatVignette + DamageVignette, 0.f, 2.25f);

	S.bOverride_ColorSaturation = true;
	const float Desat = CurrentThreatStress * 0.12f + CurrentDamagePulse * 0.35f;
	S.ColorSaturation = FVector4(
		ColorSaturation.X * FMath::Max(0.2f, 1.f - Desat * 0.85f),
		ColorSaturation.Y * FMath::Max(0.2f, 1.f - Desat),
		ColorSaturation.Z * FMath::Max(0.2f, 1.f - Desat * 1.08f),
		ColorSaturation.W);

	if (CurrentDamagePulse > 0.02f)
	{
		const float D = FMath::Clamp(CurrentDamagePulse, 0.f, 1.f);
		S.bOverride_SceneColorTint = true;
		S.SceneColorTint = FLinearColor(
			FMath::Lerp(1.f, 1.35f, D),
			FMath::Lerp(1.f, 0.42f, D),
			FMath::Lerp(1.f, 0.38f, D),
			1.f);
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(
			FMath::Lerp(1.f, 1.18f, D),
			FMath::Lerp(1.f, 0.92f, D),
			FMath::Lerp(1.f, 0.9f, D),
			1.f);
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = BloomIntensity + D * 0.65f;
	}
	else
	{
		S.bOverride_SceneColorTint = false;
		S.SceneColorTint = FLinearColor::White;
		S.bOverride_ColorContrast = false;
		S.ColorContrast = FVector4(1.f, 1.f, 1.f, 1.f);
	}

	if (CurrentThreatStress > 0.002f)
	{
		S.bOverride_SceneFringeIntensity = true;
		S.SceneFringeIntensity = CurrentThreatStress * 1.05f;
	}
	else
	{
		S.bOverride_SceneFringeIntensity = false;
		S.SceneFringeIntensity = 0.f;
	}
}
