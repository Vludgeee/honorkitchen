// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "CrumbPickup.generated.h"

class AMyProjectCharacter;

/** Крошка: подбирается в хотбар, бросается как снаряд. */
UCLASS(Blueprintable)
class MYPROJECT_API ACrumbPickup : public APickupBase
{
	GENERATED_BODY()

public:
	ACrumbPickup();

protected:
	virtual void ApplyDefaultVisual() override;
};
