// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MyProjectHUD.generated.h"

/** Палитра Canvas-HUD: настраивается в деталях HUD / Blueprint HUD. */
USTRUCT(BlueprintType)
struct FHUDTheme
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FColor HealthBarColor = FColor(64, 200, 110, 255);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FColor StaminaBarColor = FColor(90, 170, 255, 255);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FColor BatteryTextColor = FColor(185, 220, 255, 255);

	/** Подсветка рамки активного слота хотбара. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FLinearColor SlotBorderColor = FLinearColor(0.42f, 0.68f, 0.98f, 0.95f);
};

/**
 * HUD: здоровье, полоска выносливости (демо), крошки, батарейки, хотбар, метрики.
 */
UCLASS()
class MYPROJECT_API AMyProjectHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** Цвета и стиль Canvas-отрисовки (можно переопределить в BP HUD или выставить на экземпляре). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Theme")
	FHUDTheme Theme;

protected:
	virtual void DrawHUD() override;
};
