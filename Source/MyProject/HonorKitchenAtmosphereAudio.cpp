// Copyright Epic Games, Inc. All Rights Reserved.



#include "HonorKitchenAtmosphereAudio.h"

#include "Sound/SoundBase.h"

#include "UObject/UObjectGlobals.h"



namespace HonorKitchenAtmosphereAudioPrivate

{

	static const TCHAR* KitchenReadyPath =

		TEXT("/Game/Audio/Atmosphere/KitchenReady_Master.KitchenReady_Master");

	static const TCHAR* RoundAmbientPath =

		TEXT("/Game/Audio/Atmosphere/sound_19805.sound_19805");



	static const TArray<const TCHAR*> RoundStingPaths = {

		TEXT("/Game/Audio/Atmosphere/KitchenDUDUDU_Master.KitchenDUDUDU_Master"),

		TEXT("/Game/Audio/Atmosphere/KitchenGlitch1_Master.KitchenGlitch1_Master"),

		TEXT("/Game/Audio/Atmosphere/KitchenScrip_Master.KitchenScrip_Master"),

		TEXT("/Game/Audio/Atmosphere/KitchenTuTuTuuu_Master.KitchenTuTuTuuu_Master"),

	};



	static USoundBase* LoadSound(const TCHAR* Path)

	{

		return LoadObject<USoundBase>(nullptr, Path);

	}



	static USoundBase* PickFromPaths(const TArray<const TCHAR*>& Paths)

	{

		TArray<USoundBase*> Loaded;

		Loaded.Reserve(Paths.Num());

		for (const TCHAR* Path : Paths)

		{

			if (USoundBase* S = LoadSound(Path); IsValid(S))

			{

				Loaded.Add(S);

			}

		}

		if (Loaded.Num() == 0)

		{

			return nullptr;

		}

		return Loaded[FMath::RandRange(0, Loaded.Num() - 1)];

	}

}



USoundBase* HonorKitchenAtmosphereAudio::GetKitchenReadySound()

{

	using namespace HonorKitchenAtmosphereAudioPrivate;

	USoundBase* const S = LoadSound(KitchenReadyPath);

	return IsValid(S) ? S : nullptr;

}



USoundBase* HonorKitchenAtmosphereAudio::GetRoundAmbientLoop()

{

	using namespace HonorKitchenAtmosphereAudioPrivate;

	USoundBase* const S = LoadSound(RoundAmbientPath);

	return IsValid(S) ? S : nullptr;

}



USoundBase* HonorKitchenAtmosphereAudio::PickRandomRoundSting()

{

	using namespace HonorKitchenAtmosphereAudioPrivate;

	return PickFromPaths(RoundStingPaths);

}

