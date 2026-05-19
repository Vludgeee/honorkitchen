// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HonorKitchenEnemySpriteFrames.h"
#include "HonorKitchenEnemySpriteComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * 2D-спрайт врага (GMod-style): плоскость к камере, front/back / chase / attack.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UHonorKitchenEnemySpriteComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHonorKitchenEnemySpriteComponent();

	void SetSpriteFrames(const FHonorKitchenEnemySpriteFrames& InFrames);
	void SetLegacyVisualToHide(USceneComponent* InLegacyVisual);
	void SetLockedSpriteOrientation(const FRotator& WorldRotation);
	/** Прямой размер плоскости в uu (0 = считать из SpriteWorldSizeUU). */
	void SetExplicitWorldPlaneSize(float WidthUU, float HeightUU);
	void RefreshSpriteVisual();
	bool IsSpriteActive() const { return bSpriteActive; }

	/** Ширина/высота спрайта в см (плоскость 100x100 uu по умолчанию). */
	UPROPERTY(EditAnywhere, Category = "Sprite", meta = (ClampMin = "8.0", ClampMax = "512.0"))
	float SpriteWorldSizeUU = 300.f;

	/** Плоскость целиком к камере (Вилохвост на высоте). Иначе — только yaw. */
	UPROPERTY(EditAnywhere, Category = "Sprite")
	bool bBillboardFaceCamera = true;

	/** Доп. yaw после билборда. */
	UPROPERTY(EditAnywhere, Category = "Sprite")
	float SpriteFacingYawOffset = 0.f;

	/** Доп. roll (Томатозавр/Каравайчик: 180 — переворот «вверх ногами»). */
	UPROPERTY(EditAnywhere, Category = "Sprite")
	float SpriteFacingRollOffset = 0.f;

	/** Отразить по горизонтали (масштаб X). */
	UPROPERTY(EditAnywhere, Category = "Sprite")
	bool bFlipSpriteHorizontal = false;

	/** Отразить по вертикали (масштаб Y, Вилохвост). */
	UPROPERTY(EditAnywhere, Category = "Sprite")
	bool bFlipSpriteVertical = false;

	/** Доп. масштаб для Chase/Attack (если PNG attack крупнее по холсту, чем front). */
	UPROPERTY(EditAnywhere, Category = "Sprite", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float AttackChaseViewScaleMultiplier = 1.f;

	/** SpriteWorldSizeUU задаёт высоту спрайта (для широких PNG, напр. портал). */
	UPROPERTY(EditAnywhere, Category = "Sprite")
	bool bScaleSpriteByHeight = false;

	/** Есть ли хотя бы одна реально загруженная текстура. */
	bool HasLoadedSpriteTextures() const;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void PurgeLegacySpritePlaneChildren();

	UPROPERTY(EditAnywhere, Category = "Sprite")
	FHonorKitchenEnemySpriteFrames SpriteFrames;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SpritePlane;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpriteMID;

	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> LegacyVisual;

	EHonorKitchenEnemySpriteView PickView(const AActor* OwnerActor, const APawn* ViewerPawn) const;
	void ApplyView(EHonorKitchenEnemySpriteView View);
	void FaceViewerCamera();
	bool EnsureSpriteMaterial();
	void EnsureSpritePlane();
	void UpdatePlaneScaleForTexture(const class UTexture2D* Tex, EHonorKitchenEnemySpriteView View);
	void SetLegacyVisualVisible(bool bShowLegacyMesh);

	EHonorKitchenEnemySpriteView ActiveView = EHonorKitchenEnemySpriteView::Front;
	bool bSpriteActive = false;
	bool bLockSpriteOrientation = false;
	FRotator LockedSpriteRotation = FRotator::ZeroRotator;
	float ExplicitPlaneWidthUU = 0.f;
	float ExplicitPlaneHeightUU = 0.f;
	int32 SpriteActivateRetries = 0;
	static constexpr int32 MaxSpriteActivateRetries = 60;
};
