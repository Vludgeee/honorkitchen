// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenPickupIconCatalog.h"
#include "Engine/Texture2D.h"
#include "UObject/UObjectGlobals.h"

const TCHAR* HonorKitchenPickupIconCatalog::AssetPathFor(EInventoryItemType Type)
{
	switch (Type)
	{
	case EInventoryItemType::Battery:
		return TEXT("/Game/UI/PickupSprites/T_PickupBattery.T_PickupBattery");
	case EInventoryItemType::Crumb:
		return TEXT("/Game/UI/PickupSprites/T_PickupCrumb.T_PickupCrumb");
	case EInventoryItemType::Salt:
		return TEXT("/Game/UI/PickupSprites/T_PickupSalt.T_PickupSalt");
	case EInventoryItemType::Water:
		return TEXT("/Game/UI/PickupSprites/T_PickupWater.T_PickupWater");
	default:
		return nullptr;
	}
}

bool HonorKitchenPickupIconCatalog::IsRenderable(const UTexture2D* Tex)
{
	if (!Tex || Tex->HasAnyFlags(RF_BeginDestroyed))
	{
		return false;
	}
	if (Tex->GetSizeX() <= 0 || Tex->GetSizeY() <= 0)
	{
		return false;
	}
	return Tex->GetResource() != nullptr;
}

UTexture2D* HonorKitchenPickupIconCatalog::GetPickupIcon(EInventoryItemType Type)
{
	const TCHAR* Path = AssetPathFor(Type);
	if (!Path)
	{
		return nullptr;
	}

	static TMap<uint8, TObjectPtr<UTexture2D>> Cache;
	const uint8 Key = static_cast<uint8>(Type);
	if (TObjectPtr<UTexture2D>* Existing = Cache.Find(Key))
	{
		if (IsRenderable(Existing->Get()))
		{
			return Existing->Get();
		}
		Cache.Remove(Key);
	}

	if (UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, Path))
	{
		if (IsRenderable(Loaded))
		{
			Cache.Add(Key, Loaded);
			return Loaded;
		}
	}

	return nullptr;
}
