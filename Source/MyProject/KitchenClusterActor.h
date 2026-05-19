// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "KitchenClusterActor.generated.h"

UENUM(BlueprintType)
enum class EKitchenClusterKind : uint8
{
	Table,
	Stove,
	Sink,
	Cabinets,
};

/** Набор примитивов (куб/цилиндр/сфера) под одну «зону» кухни. */
UCLASS()
class MYPROJECT_API AKitchenClusterActor : public AActor
{
	GENERATED_BODY()

public:
	AKitchenClusterActor();

	void InitializeCluster(EKitchenClusterKind Kind, FRandomStream& Rand);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Kitchen")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;
};
