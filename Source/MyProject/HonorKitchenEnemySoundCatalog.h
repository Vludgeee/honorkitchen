// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class USoundBase;

enum class EHonorKitchenEnemySpecies : uint8
{
	TomatoSaurus,
	Karavaychik,
	Vilokhvost
};

enum class EHonorKitchenEnemySoundEvent : uint8
{
	Idle,
	Detected,
	Chase,
	Punch,
	Damage
};

/** Загрузка и случайный выбор вариантов SFX врагов из Content/Audio/Enemies. */
class MYPROJECT_API HonorKitchenEnemySoundCatalog
{
public:
	static USoundBase* PickSound(EHonorKitchenEnemySpecies Species, EHonorKitchenEnemySoundEvent Event);

	static void PlayAt(
		AActor* WorldContext,
		const FVector& Location,
		EHonorKitchenEnemySpecies Species,
		EHonorKitchenEnemySoundEvent Event,
		float VolumeMultiplier = 1.f,
		float PitchMultiplier = 1.f);

	static USoundBase* PickTomatoIdleLoop();
	static USoundBase* PickKaravayIdleLoop();
	static USoundBase* PickVilokhvostIdleLoop();
};
