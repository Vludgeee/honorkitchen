// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenAudioDefaults.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

namespace HonorKitchenAudioDefaultsPrivate
{
	static USoundBase* LoadFirstSound(const TArray<FString>& Paths)
	{
		for (const FString& Path : Paths)
		{
			if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path))
			{
				return S;
			}
		}
		return nullptr;
	}

	static USoundBase* CachedDamage = nullptr;
	static USoundBase* CachedUi = nullptr;
	static USoundBase* CachedAttack = nullptr;
	static USoundBase* CachedPickup = nullptr;
	static USoundBase* CachedChase = nullptr;
	static USoundBase* CachedGrunt = nullptr;
}

USoundBase* HonorKitchenAudioDefaults::GetDamageTakenSound()
{
	using namespace HonorKitchenAudioDefaultsPrivate;
	if (!CachedDamage)
	{
		CachedDamage = LoadFirstSound({
			TEXT("/Game/FirstPerson/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"),
			TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"),
			TEXT("/Game/FirstPerson/Audio/FirstPersonTemplateWeaponFire01.FirstPersonTemplateWeaponFire01"),
			TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire01.FirstPersonTemplateWeaponFire01"),
			TEXT("/Engine/EditorSounds/Notifications/Alert.Alert"),
			TEXT("/Engine/EditorSounds/Notifications/CompileFailed.CompileFailed"),
		});
	}
	return CachedDamage;
}

USoundBase* HonorKitchenAudioDefaults::GetUiClickSound()
{
	using namespace HonorKitchenAudioDefaultsPrivate;
	if (!CachedUi)
	{
		CachedUi = LoadFirstSound({
			TEXT("/Game/FirstPerson/Audio/FirstPersonTemplateWeaponFire01.FirstPersonTemplateWeaponFire01"),
			TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire01.FirstPersonTemplateWeaponFire01"),
			TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"),
			TEXT("/Engine/EditorSounds/Notifications/Notification.Notification"),
		});
	}
	return CachedUi;
}

USoundBase* HonorKitchenAudioDefaults::GetAttackHitSound()
{
	using namespace HonorKitchenAudioDefaultsPrivate;
	if (!CachedAttack)
	{
		CachedAttack = LoadFirstSound({
			TEXT("/Game/FirstPerson/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"),
			TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"),
			TEXT("/Engine/EditorSounds/Notifications/Alert.Alert"),
		});
	}
	return CachedAttack;
}

USoundBase* HonorKitchenAudioDefaults::GetPickupSound()
{
	using namespace HonorKitchenAudioDefaultsPrivate;
	if (!CachedPickup)
	{
		CachedPickup = LoadFirstSound({
			TEXT("/Game/FirstPerson/Audio/FirstPersonTemplateWeaponFire01.FirstPersonTemplateWeaponFire01"),
			TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire01.FirstPersonTemplateWeaponFire01"),
			TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"),
		});
	}
	return CachedPickup;
}

USoundBase* HonorKitchenAudioDefaults::GetChaseStartSound()
{
	using namespace HonorKitchenAudioDefaultsPrivate;
	if (!CachedChase)
	{
		CachedChase = LoadFirstSound({
			TEXT("/Game/FirstPerson/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"),
			TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"),
			TEXT("/Engine/EditorSounds/Notifications/CompileFailed.CompileFailed"),
		});
	}
	return CachedChase;
}

USoundBase* HonorKitchenAudioDefaults::GetEnemyGruntSound()
{
	using namespace HonorKitchenAudioDefaultsPrivate;
	if (!CachedGrunt)
	{
		CachedGrunt = GetChaseStartSound();
	}
	return CachedGrunt;
}

void HonorKitchenAudioDefaults::AssignIfNull(TObjectPtr<USoundBase>& Slot, USoundBase* Fallback)
{
	if (!Slot && Fallback)
	{
		Slot = Fallback;
	}
}
