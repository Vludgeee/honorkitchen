// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenDevDebug.h"
#include "MyProjectCharacter.h"
#include "MyProjectHUD.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace HonorKitchenAudioSettingsPrivate
{
	static const TCHAR* SettingsSection = TEXT("/Script/MyProject.HonorKitchenAudio");
	static bool bSoundEnabled = true;
	static bool bDeveloperMode = false;
	static float MusicVolume = 1.f;
	static float MonsterVolume = 1.f;
	static bool bLoaded = false;
	static bool bDeathStingActive = false;

	static void EnsureLoaded()
	{
		if (!bLoaded)
		{
			HonorKitchenAudioSettings::Load();
		}
	}

	static float Clamp01(float V)
	{
		return FMath::Clamp(V, 0.f, 1.f);
	}
}

void HonorKitchenAudioSettings::Load()
{
	using namespace HonorKitchenAudioSettingsPrivate;
	bLoaded = true;
	if (!GConfig->GetBool(SettingsSection, TEXT("bSoundEnabled"), bSoundEnabled, GGameUserSettingsIni))
	{
		bSoundEnabled = true;
	}
	if (!GConfig->GetBool(SettingsSection, TEXT("bDeveloperMode"), bDeveloperMode, GGameUserSettingsIni))
	{
		bDeveloperMode = false;
	}
	if (!GConfig->GetFloat(SettingsSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni))
	{
		MusicVolume = 1.f;
	}
	if (!GConfig->GetFloat(SettingsSection, TEXT("MonsterVolume"), MonsterVolume, GGameUserSettingsIni))
	{
		MonsterVolume = 1.f;
	}
	MusicVolume = Clamp01(MusicVolume);
	MonsterVolume = Clamp01(MonsterVolume);
}

void HonorKitchenAudioSettings::Save()
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	GConfig->SetBool(SettingsSection, TEXT("bSoundEnabled"), bSoundEnabled, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("bDeveloperMode"), bDeveloperMode, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("MonsterVolume"), MonsterVolume, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

bool HonorKitchenAudioSettings::IsSoundEnabled()
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	return bSoundEnabled;
}

void HonorKitchenAudioSettings::SetSoundEnabled(bool bEnabled)
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	bSoundEnabled = bEnabled;
	Save();
}

void HonorKitchenAudioSettings::ToggleSound()
{
	SetSoundEnabled(!IsSoundEnabled());
}

float HonorKitchenAudioSettings::GetMasterVolume()
{
	return IsSoundEnabled() ? 1.f : 0.f;
}

float HonorKitchenAudioSettings::ScaleVolume(float Volume)
{
	return Volume * GetMasterVolume();
}

float HonorKitchenAudioSettings::GetMusicVolume()
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	return MusicVolume;
}

void HonorKitchenAudioSettings::SetMusicVolume(float Volume01)
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	MusicVolume = Clamp01(Volume01);
	Save();
}

float HonorKitchenAudioSettings::GetMonsterVolume()
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	return MonsterVolume;
}

void HonorKitchenAudioSettings::SetMonsterVolume(float Volume01)
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	MonsterVolume = Clamp01(Volume01);
	Save();
}

float HonorKitchenAudioSettings::ScaleMonsterVolume(float Volume)
{
	return ScaleVolume(Volume) * GetMonsterVolume();
}

bool HonorKitchenAudioSettings::IsDeveloperMode()
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	return bDeveloperMode;
}

void HonorKitchenAudioSettings::SetDeveloperMode(bool bEnabled)
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	bDeveloperMode = bEnabled;
	Save();
}

void HonorKitchenAudioSettings::ToggleDeveloperMode()
{
	SetDeveloperMode(!IsDeveloperMode());
}

void HonorKitchenAudioSettings::EnforceDeveloperModeRuntime(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (!IsDeveloperMode())
	{
		HonorKitchenDevDebug::ClearOnScreenMessages();
	}
	else
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController())
		{
			continue;
		}

		if (AMyProjectHUD* HUD = Cast<AMyProjectHUD>(PC->GetHUD()))
		{
			HUD->ForceDebugTelemetryOff();
		}

		if (AMyProjectCharacter* Char = PC->GetPawn<AMyProjectCharacter>())
		{
			Char->ForcePortalNavigatorOff();
		}
	}
}

bool HonorKitchenAudioSettings::IsDeathStingActive()
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	return bDeathStingActive;
}

void HonorKitchenAudioSettings::SetDeathStingActive(bool bActive)
{
	using namespace HonorKitchenAudioSettingsPrivate;
	EnsureLoaded();
	bDeathStingActive = bActive;
}
