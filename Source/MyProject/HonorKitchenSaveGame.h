// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InventoryItemTypes.h"
#include "HonorKitchenSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FHonorKitchenSavedHotbarSlot
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 ItemType = 0;

	UPROPERTY()
	int32 Amount = 0;
};

/** Снимок одного актора в мире (враг, пикап, портал, снаряд и т.д.). */
USTRUCT(BlueprintType)
struct FHonorKitchenSavedActorRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	/** Для пикапов: EInventoryItemType, иначе 0. */
	UPROPERTY()
	uint8 ItemType = 0;

	UPROPERTY()
	int32 ItemAmount = 0;
};

UCLASS()
class MYPROJECT_API UHonorKitchenSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const TCHAR* DefaultSlotName();

	UPROPERTY()
	int32 SaveVersion = 2;

	UPROPERTY()
	int32 MapSeed = 0;

	UPROPERTY()
	int32 RequiredBatteries = 2;

	UPROPERTY()
	int32 CollectedBatteries = 0;

	UPROPERTY()
	float PlayerHealth = 100.f;

	UPROPERTY()
	float PlayerMaxHealth = 100.f;

	UPROPERTY()
	TArray<FHonorKitchenSavedHotbarSlot> Hotbar;

	UPROPERTY()
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator PlayerRotation = FRotator::ZeroRotator;

	UPROPERTY()
	TArray<FHonorKitchenSavedActorRecord> WorldActors;

	UPROPERTY()
	double SavedWorldTimeSeconds = 0.0;
};
