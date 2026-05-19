// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyProjectPlayerController.generated.h"

class UInputMappingContext;
class USoundBase;
class AMyProjectGameMode;

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

	/** Меню / титул: курсор, UI-ввод, чёрный fade камеры. */
	void ApplyFrontEndInputMode(bool bFrontEnd);

protected:

	/** Input Mapping Context to be used for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|UI")
	TObjectPtr<USoundBase> UiClickSound;

	void PlayUiClickSound() const;

	void ProcessSettingsInput(AMyProjectGameMode* GM, int32 ViewSX, int32 ViewSY);

	bool bSettingsDraggingMusic = false;
	bool bSettingsDraggingMonster = false;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
};
