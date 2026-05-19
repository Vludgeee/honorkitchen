// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenEnemySpriteFrames.h"

bool FHonorKitchenEnemySpriteFrames::HasAnyTexture() const
{
	return !Front.IsNull() || !Back.IsNull() || !Chase.IsNull() || !Attack.IsNull();
}

bool FHonorKitchenEnemySpriteFrames::HasAnyLoadedTexture() const
{
	return Resolve(EHonorKitchenEnemySpriteView::Front) != nullptr
		|| Resolve(EHonorKitchenEnemySpriteView::Back) != nullptr
		|| Resolve(EHonorKitchenEnemySpriteView::Attack) != nullptr
		|| Resolve(EHonorKitchenEnemySpriteView::Chase) != nullptr;
}

void FHonorKitchenEnemySpriteFrames::PreloadAll() const
{
	auto Load = [](const TSoftObjectPtr<UTexture2D>& Ptr)
	{
		if (!Ptr.IsNull())
		{
			Ptr.LoadSynchronous();
		}
	};
	Load(Front);
	Load(Back);
	Load(Chase);
	Load(Attack);
}

UTexture2D* FHonorKitchenEnemySpriteFrames::Resolve(EHonorKitchenEnemySpriteView View) const
{
	auto LoadFrame = [](TSoftObjectPtr<UTexture2D> Ptr) -> UTexture2D*
	{
		if (Ptr.IsNull())
		{
			return nullptr;
		}
		if (UTexture2D* Loaded = Ptr.LoadSynchronous())
		{
			return Loaded;
		}
		const FSoftObjectPath Path = Ptr.ToSoftObjectPath();
		if (Path.IsValid())
		{
			return Cast<UTexture2D>(Path.TryLoad());
		}
		return nullptr;
	};

	switch (View)
	{
	case EHonorKitchenEnemySpriteView::Back:
		if (UTexture2D* T = LoadFrame(Back))
		{
			return T;
		}
		break;
	case EHonorKitchenEnemySpriteView::Chase:
		if (UTexture2D* T = LoadFrame(Chase))
		{
			return T;
		}
		break;
	case EHonorKitchenEnemySpriteView::Attack:
		if (UTexture2D* T = LoadFrame(Attack))
		{
			return T;
		}
		break;
	case EHonorKitchenEnemySpriteView::Front:
	default:
		break;
	}

	if (UTexture2D* T = LoadFrame(Front))
	{
		return T;
	}
	return LoadFrame(Back);
}
