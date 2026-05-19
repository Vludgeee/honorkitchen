// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "SaltPickup.generated.h"

/** Соль: ПКМ — временная полоса-барьер перед игроком (см. DispatchHotbarUse). */
UCLASS(Blueprintable)
class MYPROJECT_API ASaltPickup : public APickupBase
{
	GENERATED_BODY()

public:
	ASaltPickup();

protected:
	virtual void ApplyDefaultVisual() override;
};
