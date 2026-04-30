// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "MedkitPickup.generated.h"

/** Аптечка: подбор в хотбар, использование через ThrowActiveItem / DispatchHotbarUse. */
UCLASS(Blueprintable)
class MYPROJECT_API AMedkitPickup : public APickupBase
{
	GENERATED_BODY()

public:
	AMedkitPickup();

protected:
	virtual void ApplyDefaultVisual() override;
};
