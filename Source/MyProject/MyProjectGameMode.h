// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KaravaychikCharacter.h"
#include "VilokhvostCharacter.h"
#include "MyProjectGameMode.generated.h"

class APlayerController;

UCLASS(minimalapi)
class AMyProjectGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyProjectGameMode();

	/** После смерти: уничтожить пешку и через задержку заново заспавнить игрока у Player Start. */
	void NotifyPlayerDied(APlayerController* PC);
	void NotifyCrumbsCollected(APlayerController* PC, int32 NewTotalCrumbs);
	void NotifyCrumbThrown();
	void NotifyAIDetectedPlayer();
	void NotifyBatteryCollected(int32 Amount = 1);
	bool TryActivatePortal(APlayerController* PC);
	bool IsRoundWon() const { return bRoundWon; }
	int32 GetCrumbsThrownCount() const { return CrumbsThrownCount; }
	int32 GetAIDetectionCount() const { return AIDetectionCount; }
	int32 GetRequiredBatteries() const { return RequiredBatteries; }
	int32 GetCollectedBatteries() const { return CollectedBatteries; }
	float GetRoundElapsedSeconds() const;

protected:
	virtual void BeginPlay() override;

	/** Процедурная расстановка: портал, предметы, позиции врагов на NavMesh. */
	void GenerateProceduralRound();
	void TryGenerateProceduralRoundDeferred();

	/** Отложенный спавн Каравайчика (после появления игрока). */
	void SpawnKaravaychikIfConfigured();

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Karavaychik")
	TSubclassOf<AKaravaychikCharacter> KaravaychikClass;

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Karavaychik", meta = (ClampMin = "0", ClampMax = "8"))
	int32 KaravaychikSpawnCount = 1;

	/** Смещение от позиции игрока при спавне (см). */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Karavaychik")
	FVector KaravaychikSpawnOffsetFromPlayer = FVector(900.f, 350.f, 128.f);

	FTimerHandle KaravaychikSpawnTimerHandle;

	void SpawnVilokhvostIfConfigured();

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Vilokhvost")
	TSubclassOf<AVilokhvostCharacter> VilokhvostClass;

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Vilokhvost", meta = (ClampMin = "0", ClampMax = "8"))
	int32 VilokhvostSpawnCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Vilokhvost")
	FVector VilokhvostSpawnOffsetFromPlayer = FVector(-600.f, -200.f, 180.f);

	FTimerHandle VilokhvostSpawnTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Respawn", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float RespawnDelaySeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Win", meta = (ClampMin = "2", ClampMax = "3"))
	int32 MinRequiredBatteries = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Win", meta = (ClampMin = "2", ClampMax = "3"))
	int32 MaxRequiredBatteries = 3;

	UPROPERTY(VisibleAnywhere, Category = "Win")
	int32 RequiredBatteries = 2;

	UPROPERTY(VisibleAnywhere, Category = "Win")
	int32 CollectedBatteries = 0;

	UPROPERTY(VisibleAnywhere, Category = "Win")
	bool bRoundWon = false;

	UPROPERTY(VisibleAnywhere, Category = "Metrics")
	int32 CrumbsThrownCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Metrics")
	int32 AIDetectionCount = 0;

	double RoundStartTime = 0.0;

	FTimerHandle RespawnTimerHandle;
	FTimerHandle ProceduralInitTimerHandle;

	bool bProceduralRoundGenerated = false;
	int32 ProceduralInitAttempts = 0;
};



