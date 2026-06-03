// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "KitchenClusterActor.h"
#include "KitchenGenerator.generated.h"

class UInstancedStaticMeshComponent;

UENUM()
enum class EKitchenCellZone : uint8
{
	Open,
	Cooking,
	Storage,
	Dining,
	SinkArea,
};

UENUM()
enum class EKitchenCellTopology : uint8
{
	Blocked,
	Passage,
	Room,
};

/**
 * Комнатно-сеточная кухня: пол (ISM), зоны мебели, портал в дальней клетке, батарейки (BFS при непроходимых клетках),
 * точки для врагов. Очистка сгенерированного и опционально шаблонных StaticMeshActor уровня.
 */
UCLASS()
class MYPROJECT_API AKitchenGenerator : public AActor
{
	GENERATED_BODY()

public:
	AKitchenGenerator();

	/** Полный цикл: очистка → сетка → мебель → портал/пикапы → NPC. */
	UFUNCTION(BlueprintCallable, Category = "Kitchen")
	void Regenerate(int32 Seed, int32 NumBatteries);
	bool HasValidCoreLoop() const;

	FVector GetStartCellWorldLocation() const { return StartCellWorldLocation; }
	const TArray<FVector>& GetEnemySpawnWorldLocations() const { return EnemySpawnWorldLocations; }
	FVector GetPortalWorldLocation() const;
	/** Направление «в комнату» у портала (для вилохвоста и UI). */
	FVector GetPortalIntoRoom() const { return LastPortalIntoRoom; }
	/** Центр пола комнаты с порталом (проходимая клетка). */
	FVector GetPortalRoomFloorCenter() const;

	/** Путь по проходимым клеткам (двери) от мировой позиции к порталу — для полоски-навигатора. */
	bool BuildNavigatorPathFromWorld(const FVector& WorldStart, TArray<FVector>& OutWorldPoints) const;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Grid", meta = (ClampMin = "5", ClampMax = "15"))
	int32 GridCells = 7;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Grid", meta = (ClampMin = "200", ClampMax = "500"))
	float CellSizeUU = 300.f;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Layout", meta = (ClampMin = "10", ClampMax = "14"))
	int32 RoomTileSize = 12;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Layout", meta = (ClampMin = "12", ClampMax = "16"))
	int32 MinRoomsPerRun = 12;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Layout", meta = (ClampMin = "12", ClampMax = "16"))
	int32 MaxRoomsPerRun = 16;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Layout", meta = (ClampMin = "1", ClampMax = "3"))
	int32 MinDoorTiles = 1;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Layout", meta = (ClampMin = "1", ClampMax = "3"))
	int32 MaxDoorTiles = 3;

	/** Минимальная целевая ширина прохода в uu (ТЗ: не уже 150). */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Layout", meta = (ClampMin = "150", ClampMax = "1200"))
	float MinPassageWidthUU = 150.f;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Grid")
	float FloorThicknessUU = 20.f;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Grid", meta = (ClampMin = "220", ClampMax = "1200"))
	float WallHeightUU = 620.f;

	/** Удалить AStaticMeshActor шаблонного уровня — по умолчанию выкл., чтобы не «съедать» лабиринт/FP-пол. */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Cleanup")
	bool bRemoveTemplateStaticMeshes = true;

	/** Сколько томатозавров заспавнить (точки берутся из пула клеток). */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Spawn", meta = (ClampMin = "1", ClampMax = "6"))
	int32 TomatoSpawnCount = 3;

	/** Минимальная дистанция (XY) от старта игрока для спавна врагов. */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Spawn", meta = (ClampMin = "300.0", ClampMax = "4000.0"))
	float MinEnemySpawnDistanceFromStartUU = 900.f;

	/** Минимальная дистанция (XY) от старта для доп.пикапов (не батареек). */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Spawn", meta = (ClampMin = "150.0", ClampMax = "3000.0"))
	float MinExtraPickupDistanceFromStartUU = 550.f;

	/** Минимальная дистанция (XY) от старта до портала. */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Spawn", meta = (ClampMin = "300.0", ClampMax = "6000.0"))
	float MinPortalDistanceFromStartUU = 1200.f;

	/** Point lights внутри комнат на потолке (атмосфера хоррор-кухни). */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Atmosphere")
	bool bSpawnRoomAtmosphereLights = true;

	/** Комнат без света за один Regenerate (случайный выбор, не старт/портал). */
	UPROPERTY(EditAnywhere, Category = "Kitchen|Atmosphere", meta = (ClampMin = "0", ClampMax = "6"))
	int32 DarkRoomsPerRun = 2;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Atmosphere", meta = (ClampMin = "50.0", ClampMax = "12000.0"))
	float RoomLightIntensity = 780.f;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Atmosphere", meta = (ClampMin = "50.0", ClampMax = "12000.0"))
	float PortalRoomLightIntensity = 570.f;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Atmosphere", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float RoomLightAttenuationCells = 2.4f;

	UPROPERTY(EditAnywhere, Category = "Kitchen|Atmosphere")
	FLinearColor RoomLightColor = FLinearColor(1.f, 0.84f, 0.62f);

	UPROPERTY(EditAnywhere, Category = "Kitchen|Atmosphere")
	FLinearColor PortalRoomLightColor = FLinearColor(0.72f, 0.82f, 1.f);

	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Debug")
	int32 LastUsedSeed = 0;

	/** После Regenerate и Nav Build: синхронный путь по NavMesh от старта к порталу. */
	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Debug")
	bool bLastNavConnectivityToPortal = true;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Debug")
	FIntPoint PortalCell = FIntPoint::ZeroValue;

	/** Последний Regenerate: сколько попыток GenerateGrid (1…8) до остановки цикла. */
	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Metrics")
	int32 LastTopologyAttemptsUsed = 0;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Metrics")
	bool bLastTopologyValid = false;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Metrics")
	int32 LastPickupSpawnFailures = 0;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Metrics")
	bool bLastPortalSpawnSucceeded = false;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Metrics")
	int32 LastPortalSpawnAttemptsUsed = 0;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Metrics")
	int32 LastTomatoSpawnFailures = 0;

	/** Сколько полных перегенераций (layout+геометрия) до nav OK (1…6). */
	UPROPERTY(VisibleAnywhere, Category = "Kitchen|Metrics")
	int32 LastNavRegenAttemptsUsed = 0;

	void DebugPrintGenerationSummary(bool bIncludeTomatoFailures) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Kitchen")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen")
	TObjectPtr<UInstancedStaticMeshComponent> FloorISM = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen")
	TObjectPtr<UInstancedStaticMeshComponent> WallISM = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Kitchen")
	TObjectPtr<UInstancedStaticMeshComponent> CeilingISM = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> GeneratedActors;

	TArray<TArray<EKitchenCellZone>> CellZones;
	TArray<TArray<EKitchenCellTopology>> CellTopology;
	TArray<TArray<uint8>> CellWalkable;
	int32 MapWidthCells = 0;
	int32 MapHeightCells = 0;

	FVector GridOrigin = FVector::ZeroVector;
	FVector StartCellWorldLocation = FVector::ZeroVector;
	FIntPoint StartGridCell = FIntPoint::ZeroValue;
	TArray<FIntPoint> RoomCenters;
	TArray<FIntPoint> RoomCellMins;
	TArray<FIntPoint> RoomCellMaxs;
	TArray<TArray<int32>> RoomGraph;
	TArray<FIntPoint> BatteryCells;
	TArray<FVector> EnemySpawnWorldLocations;
	FVector LastPortalWorldLocation = FVector::ZeroVector;
	FVector LastPortalIntoRoom = FVector::ForwardVector;
	int32 PortalRoomGraphIndex = INDEX_NONE;

	static FName KitchenGeneratedTag();

	void ClearLevel();
	void StripTemplateStaticMeshesIfNeeded();
	void DestroyTaggedAndStored();
	void DestroyGameplayTransientActors();

	void EnsureOddGrid();
	void GenerateGrid(FRandomStream& Rand);
	void SpawnFloorTiles();
	void SpawnFurniture(FRandomStream& Rand);
	void PickPortalAndBatteries(int32 NumBatteries, FRandomStream& Rand);
	void SpawnPortalPickupsAndExtras(FRandomStream& Rand);
	bool ComputePortalWallPlacement(FVector& OutWorldLocation, FVector& OutNormalIntoRoom) const;
	void BuildEnemySpawnPoints(FRandomStream& Rand);
	void SpawnTomatoesFromPoints();
	void SpawnRoomAtmosphereLights(FRandomStream& Rand);
	void CollectRoomLightCells(int32 RoomIdx, TArray<FIntPoint>& OutCells) const;
	FVector MakeRoomCeilingLightLocation(FIntPoint Cell, FRandomStream& Rand) const;
	bool TryPickRoomLightLocation(
		int32 RoomIdx,
		FRandomStream& Rand,
		const TArray<FVector>& ExistingInRoom,
		float MinSeparationUU,
		FVector& OutWorld) const;
	void SpawnRoomPointLightAt(const FVector& WorldLoc, bool bPortalRoom);

	/** Обновляет bLastNavConnectivityToPortal после пересборки NavMesh (лучший effort). */
	void ValidateAndCacheNavConnectivityToPortal();
	bool IsCellInBounds(FIntPoint C) const;
	bool IsCellRoom(FIntPoint C) const;
	bool IsCellInPortalRoom(FIntPoint C) const;
	bool IsCellPassage(FIntPoint C) const;
	FIntPoint GetStartCell() const { return StartGridCell; }
	EKitchenCellZone PickZoneForRoom(FIntPoint RoomCell, int32 SeedSalt) const;

	void RegisterGenerated(AActor* A);
	FVector CellCenterWorld(int32 GX, int32 GY) const;
	FVector CellCenterWorld(FIntPoint C) const { return CellCenterWorld(C.X, C.Y); }

	static bool BFSGridReachable(FIntPoint Start, FIntPoint Goal, const TArray<TArray<uint8>>& Walkable);
	static bool BFSGridPath(FIntPoint Start, FIntPoint Goal, const TArray<TArray<uint8>>& Walkable, TArray<FIntPoint>& OutPath);
	static int32 ManhattanDist(FIntPoint A, FIntPoint B);

	FIntPoint WorldToGridCell(const FVector& World) const;
	bool FindNearestWalkableCell(FIntPoint Hint, FIntPoint& OutCell) const;
	void CompressGridPath(const TArray<FIntPoint>& InPath, TArray<FIntPoint>& OutPath) const;
};
