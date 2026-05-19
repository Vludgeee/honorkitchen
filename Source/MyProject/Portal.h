// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Portal.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UHonorKitchenEnemySpriteComponent;
class AMyProjectCharacter;

UCLASS(Blueprintable)
class MYPROJECT_API APortal : public AActor
{
	GENERATED_BODY()

public:
	APortal();

	bool TryActivate(AMyProjectCharacter* Interactor);

	/** Розетка на стене: плоскость смотрит в комнату, без билборда. */
	void ApplyWallMount(const FVector& WorldLocation, const FVector& NormalIntoRoom);

	/** Вращение: нормаль в комнату, «верх» по миру, roll как у врагов. */
	static FRotator MakeWallMountRotation(const FVector& NormalIntoRoom);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** Fallback, если PNG ещё не импортирован. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> PortalVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UHonorKitchenEnemySpriteComponent> PortalSprite;

	/** Дистанция взаимодействия с порталом по E (см мира); для демо ~200–300. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "100.0"))
	float InteractionRadius = 420.f;

	/** Высота спрайта на стене (uu), как у томатозавра — 340+. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "200.0", ClampMax = "800.0"))
	float PortalSpriteHeightUU = 400.f;

	/** Ширина (uu); 0 = по пропорции PNG. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "0.0", ClampMax = "1200.0"))
	float PortalSpriteWidthUU = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PortalSpriteRollOffset = 0.f;

	/** Высота центра розетки над верхом пола (см). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "20.0", ClampMax = "200.0"))
	float SocketHeightAboveFloorUU = 45.f;
};
