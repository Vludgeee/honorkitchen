// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USoundBase;

/** Дефолтные SFX (шаблон FP / Engine), пока не заданы свои в Blueprint. */
class MYPROJECT_API HonorKitchenAudioDefaults
{
public:
	static USoundBase* GetDamageTakenSound();
	static USoundBase* GetUiClickSound();
	static USoundBase* GetAttackHitSound();
	static USoundBase* GetPickupSound();
	static USoundBase* GetChaseStartSound();
	static USoundBase* GetEnemyGruntSound();

	static void AssignIfNull(TObjectPtr<USoundBase>& Slot, USoundBase* Fallback);
};
