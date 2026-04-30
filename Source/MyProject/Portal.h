// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Portal.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class AMyProjectCharacter;

UCLASS(Blueprintable)
class MYPROJECT_API APortal : public AActor
{
	GENERATED_BODY()

public:
	APortal();

	bool TryActivate(AMyProjectCharacter* Interactor);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> PortalVisual;

	/** Дистанция взаимодействия с порталом по E (см мира); для демо ~200–300. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "100.0"))
	float InteractionRadius = 220.f;
};

