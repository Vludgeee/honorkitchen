// Copyright Epic Games, Inc. All Rights Reserved.

#include "KitchenGenerator.h"
#include "KitchenClusterActor.h"
#include "KitchenFridge.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/TextRenderActor.h"
#include "GameFramework/DefaultPhysicsVolume.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "HAL/PlatformTime.h"
#include "Portal.h"
#include "Math/RotationMatrix.h"
#include "PickupBase.h"
#include "BatteryPickup.h"
#include "MedkitPickup.h"
#include "SaltPickup.h"
#include "WaterPickup.h"
#include "CrumbPickup.h"
#include "TomatoSaurusCharacter.h"
#include "KaravaychikCharacter.h"
#include "VilokhvostCharacter.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/ConstructorHelpers.h"
#include "HonorKitchenDevDebug.h"

namespace
{
	bool IsProtectedStripActor(const AActor* A)
	{
		if (!A)
		{
			return true;
		}
		if (A->ActorHasTag(TEXT("KeepOnKitchenGen")))
		{
			return true;
		}
		if (A->IsA<APlayerStart>() || A->IsA<APawn>() || A->IsA<APlayerController>())
		{
			return true;
		}
		if (A->IsA<ALight>())
		{
			return true;
		}
		if (A->IsA<ANavigationData>() || A->IsA<ANavMeshBoundsVolume>())
		{
			return true;
		}
		if (A->IsA<AKitchenGenerator>())
		{
			return true;
		}
		if (A->IsA<ASkyAtmosphere>() || A->IsA<ADirectionalLight>() || A->IsA<ASkyLight>())
		{
			return true;
		}
		if (A->IsA<AExponentialHeightFog>() || A->IsA<APostProcessVolume>())
		{
			return true;
		}
		if (A->IsA<ADefaultPhysicsVolume>())
		{
			return true;
		}
		return false;
	}
}

FName AKitchenGenerator::KitchenGeneratedTag()
{
	static const FName Tag(TEXT("KitchenGenerated"));
	return Tag;
}

AKitchenGenerator::AKitchenGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	FloorISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FloorISM"));
	FloorISM->SetupAttachment(SceneRoot);
	FloorISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FloorISM->SetCollisionObjectType(ECC_WorldStatic);
	FloorISM->SetCollisionResponseToAllChannels(ECR_Block);
	WallISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallISM"));
	WallISM->SetupAttachment(SceneRoot);
	WallISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallISM->SetCollisionObjectType(ECC_WorldStatic);
	WallISM->SetCollisionResponseToAllChannels(ECR_Block);
	CeilingISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CeilingISM"));
	CeilingISM->SetupAttachment(SceneRoot);
	CeilingISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CeilingISM->SetCollisionObjectType(ECC_WorldStatic);
	CeilingISM->SetCollisionResponseToAllChannels(ECR_Block);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		FloorISM->SetStaticMesh(Cube.Object);
		WallISM->SetStaticMesh(Cube.Object);
		CeilingISM->SetStaticMesh(Cube.Object);
	}
}

bool AKitchenGenerator::HasValidCoreLoop() const
{
	if (MapWidthCells <= 0 || MapHeightCells <= 0 || CellWalkable.Num() == 0)
	{
		return false;
	}
	const FIntPoint Start = GetStartCell();
	if (!IsCellInBounds(Start) || !IsCellInBounds(PortalCell))
	{
		return false;
	}
	if (Start == PortalCell)
	{
		return false;
	}
	if (!BFSGridReachable(Start, PortalCell, CellWalkable))
	{
		return false;
	}
	if (BatteryCells.Num() == 0)
	{
		return false;
	}
	for (const FIntPoint& B : BatteryCells)
	{
		if (!IsCellInBounds(B))
		{
			return false;
		}
		if (!BFSGridReachable(Start, B, CellWalkable))
		{
			return false;
		}
		if (!BFSGridReachable(B, PortalCell, CellWalkable))
		{
			return false;
		}
	}
	return true;
}

void AKitchenGenerator::RegisterGenerated(AActor* A)
{
	if (!A || A == this)
	{
		return;
	}
	A->Tags.AddUnique(KitchenGeneratedTag());
	GeneratedActors.Add(A);
}

void AKitchenGenerator::DestroyTaggedAndStored()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		GeneratedActors.Reset();
		return;
	}

	for (AActor* A : GeneratedActors)
	{
		if (IsValid(A))
		{
			A->Destroy();
		}
	}
	GeneratedActors.Reset();

	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (A && A != this && A->ActorHasTag(KitchenGeneratedTag()))
		{
			A->Destroy();
		}
	}
}

void AKitchenGenerator::DestroyGameplayTransientActors()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	auto DestroyClass = [&](UClass* Cls)
	{
		if (!Cls)
		{
			return;
		}
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(W, Cls, Found);
		for (AActor* A : Found)
		{
			if (A && A != this)
			{
				A->Destroy();
			}
		}
	};

	DestroyClass(APickupBase::StaticClass());
	DestroyClass(APortal::StaticClass());
	DestroyClass(ATomatoSaurusCharacter::StaticClass());
	DestroyClass(AKaravaychikCharacter::StaticClass());
	DestroyClass(AVilokhvostCharacter::StaticClass());
}

void AKitchenGenerator::StripTemplateStaticMeshesIfNeeded()
{
	if (!bRemoveTemplateStaticMeshes)
	{
		return;
	}
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		AStaticMeshActor* SMA = *It;
		if (!SMA || IsProtectedStripActor(SMA))
		{
			continue;
		}
		SMA->Destroy();
	}

	// Дополнительно убираем template-артефакты текста/декора, которые не являются StaticMeshActor.
	for (TActorIterator<ATextRenderActor> ItText(W); ItText; ++ItText)
	{
		ATextRenderActor* Txt = *ItText;
		if (!Txt || IsProtectedStripActor(Txt))
		{
			continue;
		}
		Txt->Destroy();
	}
	for (TActorIterator<AActor> ItA(W); ItA; ++ItA)
	{
		AActor* A = *ItA;
		if (!A || IsProtectedStripActor(A))
		{
			continue;
		}
		const FString Name = A->GetName();
		if (Name.Contains(TEXT("Template"), ESearchCase::IgnoreCase))
		{
			A->Destroy();
		}
	}
}

void AKitchenGenerator::ClearLevel()
{
	DestroyTaggedAndStored();
	DestroyGameplayTransientActors();
	FloorISM->ClearInstances();
	WallISM->ClearInstances();
	CeilingISM->ClearInstances();
	CellZones.Reset();
	CellTopology.Reset();
	CellWalkable.Reset();
	MapWidthCells = 0;
	MapHeightCells = 0;
	StartGridCell = FIntPoint::ZeroValue;
	RoomCenters.Reset();
	RoomCellMins.Reset();
	RoomCellMaxs.Reset();
	RoomGraph.Reset();
	BatteryCells.Reset();
	EnemySpawnWorldLocations.Reset();
	PortalCell = FIntPoint::ZeroValue;
	PortalRoomGraphIndex = INDEX_NONE;
	LastPortalWorldLocation = FVector::ZeroVector;
	LastPortalIntoRoom = FVector::ForwardVector;
}

void AKitchenGenerator::EnsureOddGrid()
{
	GridCells = FMath::Clamp(GridCells, 5, 15);
	RoomTileSize = FMath::Clamp(RoomTileSize, 10, 14);
	MinRoomsPerRun = FMath::Clamp(MinRoomsPerRun, 12, 16);
	MaxRoomsPerRun = FMath::Clamp(MaxRoomsPerRun, 12, 16);
	if (MinRoomsPerRun > MaxRoomsPerRun)
	{
		Swap(MinRoomsPerRun, MaxRoomsPerRun);
	}
	MinDoorTiles = FMath::Clamp(MinDoorTiles, 1, 3);
	MaxDoorTiles = FMath::Clamp(MaxDoorTiles, 1, 3);

	// Гарантия минимальной ширины прохода по ТЗ.
	const int32 RequiredDoorTilesByWidth = FMath::Max(1, FMath::CeilToInt(MinPassageWidthUU / FMath::Max(1.f, CellSizeUU)));
	MinDoorTiles = FMath::Max(MinDoorTiles, RequiredDoorTilesByWidth);
	MaxDoorTiles = FMath::Max(MaxDoorTiles, MinDoorTiles);
	MinDoorTiles = FMath::Clamp(MinDoorTiles, 1, 6);
	MaxDoorTiles = FMath::Clamp(MaxDoorTiles, MinDoorTiles, 6);

	if (MinDoorTiles > MaxDoorTiles)
	{
		Swap(MinDoorTiles, MaxDoorTiles);
	}
}

bool AKitchenGenerator::IsCellInBounds(FIntPoint C) const
{
	return C.X >= 0 && C.X < MapWidthCells && C.Y >= 0 && C.Y < MapHeightCells;
}

bool AKitchenGenerator::IsCellRoom(FIntPoint C) const
{
	return IsCellInBounds(C) && CellTopology[C.Y][C.X] == EKitchenCellTopology::Room;
}

bool AKitchenGenerator::IsCellInPortalRoom(FIntPoint C) const
{
	if (!IsCellRoom(C))
	{
		return false;
	}
	if (!RoomCellMins.IsValidIndex(PortalRoomGraphIndex) || !RoomCellMaxs.IsValidIndex(PortalRoomGraphIndex))
	{
		return true;
	}
	const FIntPoint& Min = RoomCellMins[PortalRoomGraphIndex];
	const FIntPoint& Max = RoomCellMaxs[PortalRoomGraphIndex];
	return C.X >= Min.X && C.X <= Max.X && C.Y >= Min.Y && C.Y <= Max.Y;
}

bool AKitchenGenerator::IsCellPassage(FIntPoint C) const
{
	return IsCellInBounds(C) && CellTopology[C.Y][C.X] == EKitchenCellTopology::Passage;
}

EKitchenCellZone AKitchenGenerator::PickZoneForRoom(FIntPoint RoomCell, int32 SeedSalt) const
{
	const uint32 HX = static_cast<uint32>(RoomCell.X * 73856093);
	const uint32 HY = static_cast<uint32>(RoomCell.Y * 19349663);
	const uint32 HS = static_cast<uint32>(SeedSalt * 83492791);
	const uint32 H = HX ^ HY ^ HS;
	switch (H % 4u)
	{
	case 0: return EKitchenCellZone::Dining;
	case 1: return EKitchenCellZone::Cooking;
	case 2: return EKitchenCellZone::SinkArea;
	default: return EKitchenCellZone::Storage;
	}
}

void AKitchenGenerator::GenerateGrid(FRandomStream& Rand)
{
	const int32 RoomsCount = FMath::Clamp(Rand.RandRange(MinRoomsPerRun, MaxRoomsPerRun), 12, 16);
	const int32 SlotGrid = 5;
	const int32 SlotSize = FMath::Clamp(RoomTileSize, 10, 14);
	MapWidthCells = SlotGrid * SlotSize + 2;
	MapHeightCells = SlotGrid * SlotSize + 2;

	CellZones.SetNum(MapHeightCells);
	CellTopology.SetNum(MapHeightCells);
	CellWalkable.SetNum(MapHeightCells);
	for (int32 Y = 0; Y < MapHeightCells; ++Y)
	{
		CellZones[Y].SetNum(MapWidthCells);
		CellTopology[Y].SetNum(MapWidthCells);
		CellWalkable[Y].Init(0, MapWidthCells);
		for (int32 X = 0; X < MapWidthCells; ++X)
		{
			CellZones[Y][X] = EKitchenCellZone::Open;
			CellTopology[Y][X] = EKitchenCellTopology::Blocked;
		}
	}

	auto PickWeightedZone = [&Rand]() -> EKitchenCellZone
	{
		const float R = Rand.FRand();
		if (R < 0.34f) return EKitchenCellZone::Storage;
		if (R < 0.62f) return EKitchenCellZone::Dining;
		if (R < 0.84f) return EKitchenCellZone::Cooking;
		return EKitchenCellZone::SinkArea;
	};

	struct FSlotRoom
	{
		FIntPoint Slot;
		FIntPoint Min;
		FIntPoint Max;
		FIntPoint Center;
		EKitchenCellZone Zone = EKitchenCellZone::Open;
	};

	TArray<FIntPoint> SlotNodes;
	SlotNodes.Add(FIntPoint(2, 2)); // hub
	TArray<TPair<int32, int32>> Edges;

	auto TryAddSlot = [&](FIntPoint S, int32 ParentIdx) -> int32
	{
		if (S.X < 0 || S.X >= SlotGrid || S.Y < 0 || S.Y >= SlotGrid)
		{
			return INDEX_NONE;
		}
		if (SlotNodes.Contains(S))
		{
			return INDEX_NONE;
		}
		const int32 NewIdx = SlotNodes.Add(S);
		Edges.Add(TPair<int32, int32>(ParentIdx, NewIdx));
		return NewIdx;
	};

	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	int32 Guard = 0;
	while (SlotNodes.Num() < RoomsCount && Guard++ < 400)
	{
		TArray<int32> ParentOrder;
		ParentOrder.Reserve(SlotNodes.Num());
		for (int32 I = 0; I < SlotNodes.Num(); ++I)
		{
			ParentOrder.Add(I);
		}
		for (int32 I = ParentOrder.Num() - 1; I > 0; --I)
		{
			const int32 J = Rand.RandRange(0, I);
			ParentOrder.Swap(I, J);
		}

		bool bAddedAny = false;
		for (const int32 ParentIdx : ParentOrder)
		{
			TArray<FIntPoint> DirOrder;
			DirOrder.Reserve(4);
			for (int32 D = 0; D < 4; ++D)
			{
				DirOrder.Add(Dirs[D]);
			}
			for (int32 I = DirOrder.Num() - 1; I > 0; --I)
			{
				const int32 J = Rand.RandRange(0, I);
				DirOrder.Swap(I, J);
			}

			for (const FIntPoint& D : DirOrder)
			{
				const FIntPoint Candidate = SlotNodes[ParentIdx] + D;
				if (TryAddSlot(Candidate, ParentIdx) != INDEX_NONE)
				{
					bAddedAny = true;
					break;
				}
			}
			if (bAddedAny)
			{
				break;
			}
		}

		if (!bAddedAny)
		{
			break;
		}
	}

	TArray<FSlotRoom> Rooms;
	Rooms.SetNum(SlotNodes.Num());
	for (int32 I = 0; I < SlotNodes.Num(); ++I)
	{
		const FIntPoint S = SlotNodes[I];
		const int32 MinX = 1 + S.X * SlotSize;
		const int32 MinY = 1 + S.Y * SlotSize;
		const int32 MaxX = MinX + SlotSize - 1;
		const int32 MaxY = MinY + SlotSize - 1;
		FSlotRoom R;
		R.Slot = S;
		R.Min = FIntPoint(MinX, MinY);
		R.Max = FIntPoint(MaxX, MaxY);
		R.Center = FIntPoint((MinX + MaxX) / 2, (MinY + MaxY) / 2);
		R.Zone = (I == 0) ? EKitchenCellZone::Dining : PickWeightedZone();
		Rooms[I] = R;

		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const bool bBoundary = (X == MinX || X == MaxX || Y == MinY || Y == MaxY);
				CellTopology[Y][X] = EKitchenCellTopology::Room;
				CellZones[Y][X] = R.Zone;
				CellWalkable[Y][X] = bBoundary ? 0 : 1;
			}
		}
	}

	for (const TPair<int32, int32>& E : Edges)
	{
		if (!Rooms.IsValidIndex(E.Key) || !Rooms.IsValidIndex(E.Value))
		{
			continue;
		}
		const FSlotRoom& A = Rooms[E.Key];
		const FSlotRoom& B = Rooms[E.Value];
		const int32 DoorWidth = Rand.RandRange(MinDoorTiles, MaxDoorTiles);
		const int32 Half = DoorWidth / 2;

		if (A.Slot.Y == B.Slot.Y)
		{
			const bool bRight = B.Slot.X > A.Slot.X;
			const int32 WallX = bRight ? A.Max.X : A.Min.X;
			const int32 CenterY = (A.Center.Y + B.Center.Y) / 2;
			for (int32 DY = -Half; DY <= Half; ++DY)
			{
				const int32 Y = FMath::Clamp(CenterY + DY, A.Min.Y + 1, A.Max.Y - 1);
				CellWalkable[Y][WallX] = 1;
				if (bRight && WallX + 1 < MapWidthCells) { CellWalkable[Y][WallX + 1] = 1; }
				if (!bRight && WallX - 1 >= 0) { CellWalkable[Y][WallX - 1] = 1; }
			}
		}
		else if (A.Slot.X == B.Slot.X)
		{
			const bool bUp = B.Slot.Y > A.Slot.Y;
			const int32 WallY = bUp ? A.Max.Y : A.Min.Y;
			const int32 CenterX = (A.Center.X + B.Center.X) / 2;
			for (int32 DX = -Half; DX <= Half; ++DX)
			{
				const int32 X = FMath::Clamp(CenterX + DX, A.Min.X + 1, A.Max.X - 1);
				CellWalkable[WallY][X] = 1;
				if (bUp && WallY + 1 < MapHeightCells) { CellWalkable[WallY + 1][X] = 1; }
				if (!bUp && WallY - 1 >= 0) { CellWalkable[WallY - 1][X] = 1; }
			}
		}
	}

	RoomGraph.SetNum(Rooms.Num());
	for (const TPair<int32, int32>& E : Edges)
	{
		if (RoomGraph.IsValidIndex(E.Key) && RoomGraph.IsValidIndex(E.Value))
		{
			RoomGraph[E.Key].AddUnique(E.Value);
			RoomGraph[E.Value].AddUnique(E.Key);
		}
	}
	for (int32 I = 1; I < RoomGraph.Num(); ++I)
	{
		if (RoomGraph[I].Num() == 0)
		{
			RoomGraph[I].AddUnique(0);
			RoomGraph[0].AddUnique(I);
		}
	}

	RoomCenters.Reset();
	RoomCellMins.Reset();
	RoomCellMaxs.Reset();
	for (const FSlotRoom& R : Rooms)
	{
		RoomCenters.Add(R.Center);
		RoomCellMins.Add(R.Min);
		RoomCellMaxs.Add(R.Max);
	}
	StartGridCell = RoomCenters[0];
	CellZones[StartGridCell.Y][StartGridCell.X] = EKitchenCellZone::Open;

}

void AKitchenGenerator::SpawnFloorTiles()
{
	FloorISM->ClearInstances();
	WallISM->ClearInstances();
	CeilingISM->ClearInstances();
	const float Inv = 1.f / 100.f;
	const FVector FloorScale(CellSizeUU * Inv, CellSizeUU * Inv, FloorThicknessUU * Inv);
	const FVector CeilingScale(CellSizeUU * Inv, CellSizeUU * Inv, FloorThicknessUU * Inv);
	const FVector WallScale(CellSizeUU * Inv, CellSizeUU * Inv, WallHeightUU * Inv);

	for (int32 Y = 0; Y < MapHeightCells; ++Y)
	{
		for (int32 X = 0; X < MapWidthCells; ++X)
		{
			const FVector Center = CellCenterWorld(X, Y);
			const FVector FloorLoc(Center.X, Center.Y, GridOrigin.Z + FloorThicknessUU * 0.5f);
			const FVector CeilingLoc(Center.X, Center.Y, GridOrigin.Z + WallHeightUU + FloorThicknessUU * 0.5f);
			// No-void правило: пол существует везде, чтобы между комнатами нельзя было провалиться.
			FloorISM->AddInstance(FTransform(FRotator::ZeroRotator, FloorLoc, FloorScale));
			if (CellTopology[Y][X] != EKitchenCellTopology::Blocked)
			{
				CeilingISM->AddInstance(FTransform(FRotator::ZeroRotator, CeilingLoc, CeilingScale));
			}
			if (CellTopology[Y][X] == EKitchenCellTopology::Room && CellWalkable[Y][X] == 0)
			{
				const FVector WallLoc(Center.X, Center.Y, GridOrigin.Z + FloorThicknessUU + WallHeightUU * 0.5f);
				WallISM->AddInstance(FTransform(FRotator::ZeroRotator, WallLoc, WallScale));
			}
		}
	}
}

FVector AKitchenGenerator::CellCenterWorld(int32 GX, int32 GY) const
{
	return GridOrigin + FVector((static_cast<float>(GX) + 0.5f) * CellSizeUU, (static_cast<float>(GY) + 0.5f) * CellSizeUU, 0.f);
}

int32 AKitchenGenerator::ManhattanDist(FIntPoint A, FIntPoint B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

bool AKitchenGenerator::BFSGridReachable(FIntPoint Start, FIntPoint Goal, const TArray<TArray<uint8>>& Walkable)
{
	if (!Walkable.IsValidIndex(Start.Y) || !Walkable[Start.Y].IsValidIndex(Start.X) || Walkable[Start.Y][Start.X] == 0)
	{
		return false;
	}
	if (!Walkable.IsValidIndex(Goal.Y) || !Walkable[Goal.Y].IsValidIndex(Goal.X) || Walkable[Goal.Y][Goal.X] == 0)
	{
		return false;
	}

	TArray<TArray<uint8>> Visited;
	Visited.SetNum(Walkable.Num());
	for (int32 Yi = 0; Yi < Walkable.Num(); ++Yi)
	{
		Visited[Yi].Init(0, Walkable[Yi].Num());
	}

	TArray<FIntPoint> Q;
	Q.Add(Start);
	Visited[Start.Y][Start.X] = 1;

	static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	int32 Qi = 0;
	while (Qi < Q.Num())
	{
		const FIntPoint C = Q[Qi++];
		if (C == Goal)
		{
			return true;
		}
		for (const FIntPoint& D : Dirs)
		{
			const FIntPoint N = C + D;
			if (!Walkable.IsValidIndex(N.Y) || !Walkable[N.Y].IsValidIndex(N.X))
			{
				continue;
			}
			if (Walkable[N.Y][N.X] == 0 || Visited[N.Y][N.X] != 0)
			{
				continue;
			}
			Visited[N.Y][N.X] = 1;
			Q.Add(N);
		}
	}
	return false;
}

bool AKitchenGenerator::BFSGridPath(
	FIntPoint Start,
	FIntPoint Goal,
	const TArray<TArray<uint8>>& Walkable,
	TArray<FIntPoint>& OutPath)
{
	OutPath.Reset();
	if (!BFSGridReachable(Start, Goal, Walkable))
	{
		return false;
	}

	const int32 H = Walkable.Num();
	const int32 W = H > 0 ? Walkable[0].Num() : 0;
	const int32 CellCount = H * W;
	TArray<int32> ParentIdx;
	ParentIdx.Init(INDEX_NONE, CellCount);

	auto Idx = [W](FIntPoint C) { return C.Y * W + C.X; };

	TArray<FIntPoint> Q;
	Q.Add(Start);
	ParentIdx[Idx(Start)] = Idx(Start);

	static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	int32 Qi = 0;
	while (Qi < Q.Num())
	{
		const FIntPoint C = Q[Qi++];
		if (C == Goal)
		{
			TArray<FIntPoint> Rev;
			int32 CurIdx = Idx(Goal);
			while (CurIdx != INDEX_NONE)
			{
				const int32 CX = CurIdx % W;
				const int32 CY = CurIdx / W;
				Rev.Add(FIntPoint(CX, CY));
				if (CurIdx == Idx(Start))
				{
					break;
				}
				CurIdx = ParentIdx[CurIdx];
			}
			for (int32 I = Rev.Num() - 1; I >= 0; --I)
			{
				OutPath.Add(Rev[I]);
			}
			return OutPath.Num() >= 2;
		}
		for (const FIntPoint& D : Dirs)
		{
			const FIntPoint N = C + D;
			if (!Walkable.IsValidIndex(N.Y) || !Walkable[N.Y].IsValidIndex(N.X))
			{
				continue;
			}
			if (Walkable[N.Y][N.X] == 0)
			{
				continue;
			}
			const int32 NI = Idx(N);
			if (ParentIdx[NI] != INDEX_NONE)
			{
				continue;
			}
			ParentIdx[NI] = Idx(C);
			Q.Add(N);
		}
	}
	return false;
}

FIntPoint AKitchenGenerator::WorldToGridCell(const FVector& World) const
{
	if (MapWidthCells <= 0 || MapHeightCells <= 0)
	{
		return FIntPoint::ZeroValue;
	}
	const FVector Local = World - GridOrigin;
	const int32 GX = FMath::Clamp(FMath::FloorToInt(Local.X / CellSizeUU), 0, MapWidthCells - 1);
	const int32 GY = FMath::Clamp(FMath::FloorToInt(Local.Y / CellSizeUU), 0, MapHeightCells - 1);
	return FIntPoint(GX, GY);
}

bool AKitchenGenerator::FindNearestWalkableCell(FIntPoint Hint, FIntPoint& OutCell) const
{
	if (!IsCellInBounds(Hint))
	{
		return false;
	}
	if (CellWalkable[Hint.Y][Hint.X] != 0)
	{
		OutCell = Hint;
		return true;
	}

	TArray<FIntPoint> Q;
	TSet<FIntPoint> Visited;
	Q.Add(Hint);
	Visited.Add(Hint);

	static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	int32 Qi = 0;
	while (Qi < Q.Num())
	{
		const FIntPoint C = Q[Qi++];
		for (const FIntPoint& D : Dirs)
		{
			const FIntPoint N = C + D;
			if (!IsCellInBounds(N) || Visited.Contains(N))
			{
				continue;
			}
			Visited.Add(N);
			if (CellWalkable[N.Y][N.X] != 0)
			{
				OutCell = N;
				return true;
			}
			Q.Add(N);
		}
	}
	return false;
}

void AKitchenGenerator::CompressGridPath(const TArray<FIntPoint>& InPath, TArray<FIntPoint>& OutPath) const
{
	OutPath.Reset();
	if (InPath.Num() == 0)
	{
		return;
	}
	OutPath.Add(InPath[0]);
	for (int32 I = 1; I < InPath.Num() - 1; ++I)
	{
		const FIntPoint PrevDir = InPath[I] - InPath[I - 1];
		const FIntPoint NextDir = InPath[I + 1] - InPath[I];
		if (PrevDir != NextDir)
		{
			OutPath.Add(InPath[I]);
		}
	}
	if (InPath.Num() > 1)
	{
		OutPath.Add(InPath.Last());
	}
}

bool AKitchenGenerator::BuildNavigatorPathFromWorld(const FVector& WorldStart, TArray<FVector>& OutWorldPoints) const
{
	OutWorldPoints.Reset();
	if (MapWidthCells <= 0 || CellWalkable.Num() == 0)
	{
		return false;
	}

	FIntPoint StartCell;
	if (!FindNearestWalkableCell(WorldToGridCell(WorldStart), StartCell))
	{
		return false;
	}

	FIntPoint GoalCell;
	if (!FindNearestWalkableCell(WorldToGridCell(GetPortalWorldLocation()), GoalCell))
	{
		GoalCell = PortalCell;
		if (CellWalkable.IsValidIndex(GoalCell.Y) && CellWalkable[GoalCell.Y].IsValidIndex(GoalCell.X)
			&& CellWalkable[GoalCell.Y][GoalCell.X] == 0)
		{
			if (!FindNearestWalkableCell(GoalCell, GoalCell))
			{
				return false;
			}
		}
	}

	TArray<FIntPoint> GridPath;
	if (!BFSGridPath(StartCell, GoalCell, CellWalkable, GridPath))
	{
		return false;
	}

	TArray<FIntPoint> Compressed;
	CompressGridPath(GridPath, Compressed);
	const TArray<FIntPoint>& UsePath = Compressed.Num() >= 2 ? Compressed : GridPath;

	const float PathZ = GridOrigin.Z + FloorThicknessUU;
	for (const FIntPoint& C : UsePath)
	{
		OutWorldPoints.Add(CellCenterWorld(C) + FVector(0.f, 0.f, PathZ));
	}
	return OutWorldPoints.Num() >= 2;
}

void AKitchenGenerator::PickPortalAndBatteries(int32 NumBatteries, FRandomStream& Rand)
{
	const FIntPoint Start = GetStartCell();
	const auto IsFarEnoughFromStartForPortal = [&](const FIntPoint& Cell) -> bool
	{
		const FVector StartWorld = CellCenterWorld(Start) + FVector(0.f, 0.f, FloorThicknessUU + 55.f);
		const FVector CellWorld = CellCenterWorld(Cell) + FVector(0.f, 0.f, FloorThicknessUU + 55.f);
		return FVector::Dist2D(StartWorld, CellWorld) >= MinPortalDistanceFromStartUU;
	};

	int32 PortalRoomIdx = INDEX_NONE;
	if (RoomCenters.Num() > 1)
	{
		TArray<int32> Dist;
		Dist.Init(-1, RoomCenters.Num());
		TArray<int32> Q;
		Q.Add(0);
		Dist[0] = 0;
		int32 Qi = 0;
		while (Qi < Q.Num())
		{
			const int32 R = Q[Qi++];
			if (!RoomGraph.IsValidIndex(R))
			{
				continue;
			}
			for (const int32 N : RoomGraph[R])
			{
				if (Dist.IsValidIndex(N) && Dist[N] < 0)
				{
					Dist[N] = Dist[R] + 1;
					Q.Add(N);
				}
			}
		}

		int32 BestDepth = -1;
		for (int32 I = 1; I < RoomCenters.Num(); ++I)
		{
			if (Dist[I] < 0)
			{
				continue;
			}
			if (!RoomCenters.IsValidIndex(I) || !IsFarEnoughFromStartForPortal(RoomCenters[I]))
			{
				continue;
			}
			const int32 Degree = RoomGraph.IsValidIndex(I) ? RoomGraph[I].Num() : 0;
			const bool bLeaf = Degree <= 1;
			if (bLeaf && Dist[I] > BestDepth)
			{
				BestDepth = Dist[I];
				PortalRoomIdx = I;
			}
		}
		if (PortalRoomIdx == INDEX_NONE)
		{
			for (int32 I = 1; I < RoomCenters.Num(); ++I)
			{
				if (!RoomCenters.IsValidIndex(I) || !IsFarEnoughFromStartForPortal(RoomCenters[I]))
				{
					continue;
				}
				if (Dist[I] > BestDepth)
				{
					BestDepth = Dist[I];
					PortalRoomIdx = I;
				}
			}
		}

		// Жёсткий fallback: если порог дистанции недостижим для текущего графа, берём самый дальний узел по глубине.
		if (PortalRoomIdx == INDEX_NONE)
		{
			for (int32 I = 1; I < RoomCenters.Num(); ++I)
			{
				if (Dist[I] > BestDepth)
				{
					BestDepth = Dist[I];
					PortalRoomIdx = I;
				}
			}
		}
	}
	PortalRoomGraphIndex = PortalRoomIdx;
	PortalCell = (PortalRoomIdx != INDEX_NONE && RoomCenters.IsValidIndex(PortalRoomIdx)) ? RoomCenters[PortalRoomIdx] : Start;

	TArray<FIntPoint> Pool;
	for (const FIntPoint& Center : RoomCenters)
	{
		const FIntPoint P = Center;
		if (P == Start || P == PortalCell || !IsCellRoom(P))
		{
			continue;
		}
		Pool.Add(P);
	}

	auto ShufflePool = [&]()
	{
		for (int32 I = Pool.Num() - 1; I > 0; --I)
		{
			const int32 J = Rand.RandRange(0, I);
			Pool.Swap(I, J);
		}
	};

	BatteryCells.Reset();
	const int32 Need = FMath::Clamp(NumBatteries, 1, 8);
	TArray<FIntPoint> Reachable;
	for (const FIntPoint& C : Pool)
	{
		if (BFSGridReachable(Start, C, CellWalkable) && BFSGridReachable(C, PortalCell, CellWalkable))
		{
			Reachable.Add(C);
		}
	}

	// Не "рандом-мусор рядом со стартом": сначала выбираем более дальние батарейки.
	while (BatteryCells.Num() < Need && Reachable.Num() > 0)
	{
		int32 BestIdx = 0;
		int32 BestDist = -1;
		for (int32 I = 0; I < Reachable.Num(); ++I)
		{
			const int32 D = ManhattanDist(Start, Reachable[I]);
			if (D > BestDist)
			{
				BestDist = D;
				BestIdx = I;
			}
		}
		BatteryCells.Add(Reachable[BestIdx]);
		Reachable.RemoveAtSwap(BestIdx);
	}

	if (BatteryCells.Num() < Need)
	{
		ShufflePool();
		for (const FIntPoint& C : Pool)
		{
			if (BatteryCells.Num() >= Need)
			{
				break;
			}
			if (!BatteryCells.Contains(C) && C != PortalCell)
			{
				BatteryCells.Add(C);
			}
		}
	}

	while (BatteryCells.Num() > Need)
	{
		BatteryCells.Pop(false);
	}
}

void AKitchenGenerator::SpawnFurniture(FRandomStream& Rand)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	for (int32 Idx = 0; Idx < RoomCenters.Num(); ++Idx)
	{
		if (Idx == PortalRoomGraphIndex)
		{
			continue;
		}
		const FIntPoint Cell = RoomCenters[Idx];
		if (!IsCellInBounds(Cell) || !IsCellRoom(Cell))
		{
			continue;
		}

		const EKitchenCellZone Z = CellZones[Cell.Y][Cell.X];
		const FVector Base = CellCenterWorld(Cell.X, Cell.Y) + FVector(0.f, 0.f, FloorThicknessUU + 2.f);
		const int32 YawBucket = (Cell.X + Cell.Y + LastUsedSeed) % 4;
		const FRotator Yaw(0.f, static_cast<float>(YawBucket) * 90.f, 0.f);

		if (Z == EKitchenCellZone::Storage && (Idx % 2 == 0))
		{
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			if (AKitchenFridge* Fr = W->SpawnActor<AKitchenFridge>(AKitchenFridge::StaticClass(), Base + FVector(0, 0, 40.f), Yaw, Sp))
			{
				RegisterGenerated(Fr);
			}
			continue;
		}

		EKitchenClusterKind Kind = EKitchenClusterKind::Cabinets;
		switch (Z)
		{
		case EKitchenCellZone::Cooking:
			Kind = EKitchenClusterKind::Stove;
			break;
		case EKitchenCellZone::Dining:
			Kind = EKitchenClusterKind::Table;
			break;
		case EKitchenCellZone::SinkArea:
			Kind = EKitchenClusterKind::Sink;
			break;
		case EKitchenCellZone::Storage:
			Kind = EKitchenClusterKind::Cabinets;
			break;
		default:
			break;
		}

		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AKitchenClusterActor* Cl = W->SpawnActor<AKitchenClusterActor>(AKitchenClusterActor::StaticClass(), Base, Yaw, Sp))
		{
			Cl->InitializeCluster(Kind, Rand);
			RegisterGenerated(Cl);
		}
	}
}

FVector AKitchenGenerator::GetPortalWorldLocation() const
{
	if (!LastPortalWorldLocation.IsNearlyZero())
	{
		return LastPortalWorldLocation;
	}
	const float FloorTopZ = GridOrigin.Z + FloorThicknessUU;
	const FVector Center = CellCenterWorld(PortalCell);
	return FVector(Center.X, Center.Y, FloorTopZ + 55.f);
}

FVector AKitchenGenerator::GetPortalRoomFloorCenter() const
{
	const float FloorTopZ = GridOrigin.Z + FloorThicknessUU;
	if (RoomCenters.IsValidIndex(PortalRoomGraphIndex))
	{
		const FIntPoint C = RoomCenters[PortalRoomGraphIndex];
		const FVector XY = CellCenterWorld(C);
		return FVector(XY.X, XY.Y, FloorTopZ);
	}
	return FVector(GetPortalWorldLocation().X, GetPortalWorldLocation().Y, FloorTopZ);
}

bool AKitchenGenerator::ComputePortalWallPlacement(FVector& OutWorldLocation, FVector& OutNormalIntoRoom) const
{
	const FIntPoint Start = GetStartCell();
	if (!IsCellInBounds(PortalCell) || !RoomCellMins.IsValidIndex(PortalRoomGraphIndex)
		|| !RoomCellMaxs.IsValidIndex(PortalRoomGraphIndex))
	{
		return false;
	}

	static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	const FIntPoint& RMin = RoomCellMins[PortalRoomGraphIndex];
	const FIntPoint& RMax = RoomCellMaxs[PortalRoomGraphIndex];
	const FVector StartWorld = CellCenterWorld(Start);
	const float FloorTopZ = GridOrigin.Z + FloorThicknessUU;

	auto CellTouchesOtherRoom = [&](FIntPoint C) -> bool
	{
		for (const FIntPoint& D : Dirs)
		{
			const FIntPoint N = C + D;
			if (IsCellRoom(N) && !IsCellInPortalRoom(N))
			{
				return true;
			}
		}
		return false;
	};

	auto IsWallNeighbor = [&](FIntPoint C, const FIntPoint& D) -> bool
	{
		const FIntPoint N = C + D;
		if (!IsCellInBounds(N))
		{
			return true;
		}
		if (!IsCellInPortalRoom(N))
		{
			return true;
		}
		return CellWalkable[N.Y][N.X] == 0;
	};

	auto DepthIndex = [&](FIntPoint C) -> int32 { return C.Y * MapWidthCells + C.X; };

	TArray<int32> DepthFromDoor;
	DepthFromDoor.Init(-1, MapWidthCells * MapHeightCells);
	TArray<FIntPoint> BfsQ;

	for (int32 Y = RMin.Y; Y <= RMax.Y; ++Y)
	{
		for (int32 X = RMin.X; X <= RMax.X; ++X)
		{
			const FIntPoint C(X, Y);
			if (!IsCellInPortalRoom(C) || CellWalkable[Y][X] == 0)
			{
				continue;
			}
			if (CellTouchesOtherRoom(C))
			{
				const int32 Idx = DepthIndex(C);
				if (DepthFromDoor[Idx] < 0)
				{
					DepthFromDoor[Idx] = 0;
					BfsQ.Add(C);
				}
			}
		}
	}

	int32 BfsQi = 0;
	while (BfsQi < BfsQ.Num())
	{
		const FIntPoint C = BfsQ[BfsQi++];
		const int32 CurDepth = DepthFromDoor[DepthIndex(C)];
		for (const FIntPoint& D : Dirs)
		{
			const FIntPoint N = C + D;
			if (!IsCellInPortalRoom(N) || !IsCellInBounds(N) || CellWalkable[N.Y][N.X] == 0)
			{
				continue;
			}
			const int32 NIdx = DepthIndex(N);
			if (DepthFromDoor[NIdx] < 0)
			{
				DepthFromDoor[NIdx] = CurDepth + 1;
				BfsQ.Add(N);
			}
		}
	}

	struct FWallCandidate
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		FIntPoint WallDir = FIntPoint::ZeroValue;
		int32 WallFaceCount = 0;
		int32 DepthFromDoor = 0;
		bool bMapExterior = false;
		float DistSqFromStart = 0.f;
	};

	TArray<FWallCandidate> Candidates;
	Candidates.Reserve(96);

	for (int32 Y = RMin.Y; Y <= RMax.Y; ++Y)
	{
		for (int32 X = RMin.X; X <= RMax.X; ++X)
		{
			const FIntPoint C(X, Y);
			if (!IsCellInPortalRoom(C) || CellWalkable[Y][X] == 0 || CellTouchesOtherRoom(C))
			{
				continue;
			}

			const int32 Depth = DepthFromDoor[DepthIndex(C)];
			if (Depth < 0)
			{
				continue;
			}

			TArray<FIntPoint> WallDirs;
			bool bMapExterior = false;
			for (const FIntPoint& D : Dirs)
			{
				if (!IsWallNeighbor(C, D))
				{
					continue;
				}
				WallDirs.Add(D);
				const FIntPoint N = C + D;
				if (!IsCellInBounds(N) || CellTopology[N.Y][N.X] == EKitchenCellTopology::Blocked)
				{
					bMapExterior = true;
				}
			}

			if (WallDirs.Num() == 0)
			{
				continue;
			}

			FIntPoint PickDir = WallDirs[0];
			if (WallDirs.Num() > 1)
			{
				const FVector AwayFromStart = (CellCenterWorld(C) - StartWorld).GetSafeNormal2D();
				float BestDot = -2.f;
				for (const FIntPoint& D : WallDirs)
				{
					const FVector Out(static_cast<float>(D.X), static_cast<float>(D.Y), 0.f);
					const float Dot = FVector::DotProduct(Out, AwayFromStart);
					if (Dot > BestDot)
					{
						BestDot = Dot;
						PickDir = D;
					}
				}
			}

			FWallCandidate Wc;
			Wc.Cell = C;
			Wc.WallDir = PickDir;
			Wc.WallFaceCount = WallDirs.Num();
			Wc.DepthFromDoor = Depth;
			Wc.bMapExterior = bMapExterior;
			Wc.DistSqFromStart = FVector::DistSquared2D(CellCenterWorld(C), StartWorld);
			Candidates.Add(Wc);
		}
	}

	if (Candidates.Num() == 0)
	{
		return false;
	}

	int32 MaxDepth = 0;
	for (const FWallCandidate& Wc : Candidates)
	{
		MaxDepth = FMath::Max(MaxDepth, Wc.DepthFromDoor);
	}

	auto ScoreCandidate = [&](const FWallCandidate& Wc) -> float
	{
		float Score = static_cast<float>(Wc.DepthFromDoor) * 50000000.f;
		if (Wc.DepthFromDoor >= MaxDepth)
		{
			Score += 30000000.f;
		}
		Score += Wc.DistSqFromStart;
		if (Wc.bMapExterior)
		{
			Score += 10000000.f;
		}
		if (Wc.WallFaceCount == 1)
		{
			Score += 5000000.f;
		}
		else if (Wc.WallFaceCount >= 2)
		{
			Score -= 3000000.f;
		}
		return Score;
	};

	const FWallCandidate* Best = &Candidates[0];
	float BestScore = ScoreCandidate(Candidates[0]);
	for (int32 I = 1; I < Candidates.Num(); ++I)
	{
		const float S = ScoreCandidate(Candidates[I]);
		if (S > BestScore)
		{
			BestScore = S;
			Best = &Candidates[I];
		}
	}

	const FVector WallOut(
		static_cast<float>(Best->WallDir.X), static_cast<float>(Best->WallDir.Y), 0.f);
	FVector IntoRoom = (-WallOut).GetSafeNormal2D();
	if (IntoRoom.IsNearlyZero())
	{
		IntoRoom = (CellCenterWorld(Best->Cell) - StartWorld).GetSafeNormal2D();
		if (IntoRoom.IsNearlyZero())
		{
			IntoRoom = FVector::ForwardVector;
		}
	}

	const FVector CellXY = CellCenterWorld(Best->Cell);
	const float TowardWall = FMath::Clamp(CellSizeUU * 0.48f, 120.f, 200.f);
	OutWorldLocation =
		FVector(CellXY.X, CellXY.Y, FloorTopZ + 45.f) + WallOut.GetSafeNormal2D() * TowardWall;
	OutNormalIntoRoom = IntoRoom;
	return true;
}

void AKitchenGenerator::SpawnPortalPickupsAndExtras(FRandomStream& Rand)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	auto SpawnPickup = [&](UClass* Cls, FIntPoint Cell, float ZLift)
	{
		if (!Cls)
		{
			return;
		}
		const FVector BaseLoc = CellCenterWorld(Cell) + FVector(0.f, 0.f, FloorThicknessUU + ZLift);
		for (int32 Attempt = 0; Attempt < 6; ++Attempt)
		{
			FVector Loc = BaseLoc;
			if (Attempt > 0)
			{
				const float Radius = 18.f * static_cast<float>(Attempt);
				const float Angle = Rand.FRandRange(0.f, 2.f * PI);
				Loc += FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
			}
			if (APickupBase* A = W->SpawnActor<APickupBase>(Cls, Loc, FRotator::ZeroRotator, Sp))
			{
				RegisterGenerated(A);
				return;
			}
		}
		++LastPickupSpawnFailures;
	};

	FVector PortalLoc;
	FVector IntoRoom = FVector::ForwardVector;
	if (!ComputePortalWallPlacement(PortalLoc, IntoRoom))
	{
		UE_LOG(LogTemp, Warning, TEXT("KitchenGen: portal wall placement failed, using room-center fallback"));
		const float FloorTopZ = GridOrigin.Z + FloorThicknessUU;
		const FVector Center = CellCenterWorld(PortalCell);
		PortalLoc = FVector(Center.X, Center.Y, FloorTopZ + 55.f);
		IntoRoom = (Center - CellCenterWorld(GetStartCell())).GetSafeNormal2D();
		if (IntoRoom.IsNearlyZero())
		{
			IntoRoom = FVector::ForwardVector;
		}
	}
	LastPortalIntoRoom = IntoRoom.GetSafeNormal2D();
	const FRotator PortalRot = APortal::MakeWallMountRotation(IntoRoom);

	bLastPortalSpawnSucceeded = false;
	LastPortalSpawnAttemptsUsed = 0;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FVector SpawnLoc = PortalLoc;
		if (Attempt > 0)
		{
			const float Radius = 24.f * static_cast<float>(Attempt);
			const float Angle = Rand.FRandRange(0.f, 2.f * PI);
			SpawnLoc += IntoRoom * 8.f + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
		}
		if (APortal* P = W->SpawnActor<APortal>(APortal::StaticClass(), SpawnLoc, PortalRot, Sp))
		{
			P->ApplyWallMount(PortalLoc, IntoRoom);
			LastPortalWorldLocation = P->GetActorLocation();
			RegisterGenerated(P);
			bLastPortalSpawnSucceeded = true;
			LastPortalSpawnAttemptsUsed = Attempt + 1;
			break;
		}
	}
	if (!bLastPortalSpawnSucceeded)
	{
		LastPortalSpawnAttemptsUsed = 8;
	}

	for (const FIntPoint& C : BatteryCells)
	{
		SpawnPickup(ABatteryPickup::StaticClass(), C, 45.f);
	}

	TArray<FIntPoint> ExtraPool;
	for (int32 Y = 0; Y < MapHeightCells; ++Y)
	{
		for (int32 X = 0; X < MapWidthCells; ++X)
		{
			const FIntPoint P(X, Y);
			if (P == GetStartCell() || P == PortalCell || BatteryCells.Contains(P))
			{
				continue;
			}
			if (!IsCellRoom(P))
			{
				continue;
			}
			if (!BFSGridReachable(GetStartCell(), P, CellWalkable))
			{
				continue;
			}
			const FVector CandidateWorld = CellCenterWorld(P) + FVector(0.f, 0.f, FloorThicknessUU + 45.f);
			if (FVector::Dist2D(CandidateWorld, StartCellWorldLocation) < MinExtraPickupDistanceFromStartUU)
			{
				continue;
			}
			ExtraPool.Add(P);
		}
	}
	for (int32 I = ExtraPool.Num() - 1; I > 0; --I)
	{
		const int32 J = Rand.RandRange(0, I);
		ExtraPool.Swap(I, J);
	}

	int32 Idx = 0;
	auto NextExtra = [&]() -> FIntPoint
	{
		if (Idx < ExtraPool.Num())
		{
			return ExtraPool[Idx++];
		}
		return GetStartCell();
	};

	SpawnPickup(AMedkitPickup::StaticClass(), NextExtra(), 45.f);
	SpawnPickup(ASaltPickup::StaticClass(), NextExtra(), 45.f);
	SpawnPickup(AWaterPickup::StaticClass(), NextExtra(), 45.f);
	for (int32 C = 0; C < 3; ++C)
	{
		SpawnPickup(ACrumbPickup::StaticClass(), NextExtra(), 45.f);
	}
}

void AKitchenGenerator::BuildEnemySpawnPoints(FRandomStream& Rand)
{
	EnemySpawnWorldLocations.Reset();
	const FIntPoint Start = GetStartCell();
	TArray<FIntPoint> Candidates;
	for (const FIntPoint& C : RoomCenters)
	{
		if (!IsCellRoom(C))
		{
			continue;
		}
		if (C == Start || C == PortalCell || BatteryCells.Contains(C))
		{
			continue;
		}
		if (ManhattanDist(Start, C) < 2)
		{
			continue;
		}
		if (!BFSGridReachable(Start, C, CellWalkable))
		{
			continue;
		}
		const FVector CandidateWorld = CellCenterWorld(C) + FVector(0.f, 0.f, FloorThicknessUU + 90.f);
		if (FVector::Dist2D(CandidateWorld, StartCellWorldLocation) < MinEnemySpawnDistanceFromStartUU)
		{
			continue;
		}
		Candidates.Add(C);
	}
	for (int32 I = Candidates.Num() - 1; I > 0; --I)
	{
		const int32 J = Rand.RandRange(0, I);
		Candidates.Swap(I, J);
	}

	for (const FIntPoint& C : Candidates)
	{
		if (EnemySpawnWorldLocations.Num() >= 24)
		{
			break;
		}
		const FVector Loc = CellCenterWorld(C) + FVector(0.f, 0.f, FloorThicknessUU + 90.f);
		EnemySpawnWorldLocations.Add(Loc);
	}
}

void AKitchenGenerator::SpawnTomatoesFromPoints()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	LastTomatoSpawnFailures = 0;

	TArray<FVector> SpawnPoints = EnemySpawnWorldLocations;
	if (SpawnPoints.Num() == 0)
	{
		const FIntPoint Start = GetStartCell();
		for (const FIntPoint& C : RoomCenters)
		{
			if (C == Start || C == PortalCell)
			{
				continue;
			}
			if (!BFSGridReachable(Start, C, CellWalkable))
			{
				continue;
			}
			const FVector CandidateSpawn = CellCenterWorld(C) + FVector(0.f, 0.f, FloorThicknessUU + 90.f);
			if (FVector::Dist2D(CandidateSpawn, StartCellWorldLocation) < MinEnemySpawnDistanceFromStartUU)
			{
				continue;
			}
			SpawnPoints.Add(CandidateSpawn);
		}
	}
	if (SpawnPoints.Num() == 0)
	{
		// Жесткий аварийный fallback: даже при пустом графе комнат даем хотя бы одну точку.
		SpawnPoints.Add(StartCellWorldLocation + FVector(350.f, 0.f, 120.f));
	}

	const int32 NumTom = FMath::Clamp(FMath::Max(3, TomatoSpawnCount), 1, 6);
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	const FIntPoint Start = GetStartCell();
	const FVector StartWorld = StartCellWorldLocation + FVector(0.f, 0.f, 120.f);
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	int32 SpawnedCount = 0;
	for (int32 i = 0; i < NumTom; ++i)
	{
		FVector Loc = FVector::ZeroVector;
		if (i == 0)
		{
			// Первый томат: всегда в ближайшей достижимой комнате от старта, чтобы игрок реально его увидел.
			int32 BestDist = TNumericLimits<int32>::Max();
			bool bFoundNear = false;
			for (const FIntPoint& C : RoomCenters)
			{
				if (C == Start || C == PortalCell)
				{
					continue;
				}
				if (!BFSGridReachable(Start, C, CellWalkable))
				{
					continue;
				}
				const int32 D = ManhattanDist(Start, C);
				if (D < 3 || D > 16)
				{
					continue;
				}
				if (D < BestDist)
				{
					BestDist = D;
					Loc = CellCenterWorld(C) + FVector(0.f, 0.f, FloorThicknessUU + 140.f);
					bFoundNear = true;
				}
			}
			if (!bFoundNear)
			{
				Loc = StartWorld + FVector(650.f, 0.f, 0.f);
			}
		}
		else
		{
			const int32 Idx = i % SpawnPoints.Num();
			Loc = SpawnPoints[Idx] + FVector(static_cast<float>(i) * 40.f, 0.f, 40.f);
		}
		if (NavSys)
		{
			FNavLocation NavLoc;
			if (NavSys->ProjectPointToNavigation(Loc, NavLoc, FVector(220.f, 220.f, 350.f)))
			{
				Loc = NavLoc.Location + FVector(0.f, 0.f, 60.f);
			}
		}
		FRandomStream RetryRand(LastUsedSeed + 7919 * (i + 1));
		bool bSpawnedThis = false;
		for (int32 Attempt = 0; Attempt < 10; ++Attempt)
		{
			FVector TryLoc = Loc;
			if (Attempt > 0)
			{
				const float Radius = 45.f * static_cast<float>(Attempt);
				const float Angle = RetryRand.FRandRange(0.f, 2.f * PI);
				TryLoc += FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
				if (NavSys)
				{
					FNavLocation RetryNavLoc;
					if (NavSys->ProjectPointToNavigation(TryLoc, RetryNavLoc, FVector(260.f, 260.f, 380.f)))
					{
						TryLoc = RetryNavLoc.Location + FVector(0.f, 0.f, 60.f);
					}
				}
			}
			if (FVector::Dist2D(TryLoc, StartCellWorldLocation) < MinEnemySpawnDistanceFromStartUU)
			{
				continue;
			}

			if (ATomatoSaurusCharacter* T = W->SpawnActor<ATomatoSaurusCharacter>(ATomatoSaurusCharacter::StaticClass(), TryLoc, FRotator::ZeroRotator, Sp))
			{
				RegisterGenerated(T);
				++SpawnedCount;
				bSpawnedThis = true;
				break;
			}
		}
		if (!bSpawnedThis)
		{
			++LastTomatoSpawnFailures;
		}

		if (!bSpawnedThis)
		{
			HonorKitchenDevDebug::OnScreen(
				21995 + i,
				4.0f,
				FColor::Yellow,
				FString::Printf(TEXT("Tomato spawn skipped (slot=%d): no safe free point"), i));
		}
	}

	HonorKitchenDevDebug::OnScreen(
		21993,
		6.0f,
		SpawnedCount > 0 ? FColor::Red : FColor::Yellow,
		FString::Printf(TEXT("Tomatoes spawned: %d (points=%d)"), SpawnedCount, SpawnPoints.Num()));
	DebugPrintGenerationSummary(true);
}

void AKitchenGenerator::CollectRoomLightCells(int32 RoomIdx, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();
	if (!RoomCellMins.IsValidIndex(RoomIdx) || !RoomCellMaxs.IsValidIndex(RoomIdx))
	{
		return;
	}

	const FIntPoint Min = RoomCellMins[RoomIdx];
	const FIntPoint Max = RoomCellMaxs[RoomIdx];
	const int32 Margin = 1;
	for (int32 Y = Min.Y + Margin; Y <= Max.Y - Margin; ++Y)
	{
		for (int32 X = Min.X + Margin; X <= Max.X - Margin; ++X)
		{
			const FIntPoint C(X, Y);
			if (IsCellRoom(C))
			{
				OutCells.Add(C);
			}
		}
	}
}

FVector AKitchenGenerator::MakeRoomCeilingLightLocation(FIntPoint Cell, FRandomStream& Rng) const
{
	FVector Loc = CellCenterWorld(Cell);
	Loc.Z += WallHeightUU * 0.82f;
	const float Jitter = CellSizeUU * 0.32f;
	Loc.X += Rng.FRandRange(-Jitter, Jitter);
	Loc.Y += Rng.FRandRange(-Jitter, Jitter);
	return Loc;
}

bool AKitchenGenerator::TryPickRoomLightLocation(
	int32 RoomIdx,
	FRandomStream& Rng,
	const TArray<FVector>& ExistingInRoom,
	float MinSeparationUU,
	FVector& OutWorld) const
{
	TArray<FIntPoint> Cells;
	CollectRoomLightCells(RoomIdx, Cells);
	if (Cells.Num() == 0)
	{
		return false;
	}

	const float MinSepSq = MinSeparationUU * MinSeparationUU;
	const int32 MaxTries = FMath::Min(48, Cells.Num() * 6);
	for (int32 Try = 0; Try < MaxTries; ++Try)
	{
		const FIntPoint Cell = Cells[Rng.RandRange(0, Cells.Num() - 1)];
		const FVector Candidate = MakeRoomCeilingLightLocation(Cell, Rng);

		bool bTooClose = false;
		for (const FVector& Existing : ExistingInRoom)
		{
			if (FVector::DistSquared2D(Candidate, Existing) < MinSepSq)
			{
				bTooClose = true;
				break;
			}
		}
		if (!bTooClose)
		{
			OutWorld = Candidate;
			return true;
		}
	}
	return false;
}

void AKitchenGenerator::SpawnRoomPointLightAt(const FVector& WorldLoc, bool bPortalRoom)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APointLight* Light = W->SpawnActor<APointLight>(WorldLoc, FRotator::ZeroRotator, Sp);
	if (!Light)
	{
		return;
	}

	if (UPointLightComponent* LC = Light->PointLightComponent)
	{
		LC->SetMobility(EComponentMobility::Movable);
		LC->SetIntensity(bPortalRoom ? PortalRoomLightIntensity : RoomLightIntensity);
		LC->SetLightColor(bPortalRoom ? PortalRoomLightColor : RoomLightColor);
		LC->SetAttenuationRadius(CellSizeUU * RoomLightAttenuationCells);
		LC->SetCastShadows(true);
	}

	RegisterGenerated(Light);
}

void AKitchenGenerator::SpawnRoomAtmosphereLights(FRandomStream& Rand)
{
	if (!bSpawnRoomAtmosphereLights || RoomCenters.Num() == 0)
	{
		return;
	}

	const int32 StartRoomIdx = 0;
	TArray<int32> DarkCandidates;
	DarkCandidates.Reserve(RoomCenters.Num());
	for (int32 RoomIdx = 0; RoomIdx < RoomCenters.Num(); ++RoomIdx)
	{
		if (RoomIdx == PortalRoomGraphIndex || RoomIdx == StartRoomIdx)
		{
			continue;
		}
		DarkCandidates.Add(RoomIdx);
	}

	const int32 DarkCount = FMath::Clamp(DarkRoomsPerRun, 0, DarkCandidates.Num());
	for (int32 I = DarkCandidates.Num() - 1; I > 0; --I)
	{
		const int32 SwapIdx = Rand.RandRange(0, I);
		DarkCandidates.Swap(I, SwapIdx);
	}

	TSet<int32> DarkRoomIndices;
	for (int32 I = 0; I < DarkCount; ++I)
	{
		DarkRoomIndices.Add(DarkCandidates[I]);
	}

	for (int32 RoomIdx = 0; RoomIdx < RoomCenters.Num(); ++RoomIdx)
	{
		if (DarkRoomIndices.Contains(RoomIdx))
		{
			continue;
		}

		const bool bPortalRoom = (RoomIdx == PortalRoomGraphIndex);
		const int32 LightCount = Rand.FRand() < 0.42f ? 2 : 1;

		float MinSepUU = CellSizeUU * 2.5f;
		if (RoomCellMins.IsValidIndex(RoomIdx) && RoomCellMaxs.IsValidIndex(RoomIdx))
		{
			const FIntPoint& RMin = RoomCellMins[RoomIdx];
			const FIntPoint& RMax = RoomCellMaxs[RoomIdx];
			const float SpanX = static_cast<float>(RMax.X - RMin.X + 1) * CellSizeUU;
			const float SpanY = static_cast<float>(RMax.Y - RMin.Y + 1) * CellSizeUU;
			MinSepUU = FMath::Max(MinSepUU, FMath::Min(SpanX, SpanY) * 0.42f);
		}

		TArray<FVector> PlacedInRoom;
		for (int32 L = 0; L < LightCount; ++L)
		{
			const float Sep = (L == 0) ? 0.f : MinSepUU;
			FVector Loc;
			if (!TryPickRoomLightLocation(RoomIdx, Rand, PlacedInRoom, Sep, Loc))
			{
				break;
			}
			PlacedInRoom.Add(Loc);
			SpawnRoomPointLightAt(Loc, bPortalRoom);
		}
	}
}

void AKitchenGenerator::Regenerate(int32 Seed, int32 NumBatteries)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	LastTopologyAttemptsUsed = 0;
	bLastTopologyValid = false;
	LastNavRegenAttemptsUsed = 0;
	LastPickupSpawnFailures = 0;
	bLastPortalSpawnSucceeded = false;
	LastPortalSpawnAttemptsUsed = 0;
	LastTomatoSpawnFailures = 0;

	ClearLevel();
	StripTemplateStaticMeshesIfNeeded();

	APlayerStart* PS = nullptr;
	for (TActorIterator<APlayerStart> It(W); It; ++It)
	{
		PS = *It;
		break;
	}
	const FVector PSLoc = PS ? PS->GetActorLocation() : FVector::ZeroVector;
	StartCellWorldLocation = FVector(PSLoc.X, PSLoc.Y, PSLoc.Z);

	if (Seed != 0)
	{
		LastUsedSeed = Seed;
	}
	else
	{
		const uint64 Noise = FPlatformTime::Cycles64();
		LastUsedSeed = static_cast<int32>((Noise ^ static_cast<uint64>(GetUniqueID())) & 0x7FFFFFFF);
		if (LastUsedSeed == 0)
		{
			LastUsedSeed = 1;
		}
	}
	FRandomStream MasterRand(LastUsedSeed);
	int32 LayoutSeed = MasterRand.RandRange(1, MAX_int32);
	int32 FurnitureSeed = MasterRand.RandRange(1, MAX_int32);
	int32 LootSeed = MasterRand.RandRange(1, MAX_int32);
	int32 EnemySeed = MasterRand.RandRange(1, MAX_int32);

	EnsureOddGrid();
	GridOrigin = FVector(PSLoc.X, PSLoc.Y, PSLoc.Z - FloorThicknessUU);

	const int32 MaxNavAttempts = 6;
	bLastNavConnectivityToPortal = false;
	for (int32 NavAttempt = 0; NavAttempt < MaxNavAttempts; ++NavAttempt)
	{
		if (NavAttempt > 0)
		{
			DestroyTaggedAndStored();
			DestroyGameplayTransientActors();
			FloorISM->ClearInstances();
			WallISM->ClearInstances();
			CeilingISM->ClearInstances();
			GeneratedActors.Reset();
			EnemySpawnWorldLocations.Reset();
			LastPortalWorldLocation = FVector::ZeroVector;
			LayoutSeed = MasterRand.RandRange(1, MAX_int32) ^ (NavAttempt * 15485867);
			FurnitureSeed = MasterRand.RandRange(1, MAX_int32) ^ (NavAttempt * 32452843);
			LootSeed = MasterRand.RandRange(1, MAX_int32) ^ (NavAttempt * 49979687);
			EnemySeed = MasterRand.RandRange(1, MAX_int32) ^ (NavAttempt * 67867967);
		}

		FRandomStream LayoutRand(LayoutSeed);
		FRandomStream FurnitureRand(FurnitureSeed);
		FRandomStream LootRand(LootSeed);
		FRandomStream EnemyRand(EnemySeed);

		bool bValidTopology = false;
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			GenerateGrid(LayoutRand);
			GridOrigin = FVector(
				PSLoc.X - (static_cast<float>(GetStartCell().X) + 0.5f) * CellSizeUU,
				PSLoc.Y - (static_cast<float>(GetStartCell().Y) + 0.5f) * CellSizeUU,
				PSLoc.Z - FloorThicknessUU);
			PickPortalAndBatteries(NumBatteries, LayoutRand);
			const FIntPoint CurrentStart = GetStartCell();
			bValidTopology = (PortalCell != CurrentStart) && BFSGridReachable(CurrentStart, PortalCell, CellWalkable);
			LastTopologyAttemptsUsed = Attempt + 1;
			if (bValidTopology)
			{
				break;
			}
		}
		bLastTopologyValid = bValidTopology;
		if (!bValidTopology)
		{
			PortalCell = GetStartCell();
			BatteryCells.Reset();
		}
		StartCellWorldLocation = CellCenterWorld(GetStartCell()) + FVector(0.f, 0.f, FloorThicknessUU);

		HonorKitchenDevDebug::OnScreen(
			21990,
			7.0f,
			FColor::Green,
			FString::Printf(TEXT("Start=(%d,%d) navTry=%d"), GetStartCell().X, GetStartCell().Y, NavAttempt + 1));
		HonorKitchenDevDebug::OnScreen(
			21991,
			7.0f,
			FColor::Cyan,
			FString::Printf(TEXT("Portal room=%d cell=(%d,%d)"), PortalRoomGraphIndex, PortalCell.X, PortalCell.Y));
		HonorKitchenDevDebug::OnScreen(
			21992,
			7.0f,
			FColor::Orange,
			FString::Printf(TEXT("Rooms=%d Batteries=%d"), RoomCenters.Num(), BatteryCells.Num()));
		SpawnFloorTiles();
		SpawnFurniture(FurnitureRand);
		SpawnRoomAtmosphereLights(FurnitureRand);
		SpawnPortalPickupsAndExtras(LootRand);
		BuildEnemySpawnPoints(EnemyRand);

		UNavigationSystemV1::UpdateActorAndComponentsInNavOctree(*this);
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
		{
			NavSys->Build();
		}

		ValidateAndCacheNavConnectivityToPortal();
		LastNavRegenAttemptsUsed = NavAttempt + 1;
		if (bLastNavConnectivityToPortal && bLastPortalSpawnSucceeded)
		{
			break;
		}
	}

	DebugPrintGenerationSummary(false);
}

void AKitchenGenerator::DebugPrintGenerationSummary(bool bIncludeTomatoFailures) const
{
	const FString TomFailStr =
		bIncludeTomatoFailures ? FString::FromInt(LastTomatoSpawnFailures) : FString(TEXT("-"));
	HonorKitchenDevDebug::OnScreen(
		21986,
		8.0f,
		FColor::White,
		FString::Printf(
			TEXT("KitchenGen: seed=%d topoTry=%d topoOK=%d navTry=%d pickFail=%d portal=%s(%d) nav=%s tomFail=%s"),
			LastUsedSeed,
			LastTopologyAttemptsUsed,
			bLastTopologyValid ? 1 : 0,
			LastNavRegenAttemptsUsed,
			LastPickupSpawnFailures,
			bLastPortalSpawnSucceeded ? TEXT("ok") : TEXT("no"),
			LastPortalSpawnAttemptsUsed,
			bLastNavConnectivityToPortal ? TEXT("ok") : TEXT("no"),
			*TomFailStr));
}

void AKitchenGenerator::ValidateAndCacheNavConnectivityToPortal()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		bLastNavConnectivityToPortal = true;
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	if (!NavSys)
	{
		bLastNavConnectivityToPortal = true;
		return;
	}

	const FVector PathStartFeet = CellCenterWorld(GetStartCell())
		+ FVector(0.f, 0.f, FloorThicknessUU + 92.f);
	const FVector PortalPoint = GetPortalWorldLocation();
	const FVector ToStart2D = CellCenterWorld(GetStartCell()) - PortalPoint;
	const FVector PortalNavQuery =
		PortalPoint
		+ FVector(ToStart2D.X, ToStart2D.Y, 0.f).GetSafeNormal2D() * FMath::Min(180.f, ToStart2D.Size2D() * 0.35f);

	const FVector QueryExtent(600.f, 600.f, 500.f);

	FNavLocation NavStartProjected;
	FNavLocation NavPortalProjected;
	if (!NavSys->ProjectPointToNavigation(PathStartFeet, NavStartProjected, QueryExtent)
		|| !NavSys->ProjectPointToNavigation(PortalNavQuery, NavPortalProjected, QueryExtent))
	{
		bLastNavConnectivityToPortal = false;
		return;
	}

	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		this,
		NavStartProjected.Location,
		NavPortalProjected.Location,
		this,
		nullptr);
	const bool bPathOk =
		IsValid(Path) && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() >= 2;
	bLastNavConnectivityToPortal = bPathOk;
	HonorKitchenDevDebug::OnScreen(
		21989,
		5.5f,
		bPathOk ? FColor::Green : FColor::Red,
		bPathOk ? TEXT("Kitchen Nav: OK start→portal")
				: TEXT("Kitchen Nav: FAIL start→portal (логический BFS есть, pawn-path нет)"));
}
