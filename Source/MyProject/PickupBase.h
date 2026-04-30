// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryItemTypes.h"
#include "PickupBase.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class AMyProjectCharacter;

/**
 * Базовый подбираемый предмет: общая геометрия, TryCollect, хуки для расширения.
 * Crumb / Battery / Medkit наследуются от этого класса.
 */
UCLASS(Blueprintable)
class MYPROJECT_API APickupBase : public AActor
{
	GENERATED_BODY()

public:
	APickupBase();

	/** Подбор в инвентарь (E). */
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	virtual bool TryCollect(AMyProjectCharacter* Collector);

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void ConfigurePickup(EInventoryItemType NewType, int32 NewAmount);

	EInventoryItemType GetItemType() const { return ItemType; }
	int32 GetAmount() const { return ItemAmount; }

	/**
	 * Использование предмета из хотбара (ПКМ и т.д.) без актора в мире.
	 * Централизованная точка расширения для аптечки, соли и т.д.
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	static bool DispatchHotbarUse(EInventoryItemType Type, AMyProjectCharacter* User);

	/** После спавна на землю из хотбара (Q); Dropper может быть nullptr. */
	void NotifyDroppedFromInventory(AMyProjectCharacter* Dropper) { OnDrop(Dropper); }

protected:
	virtual void BeginPlay() override;

	/** После успешного добавления в хотбар, до Destroy. */
	virtual void OnPickup(AMyProjectCharacter* Collector);

	/** После появления в мире при выбросе из хотбара (Q). */
	virtual void OnDrop(AMyProjectCharacter* Dropper);

	/**
	 * «Использование» предмета как актора в мире (по умолчанию нет).
	 * Для хотбара без актора — см. DispatchHotbarUse.
	 */
	virtual bool OnUse(AMyProjectCharacter* User);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	EInventoryItemType ItemType = EInventoryItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "1"))
	int32 ItemAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Visual")
	TObjectPtr<UMaterialInterface> PickupMaterialOverride;

	/**
	 * Базовый материал слота 0 (назначь в BP или положи M_* в /Game/Materials/).
	 * Если задан — используется вместо раскраски через MID в ApplyDefaultVisual.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Visual")
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	/** Радиус сферы подбора (визуал/позиция). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "10.0"))
	float PickupSphereRadius = 48.f;

	/** Применить цвет/стиль меша (переопределяется в наследниках). */
	virtual void ApplyDefaultVisual();

	/** Если задан DefaultMaterial — ставит его на PickupVisual и возвращает true. */
	bool ApplyDefaultMaterialIfSpecified();
};
