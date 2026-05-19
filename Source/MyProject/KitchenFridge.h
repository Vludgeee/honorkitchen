// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveProp.h"
#include "KitchenFridge.generated.h"

class UStaticMeshComponent;

/** Холодильник: пока декор; взаимодействие — заготовка под будущую логику. */
UCLASS()
class MYPROJECT_API AKitchenFridge : public AInteractiveProp
{
	GENERATED_BODY()

public:
	AKitchenFridge();

	virtual void BeginPlay() override;
	virtual void OnInteract(AActor* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Kitchen")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
};
