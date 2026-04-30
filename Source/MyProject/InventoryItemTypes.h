// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemTypes.generated.h"

UENUM(BlueprintType)
enum class EInventoryItemType : uint8
{
	None UMETA(DisplayName = "None"),
	Crumb UMETA(DisplayName = "Crumb"),
	Medkit UMETA(DisplayName = "Medkit"),
	Battery UMETA(DisplayName = "Battery"),
	Salt UMETA(DisplayName = "Salt"),
	Water UMETA(DisplayName = "Water"),
	Magnet UMETA(DisplayName = "Magnet")
};

