// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaltPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASaltPickup::ASaltPickup()
{
	ConfigurePickup(EInventoryItemType::Salt, 1);
	PickupSphereRadius = 48.f;
	if (PickupVisual)
	{
		PickupVisual->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.35f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/M_Salt.M_Salt"));
	if (Mat.Succeeded())
	{
		DefaultMaterial = Mat.Object;
	}
}

void ASaltPickup::ApplyDefaultVisual()
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
			const FLinearColor White(0.95f, 0.95f, 0.98f, 1.f);
			MID->SetVectorParameterValue(FName(TEXT("Color")), White);
			MID->SetVectorParameterValue(FName(TEXT("BaseColor")), White);
			MID->SetVectorParameterValue(FName(TEXT("EmissiveColor")), White * 1.5f);
		}
	}
}
