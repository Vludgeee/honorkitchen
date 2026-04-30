// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "BatteryPickup.generated.h"

/** Батарейка для победы через портал; из хотбара не выбрасывается (логика в персонаже). */
UCLASS(Blueprintable)
class MYPROJECT_API ABatteryPickup : public APickupBase
{
	GENERATED_BODY()

public:
	ABatteryPickup();

protected:
	virtual void ApplyDefaultVisual() override;
};
