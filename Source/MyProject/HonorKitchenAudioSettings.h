// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Громкость, режим разработчика (сохраняется в GameUserSettings.ini). */
class MYPROJECT_API HonorKitchenAudioSettings
{
public:
	static void Load();
	static void Save();

	static bool IsSoundEnabled();
	static void SetSoundEnabled(bool bEnabled);
	static void ToggleSound();

	static float GetMasterVolume();
	static float ScaleVolume(float Volume);
	static float ScaleMonsterVolume(float Volume);

	static float GetMusicVolume();
	static void SetMusicVolume(float Volume01);
	static float GetMonsterVolume();
	static void SetMonsterVolume(float Volume01);

	static bool IsDeveloperMode();
	static void SetDeveloperMode(bool bEnabled);
	static void ToggleDeveloperMode();

	/** Сбросить отладочные оверлеи у всех локальных игроков (навигатор, телеметрия HUD). */
	static void EnforceDeveloperModeRuntime(UWorld* World);

	/** Пока идёт death-sting: только звук смерти, без музыки/монстров. */
	static bool IsDeathStingActive();
	static void SetDeathStingActive(bool bActive);
};
