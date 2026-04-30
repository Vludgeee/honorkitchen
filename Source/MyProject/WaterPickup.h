// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "WaterPickup.generated.h"

/** Вода: ПКМ — временное отключение зрения у врагов (см. DispatchHotbarUse). */
UCLASS(Blueprintable)
class MYPROJECT_API AWaterPickup : public APickupBase
{
	GENERATED_BODY()

public:
	AWaterPickup();

protected:
	virtual void ApplyDefaultVisual() override;
};
