// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TomatoSaurusCharacter.h"
#include "KaravaychikCharacter.generated.h"

class USoundBase;

/**
 * Каравайчик (хлеб): узкое зрение, Alert «хруст» при первом контакте с игроком — зовёт союзников.
 * Вода: замедление и блок крика (ФТ-4.2).
 */
UCLASS(Blueprintable, meta = (DisplayName = "Каравайчик (хлеб)"))
class MYPROJECT_API AKaravaychikCharacter : public ATomatoSaurusCharacter
{
	GENERATED_BODY()

public:
	AKaravaychikCharacter();

	/** Вода по ТЗ: 3 с, скорость ×0.3, нельзя кричать (Alert). */
	void ApplyKaravaychikWaterDebuff();

protected:
	virtual void OnPlayerSightGained(AMyProjectCharacter* Player) override;

	/** Радиус привлечения TomatoSaurus и др. врагов (см). */
	UPROPERTY(EditDefaultsOnly, Category = "Karavaychik|Alert")
	float CrunchAlertRadiusUU = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Karavaychik|Alert")
	float CrunchNoiseLoudness = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Karavaychik|Alert")
	float AllyInvestigateDurationSec = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Karavaychik|Audio")
	TObjectPtr<USoundBase> CrunchSound;

	bool bHasPlayedCrunchAlert = false;
	bool bShoutBlockedByWater = false;

	FTimerHandle KaravaychikWaterTimerHandle;

	UFUNCTION()
	void ClearKaravaychikWaterDebuff();

	void BroadcastCrunchAlert();
};
