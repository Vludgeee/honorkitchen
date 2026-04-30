// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyProjectPlayerController.generated.h"

class UInputMappingContext;

/**
 *
 */
UCLASS()
class MYPROJECT_API AMyProjectPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMyProjectPlayerController();

	/** Полная перезагрузка текущего уровня (клавиша R в игре). */
	UFUNCTION(BlueprintCallable, Category = "Game")
	void RestartCurrentLevel();

protected:

	/** Input Mapping Context to be used for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* InputMappingContext;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
};
