// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HonorKitchenPortalNavigatorComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class AKitchenGenerator;

/** Тонкая зелёная полоска на полу от игрока к порталу (сетка проходимых клеток). */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UHonorKitchenPortalNavigatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHonorKitchenPortalNavigatorComponent();

	void ToggleNavigator();
	bool IsNavigatorEnabled() const { return bNavigatorEnabled; }
	void SetNavigatorEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(VisibleAnywhere, Category = "Navigator")
	TObjectPtr<UInstancedStaticMeshComponent> StripISM = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StripMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Navigator", meta = (ClampMin = "4.0", ClampMax = "80.0"))
	float StripWidthUU = 14.f;

	UPROPERTY(EditAnywhere, Category = "Navigator", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float StripHeightUU = 4.f;

	UPROPERTY(EditAnywhere, Category = "Navigator", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float PathRefreshIntervalSeconds = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Navigator", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float FloorLiftAboveTilesUU = 3.f;

	bool bNavigatorEnabled = false;
	float PathRefreshTimer = 0.f;

	void RebuildStrip();
	AKitchenGenerator* FindKitchenGenerator() const;
};
