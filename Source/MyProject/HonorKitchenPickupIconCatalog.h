// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemTypes.h"

class UTexture2D;

/** Текстуры предметов для мира (билборд) и хотбара (Canvas). Импорт: Tools/import_pickup_sprites.py */
class MYPROJECT_API HonorKitchenPickupIconCatalog
{
public:
	static UTexture2D* GetPickupIcon(EInventoryItemType Type);

	/** Текстура загружена и RHI-ресурс готов (иначе DrawTexture / mesh могут упасть). */
	static bool IsRenderable(const UTexture2D* Tex);

private:
	static const TCHAR* AssetPathFor(EInventoryItemType Type);
};
