// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenEnemySoundCatalog.h"
#include "HonorKitchenAudioSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

namespace HonorKitchenEnemySoundCatalogPrivate
{
	static TArray<USoundBase*> LoadedGroup;
	static FString LastLoadedPath;

	static USoundBase* LoadSound(const TCHAR* AssetPath)
	{
		return LoadObject<USoundBase>(nullptr, AssetPath);
	}

	static void LoadGroup(const TArray<const TCHAR*>& Paths)
	{
		LoadedGroup.Reset();
		for (const TCHAR* Path : Paths)
		{
			if (USoundBase* S = LoadSound(Path))
			{
				LoadedGroup.Add(S);
			}
		}
	}

	static USoundBase* PickFromPaths(const TArray<const TCHAR*>& Paths)
	{
		LoadGroup(Paths);
		if (LoadedGroup.Num() == 0)
		{
			return nullptr;
		}
		const int32 Idx = FMath::RandRange(0, LoadedGroup.Num() - 1);
		return LoadedGroup[Idx];
	}

	static const TArray<const TCHAR*>& PathsFor(EHonorKitchenEnemySpecies Species, EHonorKitchenEnemySoundEvent Event)
	{
		static const TArray<const TCHAR*> TomatoIdle = {
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusIdle.TomatoSaurusIdle"),
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusIdle_2.TomatoSaurusIdle_2"),
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusIdle_3.TomatoSaurusIdle_3"),
		};
		static const TArray<const TCHAR*> TomatoChase = {
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusChase.TomatoSaurusChase"),
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusChase_2.TomatoSaurusChase_2"),
		};
		static const TArray<const TCHAR*> TomatoPunch = {
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusPunch.TomatoSaurusPunch"),
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusPunch_2.TomatoSaurusPunch_2"),
		};
		static const TArray<const TCHAR*> TomatoDamage = {
			TEXT("/Game/Audio/Enemies/TomatoSaurus/TomatoSaurusDamage.TomatoSaurusDamage"),
		};

		static const TArray<const TCHAR*> KaravayIdle = {
			TEXT("/Game/Audio/Enemies/Karavaychik/KaravaychickIdle.KaravaychickIdle"),
		};
		static const TArray<const TCHAR*> KaravayDetected = {
			TEXT("/Game/Audio/Enemies/Karavaychik/KaravaychickDetected.KaravaychickDetected"),
		};
		static const TArray<const TCHAR*> KaravayChase = {
			TEXT("/Game/Audio/Enemies/Karavaychik/KaravaychickChase.KaravaychickChase"),
		};
		static const TArray<const TCHAR*> KaravayPunch = {
			TEXT("/Game/Audio/Enemies/Karavaychik/KaravaychickPunch.KaravaychickPunch"),
			TEXT("/Game/Audio/Enemies/Karavaychik/KaravaychickPunch_2.KaravaychickPunch_2"),
		};
		static const TArray<const TCHAR*> KaravayDamage = {
			TEXT("/Game/Audio/Enemies/Karavaychik/KaravaychickDamage.KaravaychickDamage"),
		};

		static const TArray<const TCHAR*> VilokhvostIdle = {
			TEXT("/Game/Audio/Enemies/Vilokhvost/VilokhvostIdle.VilokhvostIdle"),
			TEXT("/Game/Audio/Enemies/Vilokhvost/VilokhvostIdle_2.VilokhvostIdle_2"),
			TEXT("/Game/Audio/Enemies/Vilokhvost/VilokhvostIdle_3.VilokhvostIdle_3"),
		};
		static const TArray<const TCHAR*> VilokhvostDetected = {
			TEXT("/Game/Audio/Enemies/Vilokhvost/VilokhvostDetected.VilokhvostDetected"),
			TEXT("/Game/Audio/Enemies/Vilokhvost/VilokhvostDetected3.VilokhvostDetected3"),
		};
		static const TArray<const TCHAR*> VilokhvostPunch = {
			TEXT("/Game/Audio/Enemies/Vilokhvost/VilokhvostPunch.VilokhvostPunch"),
		};

		static const TArray<const TCHAR*> Empty;

		if (Species == EHonorKitchenEnemySpecies::TomatoSaurus)
		{
			switch (Event)
			{
			case EHonorKitchenEnemySoundEvent::Idle: return TomatoIdle;
			case EHonorKitchenEnemySoundEvent::Chase: return TomatoChase;
			case EHonorKitchenEnemySoundEvent::Punch: return TomatoPunch;
			case EHonorKitchenEnemySoundEvent::Damage: return TomatoDamage;
			default: return Empty;
			}
		}
		if (Species == EHonorKitchenEnemySpecies::Karavaychik)
		{
			switch (Event)
			{
			case EHonorKitchenEnemySoundEvent::Idle: return KaravayIdle;
			case EHonorKitchenEnemySoundEvent::Detected: return KaravayDetected;
			case EHonorKitchenEnemySoundEvent::Chase: return KaravayChase;
			case EHonorKitchenEnemySoundEvent::Punch: return KaravayPunch;
			case EHonorKitchenEnemySoundEvent::Damage: return KaravayDamage;
			default: return Empty;
			}
		}
		if (Species == EHonorKitchenEnemySpecies::Vilokhvost)
		{
			switch (Event)
			{
			case EHonorKitchenEnemySoundEvent::Idle: return VilokhvostIdle;
			case EHonorKitchenEnemySoundEvent::Detected: return VilokhvostDetected;
			case EHonorKitchenEnemySoundEvent::Punch: return VilokhvostPunch;
			default: return Empty;
			}
		}
		return Empty;
	}
}

USoundBase* HonorKitchenEnemySoundCatalog::PickSound(EHonorKitchenEnemySpecies Species, EHonorKitchenEnemySoundEvent Event)
{
	return HonorKitchenEnemySoundCatalogPrivate::PickFromPaths(
		HonorKitchenEnemySoundCatalogPrivate::PathsFor(Species, Event));
}

void HonorKitchenEnemySoundCatalog::PlayAt(
	AActor* WorldContext,
	const FVector& Location,
	EHonorKitchenEnemySpecies Species,
	EHonorKitchenEnemySoundEvent Event,
	float VolumeMultiplier,
	float PitchMultiplier)
{
	if (!WorldContext || !HonorKitchenAudioSettings::IsSoundEnabled())
	{
		return;
	}

	if (USoundBase* S = PickSound(Species, Event))
	{
		UGameplayStatics::PlaySoundAtLocation(
			WorldContext,
			S,
			Location,
			FRotator::ZeroRotator,
			HonorKitchenAudioSettings::ScaleMonsterVolume(VolumeMultiplier),
			PitchMultiplier);
	}
}

USoundBase* HonorKitchenEnemySoundCatalog::PickTomatoIdleLoop()
{
	return PickSound(EHonorKitchenEnemySpecies::TomatoSaurus, EHonorKitchenEnemySoundEvent::Idle);
}

USoundBase* HonorKitchenEnemySoundCatalog::PickKaravayIdleLoop()
{
	return PickSound(EHonorKitchenEnemySpecies::Karavaychik, EHonorKitchenEnemySoundEvent::Idle);
}

USoundBase* HonorKitchenEnemySoundCatalog::PickVilokhvostIdleLoop()
{
	return PickSound(EHonorKitchenEnemySpecies::Vilokhvost, EHonorKitchenEnemySoundEvent::Idle);
}
