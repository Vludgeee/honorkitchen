// Copyright Epic Games, Inc. All Rights Reserved.

#include "MedkitPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AMedkitPickup::AMedkitPickup()
{
	ConfigurePickup(EInventoryItemType::Medkit, 1);
	if (PickupVisual)
	{
		PickupVisual->SetRelativeScale3D(FVector(1.8f, 1.35f, 1.05f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/M_Medkit.M_Medkit"));
	if (Mat.Succeeded())
	{
		DefaultMaterial = Mat.Object;
	}
}

void AMedkitPickup::ApplyDefaultVisual()
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
			const FLinearColor Red(0.95f, 0.2f, 0.15f, 1.f);
			MID->SetVectorParameterValue(FName(TEXT("Color")), Red);
			MID->SetVectorParameterValue(FName(TEXT("BaseColor")), Red);
			MID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), Red);
			MID->SetVectorParameterValue(FName(TEXT("EmissiveColor")), Red * 2.f);
		}
	}
}
