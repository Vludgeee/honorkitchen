// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "HonorKitchenEnemySpriteFrames.generated.h"

UENUM(BlueprintType)
enum class EHonorKitchenEnemySpriteView : uint8
{
	Front,
	Back,
	Chase,
	Attack
};

/** Набор PNG-кадров для одного типа врага. Chase может совпадать с Front, если отдельного файла нет. */
USTRUCT(BlueprintType)
struct FHonorKitchenEnemySpriteFrames
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprite")
	TSoftObjectPtr<UTexture2D> Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprite")
	TSoftObjectPtr<UTexture2D> Back;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprite")
	TSoftObjectPtr<UTexture2D> Chase;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprite")
	TSoftObjectPtr<UTexture2D> Attack;

	bool HasAnyTexture() const;
	bool HasAnyLoadedTexture() const;
	void PreloadAll() const;
	UTexture2D* Resolve(EHonorKitchenEnemySpriteView View) const;
};
