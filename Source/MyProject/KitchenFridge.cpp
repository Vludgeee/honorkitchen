// Copyright Epic Games, Inc. All Rights Reserved.

#include "KitchenFridge.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AKitchenFridge::AKitchenFridge()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FridgeMesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SovietFridge(TEXT("/Game/SovietApartments/Meshes/SM_Fridge.SM_Fridge"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SovietFridge.Succeeded())
	{
		Mesh->SetStaticMesh(SovietFridge.Object);
		Mesh->SetRelativeScale3D(FVector(1.05f, 1.05f, 1.05f));
	}
	else if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeScale3D(FVector(0.55f, 0.5f, 1.6f));
	}
	if (!SovietFridge.Succeeded() && Mat.Succeeded())
	{
		// MID только в BeginPlay — в конструкторе (в т.ч. при SpawnActor) Create() даёт fatal про default subobjects.
		Mesh->SetMaterial(0, Mat.Object);
	}
}

void AKitchenFridge::BeginPlay()
{
	Super::BeginPlay();
	if (Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cube")))
	{
		if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.78f, 0.82f));
		}
	}
}

void AKitchenFridge::OnInteract(AActor* Interactor)
{
	Super::OnInteract(Interactor);
}
