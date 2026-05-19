// Copyright Epic Games, Inc. All Rights Reserved.

#include "KitchenClusterActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace KitchenEngineAssets
{
	static UStaticMesh* GetCubeMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	static UStaticMesh* GetCylinderMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	static UStaticMesh* GetSphereMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}
	static UMaterialInterface* GetBasicShapeMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	static UStaticMesh* GetSovietDiningTable()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_DiningTable.SM_DiningTable"));
	}
	static UStaticMesh* GetSovietOven()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_Oven.SM_Oven"));
	}
	static UStaticMesh* GetSovietSinkA()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_KitchenUnit_Sink_01a.SM_KitchenUnit_Sink_01a"));
	}
	static UStaticMesh* GetSovietSinkB()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_KitchenUnit_Sink_01b.SM_KitchenUnit_Sink_01b"));
	}
	static UStaticMesh* GetSovietCabinetLargeA()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_KitchenUnit_Large_01a.SM_KitchenUnit_Large_01a"));
	}
	static UStaticMesh* GetSovietCabinetLargeB()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_KitchenUnit_Large_01b.SM_KitchenUnit_Large_01b"));
	}
	static UStaticMesh* GetSovietCabinetSmallA()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_KitchenUnit_Small_01a.SM_KitchenUnit_Small_01a"));
	}
	static UStaticMesh* GetSovietCabinetSmallB()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_KitchenUnit_Small_01b.SM_KitchenUnit_Small_01b"));
	}
	static UStaticMesh* GetSovietShelf()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_KitchenShelf.SM_KitchenShelf"));
	}
	static UStaticMesh* GetSovietCupboard()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_Cupboard_01a.SM_Cupboard_01a"));
	}
	static UStaticMesh* GetSovietMug()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_Mug.SM_Mug"));
	}
	static UStaticMesh* GetSovietBottle()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SovietApartments/Meshes/SM_Bottle.SM_Bottle"));
	}
}

namespace KitchenColors
{
	static const FLinearColor Wood(0.35f, 0.22f, 0.12f);
	static const FLinearColor Chrome(0.72f, 0.74f, 0.76f);
	static const FLinearColor PlasticWhite(0.92f, 0.93f, 0.94f);
	static const FLinearColor PlasticBlack(0.12f, 0.12f, 0.13f);
	static const FLinearColor Burner(0.15f, 0.15f, 0.18f);
}

static UMaterialInstanceDynamic* MakeMid(UObject* Outer, UMaterialInterface* Base, FLinearColor Tint)
{
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
	if (Mid)
	{
		static const FName ColorName(TEXT("Color"));
		Mid->SetVectorParameterValue(ColorName, Tint);
	}
	return Mid;
}

AKitchenClusterActor::AKitchenClusterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AKitchenClusterActor::InitializeCluster(EKitchenClusterKind Kind, FRandomStream& Rand)
{
	// Повторная инициализация (новый раунд без перезагрузки уровня): очищаем динамически добавленные компоненты.
	TArray<UStaticMeshComponent*> ExistingComponents;
	GetComponents<UStaticMeshComponent>(ExistingComponents);
	for (UStaticMeshComponent* MeshComp : ExistingComponents)
	{
		if (!MeshComp || MeshComp->CreationMethod == EComponentCreationMethod::Native)
		{
			continue;
		}
		MeshComp->UnregisterComponent();
		MeshComp->DestroyComponent();
	}
	if (!IsValid(SceneRoot))
	{
		SceneRoot = NewObject<USceneComponent>(this, USceneComponent::StaticClass(), TEXT("SceneRoot_Runtime"), RF_Transient);
		if (SceneRoot)
		{
			SetRootComponent(SceneRoot);
			AddInstanceComponent(SceneRoot);
			SceneRoot->RegisterComponent();
		}
	}

	UStaticMesh* const MCube = KitchenEngineAssets::GetCubeMesh();
	UStaticMesh* const MCyl = KitchenEngineAssets::GetCylinderMesh();
	UStaticMesh* const MSph = KitchenEngineAssets::GetSphereMesh();
	UMaterialInterface* const MBase = KitchenEngineAssets::GetBasicShapeMaterial();
	UStaticMesh* const STable = KitchenEngineAssets::GetSovietDiningTable();
	UStaticMesh* const SOven = KitchenEngineAssets::GetSovietOven();
	UStaticMesh* const SSinkA = KitchenEngineAssets::GetSovietSinkA();
	UStaticMesh* const SSinkB = KitchenEngineAssets::GetSovietSinkB();
	UStaticMesh* const SCabLargeA = KitchenEngineAssets::GetSovietCabinetLargeA();
	UStaticMesh* const SCabLargeB = KitchenEngineAssets::GetSovietCabinetLargeB();
	UStaticMesh* const SCabSmallA = KitchenEngineAssets::GetSovietCabinetSmallA();
	UStaticMesh* const SCabSmallB = KitchenEngineAssets::GetSovietCabinetSmallB();
	UStaticMesh* const SShelf = KitchenEngineAssets::GetSovietShelf();
	UStaticMesh* const SCupboard = KitchenEngineAssets::GetSovietCupboard();
	UStaticMesh* const SMug = KitchenEngineAssets::GetSovietMug();
	UStaticMesh* const SBottle = KitchenEngineAssets::GetSovietBottle();

	const bool bCanUseSovietKitchen = (STable && SOven && (SCabLargeA || SCabSmallA || SCupboard));

	auto AddMesh = [&](const TCHAR* Name, UStaticMesh* M, FVector RelLoc, FRotator Rot, FVector Scale, FLinearColor Tint)
	{
		if (!IsValid(this) || IsActorBeingDestroyed() || !IsValid(M) || !IsValid(SceneRoot))
		{
			return;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), NAME_None, RF_Transient);
		if (!C)
		{
			return;
		}
		C->SetupAttachment(SceneRoot);
		C->SetStaticMesh(M);
		C->SetRelativeLocation(RelLoc);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCollisionObjectType(ECC_WorldStatic);
		C->SetCollisionResponseToAllChannels(ECR_Block);
		AddInstanceComponent(C);
		C->RegisterComponent();
		if (UMaterialInstanceDynamic* Mid = MakeMid(this, MBase, Tint))
		{
			C->SetMaterial(0, Mid);
		}
	};

	auto AddWorldMesh = [&](const TCHAR* Name, UStaticMesh* M, FVector RelLoc, FRotator Rot, FVector Scale)
	{
		if (!IsValid(this) || IsActorBeingDestroyed() || !IsValid(M) || !IsValid(SceneRoot))
		{
			return;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), NAME_None, RF_Transient);
		if (!C)
		{
			return;
		}
		C->SetupAttachment(SceneRoot);
		C->SetStaticMesh(M);
		C->SetRelativeLocation(RelLoc);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCollisionObjectType(ECC_WorldStatic);
		C->SetCollisionResponseToAllChannels(ECR_Block);
		AddInstanceComponent(C);
		C->RegisterComponent();
	};

	if (bCanUseSovietKitchen)
	{
		switch (Kind)
		{
		case EKitchenClusterKind::Table:
			AddWorldMesh(TEXT("SovietTable"), STable, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(1.2f, 1.2f, 1.2f));
			if (Rand.GetFraction() > 0.45f)
			{
				AddWorldMesh(TEXT("SovietMug"), SMug, FVector(20.f, -10.f, 85.f), FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), FVector(1.2f, 1.2f, 1.2f));
			}
			break;
		case EKitchenClusterKind::Stove:
			AddWorldMesh(TEXT("SovietOven"), SOven, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(1.05f, 1.05f, 1.05f));
			if (Rand.GetFraction() > 0.35f)
			{
				AddWorldMesh(TEXT("SovietBottle"), SBottle, FVector(26.f, 8.f, 92.f), FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), FVector(0.8f, 0.8f, 0.8f));
			}
			break;
		case EKitchenClusterKind::Sink:
		{
			UStaticMesh* SinkMesh = (Rand.GetFraction() > 0.5f) ? SSinkA : SSinkB;
			AddWorldMesh(TEXT("SovietSink"), SinkMesh ? SinkMesh : SSinkA, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(1.05f, 1.05f, 1.05f));
			break;
		}
		case EKitchenClusterKind::Cabinets:
		default:
		{
			const float Choice = Rand.GetFraction();
			UStaticMesh* Pick = SCabLargeA;
			if (Choice > 0.75f && SCupboard)
			{
				Pick = SCupboard;
			}
			else if (Choice > 0.5f && SCabSmallA)
			{
				Pick = SCabSmallA;
			}
			else if (Choice > 0.25f && SCabLargeB)
			{
				Pick = SCabLargeB;
			}
			AddWorldMesh(TEXT("SovietCabinet"), Pick, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(1.05f, 1.05f, 1.05f));
			if (SShelf && Rand.GetFraction() > 0.55f)
			{
				AddWorldMesh(TEXT("SovietShelf"), SShelf, FVector(0.f, -70.f, 100.f), FRotator::ZeroRotator, FVector(0.9f, 0.9f, 0.9f));
			}
			break;
		}
		}
		(void)SCabSmallB;
		return;
	}

	// Fallback: старые примитивы, если контент SovietApartments не найден.
	switch (Kind)
	{
	case EKitchenClusterKind::Table:
	{
		const float LegZ = 1.25f;
		const float LegHalf = 50.f * LegZ;
		const float Spread = 70.f;
		const FVector LegSc(0.16f, 0.16f, LegZ);
		AddMesh(TEXT("LegA"), MCyl, FVector(-Spread, -Spread, LegHalf), FRotator::ZeroRotator, LegSc, KitchenColors::Wood);
		AddMesh(TEXT("LegB"), MCyl, FVector(Spread, -Spread, LegHalf), FRotator::ZeroRotator, LegSc, KitchenColors::Wood);
		AddMesh(TEXT("LegC"), MCyl, FVector(-Spread, Spread, LegHalf), FRotator::ZeroRotator, LegSc, KitchenColors::Wood);
		AddMesh(TEXT("LegD"), MCyl, FVector(Spread, Spread, LegHalf), FRotator::ZeroRotator, LegSc, KitchenColors::Wood);
		const float TopZ = LegHalf * 2.f + 5.f;
		AddMesh(TEXT("Top"), MCube, FVector(0.f, 0.f, TopZ), FRotator::ZeroRotator, FVector(1.55f, 1.15f, 0.1f), KitchenColors::Wood);
		break;
	}
	case EKitchenClusterKind::Stove:
		AddMesh(TEXT("Body"), MCube, FVector(0.f, 0.f, 32.f), FRotator::ZeroRotator, FVector(0.95f, 0.8f, 0.22f), KitchenColors::Chrome);
		AddMesh(TEXT("B1"), MSph, FVector(-28.f, -22.f, 68.f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.1f), KitchenColors::Burner);
		AddMesh(TEXT("B2"), MSph, FVector(28.f, -22.f, 68.f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.1f), KitchenColors::Burner);
		AddMesh(TEXT("B3"), MSph, FVector(-28.f, 22.f, 68.f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.1f), KitchenColors::Burner);
		AddMesh(TEXT("B4"), MSph, FVector(28.f, 22.f, 68.f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.1f), KitchenColors::Burner);
		break;
	case EKitchenClusterKind::Sink:
		AddMesh(TEXT("Basin"), MCube, FVector(0.f, 0.f, 38.f), FRotator::ZeroRotator, FVector(0.8f, 0.55f, 0.28f), KitchenColors::Chrome);
		AddMesh(TEXT("Tap"), MCyl, FVector(0.f, -42.f, 92.f), FRotator(18.f, 0.f, 0.f), FVector(0.055f, 0.055f, 0.42f), KitchenColors::Chrome);
		break;
	case EKitchenClusterKind::Cabinets:
	default:
		AddMesh(TEXT("C1"), MCube, FVector(-48.f, 0.f, 68.f), FRotator::ZeroRotator, FVector(0.42f, 0.58f, 0.68f), KitchenColors::PlasticWhite);
		AddMesh(TEXT("C2"), MCube, FVector(48.f, 0.f, 68.f), FRotator::ZeroRotator, FVector(0.42f, 0.58f, 0.68f), KitchenColors::PlasticWhite);
		AddMesh(TEXT("HandleA"), MCube, FVector(-48.f, 38.f, 68.f), FRotator::ZeroRotator, FVector(0.28f, 0.04f, 0.04f), KitchenColors::PlasticBlack);
		AddMesh(TEXT("HandleB"), MCube, FVector(48.f, 38.f, 68.f), FRotator::ZeroRotator, FVector(0.28f, 0.04f, 0.04f), KitchenColors::PlasticBlack);
		break;
	}

	(void)Rand;
}
