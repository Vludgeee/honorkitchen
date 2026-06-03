// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"



class USoundBase;



/** Атмосферные SFX кухни: KitchenReady, фоновый loop, редкие one-shot в раунде. */

class MYPROJECT_API HonorKitchenAtmosphereAudio

{

public:

	static USoundBase* GetKitchenReadySound();

	static USoundBase* GetRoundAmbientLoop();

	static USoundBase* PickRandomRoundSting();

};

