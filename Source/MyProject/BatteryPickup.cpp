// Copyright Epic Games, Inc. All Rights Reserved.

#include "BatteryPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABatteryPickup::ABatteryPickup()
{
	ConfigurePickup(EInventoryItemType::Battery, 1);
	if (PickupVisual)
	{
		PickupVisual->SetRelativeScale3D(FVector(1.65f, 1.65f, 1.35f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/M_Battery.M_Battery"));
	if (Mat.Succeeded())
	{
		DefaultMaterial = Mat.Object;
	}
}

void ABatteryPickup::ApplyDefaultVisual()
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
			const FLinearColor Green(0.15f, 0.85f, 0.25f, 1.f);
			MID->SetVectorParameterValue(FName(TEXT("Color")), Green);
			MID->SetVectorParameterValue(FName(TEXT("BaseColor")), Green);
			MID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), Green);
			MID->SetVectorParameterValue(FName(TEXT("EmissiveColor")), Green * 3.f);
			MID->SetScalarParameterValue(FName(TEXT("Emissive")), 1.2f);
		}
	}
}
