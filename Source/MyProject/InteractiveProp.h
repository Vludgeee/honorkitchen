// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractiveProp.generated.h"

/** Базовый интерактивный предмет (холодильник и т.д.) — расширение через OnInteract. */
UCLASS(Blueprintable)
class MYPROJECT_API AInteractiveProp : public AActor
{
	GENERATED_BODY()

public:
	AInteractiveProp();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Interact")
	virtual void OnInteract(AActor* Interactor);
};
