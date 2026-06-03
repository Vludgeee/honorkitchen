// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"



class UAudioComponent;

class UObject;

class USoundBase;



/** Пространственное воспроизведение SFX монстров: затухание по дистанции + короткий fade out. */

class MYPROJECT_API HonorKitchenMonsterAudio

{

public:

	static constexpr float FadeInSeconds = 0.12f;

	static constexpr float FadeOutSeconds = 0.12f;



	/** Дистанция (uu), на которой звук почти неслышен. */

	static constexpr float AttenuationFalloffDistanceUU = 3200.f;



	static void ConfigureSpatialAudio(UAudioComponent* AudioComponent);



	/** One-shot с fade out; возвращает компонент (может быть nullptr). */

	static UAudioComponent* PlayOneShotAt(

		const UObject* WorldContext,

		USoundBase* Sound,

		const FVector& Location,

		float VolumeMultiplier = 1.f,

		float PitchMultiplier = 1.f);

};

