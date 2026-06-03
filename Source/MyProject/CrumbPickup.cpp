// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrumbPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ACrumbPickup::ACrumbPickup()
{
	ConfigurePickup(EInventoryItemType::Crumb, 1);
	if (PickupVisual)
	{
		PickupVisual->SetRelativeScale3D(FVector(1.95f, 1.95f, 1.2f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CrumbMat(TEXT("/Game/Materials/M_Crumb.M_Crumb"));
	if (CrumbMat.Succeeded())
	{
		DefaultMaterial = CrumbMat.Object;
	}
}

void ACrumbPickup::ApplyDefaultVisual()
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
			const FLinearColor Yellow(1.f, 0.88f, 0.08f, 1.f);
			MID->SetVectorParameterValue(FName(TEXT("Color")), Yellow);
			MID->SetVectorParameterValue(FName(TEXT("BaseColor")), Yellow);
			MID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), Yellow);
			MID->SetVectorParameterValue(FName(TEXT("EmissiveColor")), Yellow * 4.f);
			MID->SetScalarParameterValue(FName(TEXT("Emissive")), 1.5f);
			MID->SetScalarParameterValue(FName(TEXT("EmissiveStrength")), 1.5f);
		}
	}
}
