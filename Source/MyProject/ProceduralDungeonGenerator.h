// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralDungeonGenerator.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Процедурная геометрия уровня: лабиринт на сетке из инстансов пола/стен.
 * После GenerateFromSeed нужна перестройка NavMesh (см. GameMode).
 */
UCLASS()
class MYPROJECT_API AProceduralDungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	AProceduralDungeonGenerator();

	/** Сбросить инстансы и построить новый лабиринт. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void GenerateFromSeed(int32 Seed);

	/** Выставить GridWorldOrigin и Z пола так, чтобы стартовая клетка (1,1) совпала с точкой на земле. */
	void AlignStartCellToWorldLocation(const FVector& PlayerStartLocation);

	UPROPERTY(EditAnywhere, Category = "Dungeon", meta = (ClampMin = "100", ClampMax = "600"))
	float CellSize = 200.f;

	UPROPERTY(EditAnywhere, Category = "Dungeon", meta = (ClampMin = "5", ClampMax = "101"))
	int32 GridWidth = 21;

	UPROPERTY(EditAnywhere, Category = "Dungeon", meta = (ClampMin = "5", ClampMax = "101"))
	int32 GridHeight = 21;

	UPROPERTY(EditAnywhere, Category = "Dungeon", meta = (ClampMin = "100", ClampMax = "600"))
	float WallHeight = 280.f;

	UPROPERTY(EditAnywhere, Category = "Dungeon", meta = (ClampMin = "5", ClampMax = "80"))
	float FloorThickness = 24.f;

	/**
	 * Удалить AStaticMeshActor шаблонной карты (пол/стены FirstPerson-комнаты), чтобы лабиринт не оказался «внутри старой геометрии».
	 * Актор с тегом KeepOnKitchenGen не трогаем (тот же тег, что у кухонного генератора).
	 */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Cleanup")
	bool bStripTemplateStaticMeshes = true;

	/** Вызывается из GameMode перед Align/GenerateFromSeed. */
	void StripTemplateStaticMeshesIfConfigured();

	/** Центр клетки (0,0) в мире; Z — центр «плиты» пола. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon")
	FVector GridWorldOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon")
	int32 LastUsedSeed = 0;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Dungeon")
	UInstancedStaticMeshComponent* FloorISM = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Dungeon")
	UInstancedStaticMeshComponent* WallISM = nullptr;

	/** 0 — пол/коридор, 1 — стена. */
	TArray<TArray<uint8>> Cells;

	void EnsureOddGrid();
	void CarveMazeDFS(int32 StartX, int32 StartY, FRandomStream& Rand);
	void RebuildInstances();

	FVector CellCenterWorld(int32 CellX, int32 CellY) const;
};
