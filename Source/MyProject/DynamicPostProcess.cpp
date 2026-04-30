// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicPostProcess.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Scene.h"

ADynamicPostProcess::ADynamicPostProcess()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PostProcessVolume = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessVolume"));
	PostProcessVolume->SetupAttachment(SceneRoot);
}

void ADynamicPostProcess::BeginPlay()
{
	Super::BeginPlay();
	ApplyPostProcessSettings();
}

void ADynamicPostProcess::ApplyPostProcessSettings()
{
	if (!PostProcessVolume)
	{
		return;
	}

	PostProcessVolume->bEnabled = true;
	PostProcessVolume->bUnbound = true;
	PostProcessVolume->BlendWeight = 1.f;
	PostProcessVolume->BlendRadius = 0.f;
	PostProcessVolume->Priority = 10.f;

	FPostProcessSettings& S = PostProcessVolume->Settings;

	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = BloomIntensity;

	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = VignetteIntensity;

	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = ColorSaturation;
}
