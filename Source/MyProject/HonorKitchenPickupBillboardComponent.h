// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "InventoryItemTypes.h"
#include "HonorKitchenPickupBillboardComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

/**
 * Одна текстура, плоскость к камере (как спрайты врагов). Для пикапов Crumb / Battery / Salt / Water.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UHonorKitchenPickupBillboardComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHonorKitchenPickupBillboardComponent();

	/** Размер по большей стороне текстуры в uu. */
	UPROPERTY(EditAnywhere, Category = "Pickup Sprite", meta = (ClampMin = "16.0", ClampMax = "512.0"))
	float SpriteWorldSizeUU = 76.f;

	bool ActivateSpriteIfAvailable(EInventoryItemType Type);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SpritePlane;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpriteMID;

	EInventoryItemType ActiveType = EInventoryItemType::None;
	bool bSpriteActive = false;

	void EnsureSpritePlane();
	bool EnsureMaterial(UTexture2D* Tex);
	void FaceViewerCamera();
	void UpdatePlaneScale(UTexture2D* Tex);
};
