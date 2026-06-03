// Copyright Epic Games, Inc. All Rights Reserved.



#include "HonorKitchenMonsterAudio.h"

#include "HonorKitchenAudioSettings.h"

#include "Components/AudioComponent.h"

#include "Engine/Attenuation.h"

#include "Engine/World.h"

#include "Kismet/GameplayStatics.h"

#include "TimerManager.h"

#include "UObject/UObjectGlobals.h"



void HonorKitchenMonsterAudio::ConfigureSpatialAudio(UAudioComponent* AudioComponent)

{

	if (!AudioComponent)

	{

		return;

	}



	AudioComponent->bAllowSpatialization = true;

	AudioComponent->bIsUISound = false;

	AudioComponent->bOverrideAttenuation = true;

	AudioComponent->AttenuationSettings = nullptr;



	FSoundAttenuationSettings& S = AudioComponent->AttenuationOverrides;

	S.bAttenuate = true;

	S.bSpatialize = true;

	S.AttenuationShape = EAttenuationShape::Sphere;

	S.FalloffDistance = AttenuationFalloffDistanceUU;

	S.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;

	S.AttenuationShapeExtents = FVector(80.f, 0.f, 0.f);

	S.bEnableListenerFocus = false;

	S.bEnableOcclusion = false;

}



UAudioComponent* HonorKitchenMonsterAudio::PlayOneShotAt(

	const UObject* WorldContext,

	USoundBase* Sound,

	const FVector& Location,

	float VolumeMultiplier,

	float PitchMultiplier)

{

	if (!IsValid(WorldContext) || !IsValid(Sound) || !HonorKitchenAudioSettings::IsSoundEnabled()

		|| HonorKitchenAudioSettings::IsDeathStingActive())

	{

		return nullptr;

	}



	UWorld* World = WorldContext->GetWorld();

	if (!World || !World->IsGameWorld())

	{

		return nullptr;

	}



	const float ScaledVolume = HonorKitchenAudioSettings::ScaleMonsterVolume(VolumeMultiplier);



	UAudioComponent* const AC = UGameplayStatics::SpawnSoundAtLocation(

		WorldContext,

		Sound,

		Location,

		FRotator::ZeroRotator,

		ScaledVolume,

		PitchMultiplier,

		0.f,

		nullptr,

		nullptr,

		true);



	if (!AC)

	{

		return nullptr;

	}



	ConfigureSpatialAudio(AC);

	AC->SetVolumeMultiplier(ScaledVolume);



	const float Duration = FMath::Max(Sound->GetDuration(), FadeOutSeconds + 0.05f);

	const float FadeOutStart = FMath::Max(0.f, Duration - FadeOutSeconds);

	if (FadeOutSeconds > KINDA_SMALL_NUMBER && FadeOutStart > 0.f)

	{

		TWeakObjectPtr<UAudioComponent> WeakAC(AC);

		FTimerHandle FadeOutTimer;

		World->GetTimerManager().SetTimer(

			FadeOutTimer,

			[WeakAC]()

			{

				if (UAudioComponent* Comp = WeakAC.Get())

				{

					if (Comp->IsPlaying())

					{

						Comp->FadeOut(FadeOutSeconds, 0.f);

					}

				}

			},

			FadeOutStart,

			false);

	}



	return AC;

}

