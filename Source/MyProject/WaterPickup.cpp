// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AWaterPickup::AWaterPickup()
{
	ConfigurePickup(EInventoryItemType::Water, 1);
	PickupSphereRadius = 48.f;
	if (PickupVisual)
	{
		PickupVisual->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.55f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/M_Water.M_Water"));
	if (Mat.Succeeded())
	{
		DefaultMaterial = Mat.Object;
	}
}

void AWaterPickup::ApplyDefaultVisual()
{
	if (ApplyDefaultMaterialIfSpecified())
	{
		return;
	}
	if (!PickupVisual)
	{
		return;
	}
	const int32 NumMats = PickupVisual->GetNumMaterials();
	for (int32 i = 0; i < NumMats; ++i)
	{
		if (UMaterialInstanceDynamic* MID = PickupVisual->CreateAndSetMaterialInstanceDynamic(i))
		{
			const FLinearColor Cyan(0.2f, 0.75f, 0.95f, 1.f);
			MID->SetVectorParameterValue(FName(TEXT("Color")), Cyan);
			MID->SetVectorParameterValue(FName(TEXT("BaseColor")), Cyan);
			MID->SetVectorParameterValue(FName(TEXT("EmissiveColor")), Cyan * 2.f);
		}
	}
}
