// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProceduralDungeonGenerator.h"
#include "KitchenGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "UObject/ConstructorHelpers.h"

namespace DungeonStrip
{
	static bool IsProtectedTemplateActor(const AActor* A)
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
		if (A->IsA<AKitchenGenerator>() || A->IsA<AProceduralDungeonGenerator>())
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

namespace
{
	static constexpr int32 StartCellX = 1;
	static constexpr int32 StartCellY = 1;
}

AProceduralDungeonGenerator::AProceduralDungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	FloorISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FloorISM"));
	SetRootComponent(FloorISM);
	FloorISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FloorISM->SetCollisionObjectType(ECC_WorldStatic);
	FloorISM->SetCollisionResponseToAllChannels(ECR_Block);

	WallISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallISM"));
	WallISM->SetupAttachment(FloorISM);
	WallISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallISM->SetCollisionObjectType(ECC_WorldStatic);
	WallISM->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FloorISM->SetStaticMesh(CubeMesh.Object);
		WallISM->SetStaticMesh(CubeMesh.Object);
	}
}

void AProceduralDungeonGenerator::StripTemplateStaticMeshesIfConfigured()
{
	if (!bStripTemplateStaticMeshes)
	{
		return;
	}

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(W, AStaticMeshActor::StaticClass(), Found);
	for (AActor* A : Found)
	{
		if (!A || A == this)
		{
			continue;
		}
		if (DungeonStrip::IsProtectedTemplateActor(A))
		{
			continue;
		}
		A->Destroy();
	}
}

FVector AProceduralDungeonGenerator::CellCenterWorld(int32 CellX, int32 CellY) const
{
	return GridWorldOrigin + FVector(
		(static_cast<float>(CellX) + 0.5f) * CellSize,
		(static_cast<float>(CellY) + 0.5f) * CellSize,
		0.f);
}

void AProceduralDungeonGenerator::AlignStartCellToWorldLocation(const FVector& PlayerStartLocation)
{
	// Центр клетки (1,1) совпадает с XY игрока; Z — центр плиты пола под ногами.
	GridWorldOrigin.X = PlayerStartLocation.X - (static_cast<float>(StartCellX) + 0.5f) * CellSize;
	GridWorldOrigin.Y = PlayerStartLocation.Y - (static_cast<float>(StartCellY) + 0.5f) * CellSize;
	GridWorldOrigin.Z = PlayerStartLocation.Z - FloorThickness * 0.5f;
	SetActorLocation(FVector::ZeroVector);
}

void AProceduralDungeonGenerator::EnsureOddGrid()
{
	GridWidth = FMath::Clamp(GridWidth, 5, 101);
	GridHeight = FMath::Clamp(GridHeight, 5, 101);
	if (GridWidth % 2 == 0)
	{
		++GridWidth;
	}
	if (GridHeight % 2 == 0)
	{
		++GridHeight;
	}
}

void AProceduralDungeonGenerator::CarveMazeDFS(int32 StartX, int32 StartY, FRandomStream& Rand)
{
	const int32 W = GridWidth;
	const int32 H = GridHeight;

	struct FNode
	{
		int32 X = 0;
		int32 Y = 0;
	};
	TArray<FNode> Stack;
	Stack.Reserve(W * H);

	auto Push = [&](int32 X, int32 Y)
	{
		Cells[Y][X] = 0;
		Stack.Push({ X, Y });
	};

	Push(StartX, StartY);

	static const FIntPoint Dirs[4] = {
		FIntPoint(2, 0),
		FIntPoint(-2, 0),
		FIntPoint(0, 2),
		FIntPoint(0, -2)
	};

	while (Stack.Num() > 0)
	{
		const FNode Cur = Stack.Last();
		const int32 X = Cur.X;
		const int32 Y = Cur.Y;

		TArray<int32, TInlineAllocator<4>> Order = { 0, 1, 2, 3 };
		for (int32 A = 0; A < 4; ++A)
		{
			const int32 B = Rand.RandRange(A, 3);
			Order.Swap(A, B);
		}

		bool bAdvanced = false;
		for (int32 Idx : Order)
		{
			const FIntPoint D = Dirs[Idx];
			const int32 NX = X + D.X;
			const int32 NY = Y + D.Y;
			if (NX <= 0 || NX >= W - 1 || NY <= 0 || NY >= H - 1)
			{
				continue;
			}
			if (Cells[NY][NX] == 0)
			{
				continue;
			}

			const int32 WX = X + D.X / 2;
			const int32 WY = Y + D.Y / 2;
			Cells[WY][WX] = 0;
			Push(NX, NY);
			bAdvanced = true;
			break;
		}

		if (!bAdvanced)
		{
			Stack.Pop(false);
		}
	}
}

void AProceduralDungeonGenerator::RebuildInstances()
{
	FloorISM->ClearInstances();
	WallISM->ClearInstances();

	const float InvHundred = 1.f / 100.f;
	const FVector FloorScale(CellSize * InvHundred, CellSize * InvHundred, FloorThickness * InvHundred);
	const FVector WallScale(CellSize * InvHundred, CellSize * InvHundred, WallHeight * InvHundred);

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FVector Center = CellCenterWorld(X, Y);
			if (Cells[Y][X] == 0)
			{
				const FVector Loc(Center.X, Center.Y, GridWorldOrigin.Z);
				FloorISM->AddInstance(FTransform(FRotator::ZeroRotator, Loc, FloorScale));
			}
			else
			{
				const float WallCenterZ = GridWorldOrigin.Z + FloorThickness * 0.5f + WallHeight * 0.5f;
				const FVector Loc(Center.X, Center.Y, WallCenterZ);
				WallISM->AddInstance(FTransform(FRotator::ZeroRotator, Loc, WallScale));
			}
		}
	}
}

void AProceduralDungeonGenerator::GenerateFromSeed(int32 Seed)
{
	EnsureOddGrid();
	const int32 UseSeed = (Seed == 0) ? FMath::Rand() : Seed;
	LastUsedSeed = UseSeed;
	FRandomStream Rand(UseSeed);

	Cells.SetNum(GridHeight);
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		Cells[Y].Init(1, GridWidth);
	}

	CarveMazeDFS(StartCellX, StartCellY, Rand);
	RebuildInstances();
}
