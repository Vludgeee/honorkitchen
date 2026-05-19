// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenPortalSpriteCatalog.h"

namespace HonorKitchenPortalSpriteCatalogPrivate
{
	static TSoftObjectPtr<UTexture2D> PortalTexture(const TCHAR* Path)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path));
	}
}

namespace HonorKitchenPortalSpriteCatalog
{
	FHonorKitchenEnemySpriteFrames MakePortalFrames()
	{
		using namespace HonorKitchenPortalSpriteCatalogPrivate;
		FHonorKitchenEnemySpriteFrames F;
		const TCHAR* Path = TEXT("/Game/Portal/PortalSprite.PortalSprite");
		F.Front = PortalTexture(Path);
		F.Back = F.Front;
		F.Chase = F.Front;
		F.Attack = F.Front;
		return F;
	}
}
