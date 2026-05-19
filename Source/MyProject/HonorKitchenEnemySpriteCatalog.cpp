// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenEnemySpriteCatalog.h"

namespace HonorKitchenEnemySpriteCatalogPrivate
{
	static TSoftObjectPtr<UTexture2D> Tex(const TCHAR* Path)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path));
	}
}

// Имена совпадают с Interchange (имя PNG без пробелов): Tomatosaur_front, Karavaychick_front, …

FHonorKitchenEnemySpriteFrames HonorKitchenEnemySpriteCatalog::MakeTomatosaurFrames()
{
	using namespace HonorKitchenEnemySpriteCatalogPrivate;
	FHonorKitchenEnemySpriteFrames F;
	F.Front = Tex(TEXT("/Game/Enemies/Sprites/Tomatosaur/Tomatosaur_front.Tomatosaur_front"));
	F.Back = Tex(TEXT("/Game/Enemies/Sprites/Tomatosaur/Tomatosaur_back.Tomatosaur_back"));
	F.Attack = Tex(TEXT("/Game/Enemies/Sprites/Tomatosaur/Tomatosaur_attack.Tomatosaur_attack"));
	F.Chase = F.Attack;
	return F;
}

FHonorKitchenEnemySpriteFrames HonorKitchenEnemySpriteCatalog::MakeKaravaychickFrames()
{
	using namespace HonorKitchenEnemySpriteCatalogPrivate;
	FHonorKitchenEnemySpriteFrames F;
	F.Front = Tex(TEXT("/Game/Enemies/Sprites/Karavaychick/Karavaychick_front.Karavaychick_front"));
	F.Back = Tex(TEXT("/Game/Enemies/Sprites/Karavaychick/Karavaychick_back.Karavaychick_back"));
	F.Attack = Tex(TEXT("/Game/Enemies/Sprites/Karavaychick/Karavaychick_attack.Karavaychick_attack"));
	F.Chase = F.Attack;
	return F;
}

FHonorKitchenEnemySpriteFrames HonorKitchenEnemySpriteCatalog::MakeVilokhvostFrames()
{
	using namespace HonorKitchenEnemySpriteCatalogPrivate;
	FHonorKitchenEnemySpriteFrames F;
	F.Front = Tex(TEXT("/Game/Enemies/Sprites/Vilokhvost/Vilokvost.Vilokvost"));
	return F;
}
