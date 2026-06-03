// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KaravaychikCharacter.h"
#include "VilokhvostCharacter.h"
#include "HonorKitchenSaveGame.h"
#include "KitchenGenerator.h"
#include "ProceduralDungeonGenerator.h"
#include "Containers/Ticker.h"
#include "MyProjectGameMode.generated.h"

class APlayerController;
class UAudioComponent;
class USoundBase;
class UHonorKitchenSaveGame;
class UHonorKitchenFrontEndMedia;
class UMediaTexture;

UENUM(BlueprintType)
enum class EHonorKitchenBootFlow : uint8
{
	IntroVideo,
	LoadingVideo,
	MainMenu
};

UENUM(BlueprintType)
enum class EFrontEndScreen : uint8
{
	None,
	MainMenu,
	Credits,
	Settings
};

UENUM()
enum class EPreRoundState : uint8
{
	Preparing,
	WaitingPlayerReady,
	ReadyToStart,
	Failed,
	InRound
};

UCLASS(minimalapi)
class AMyProjectGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyProjectGameMode();

	/** После смерти: freeze + death sting, затем экран поражения. */
	void NotifyPlayerDied(APlayerController* PC);
	bool IsDeathStingActive() const { return bDeathStingActive; }
	/** Пауза / победа / поражение / death-sting — мир на TimeDilation=0. */
	void RefreshWorldTimeFreeze();
	void NotifyCrumbThrown();
	void NotifyAIDetectedPlayer();
	void NotifyBatteryCollected(int32 Amount = 1);
	bool TryActivatePortal(APlayerController* PC);
	bool IsRoundWon() const { return bRoundWon; }
	bool IsRoundLost() const { return bRoundLost; }
	int32 GetCrumbsThrownCount() const { return CrumbsThrownCount; }
	int32 GetAIDetectionCount() const { return AIDetectionCount; }
	int32 GetRequiredBatteries() const { return RequiredBatteries; }
	int32 GetCollectedBatteries() const { return CollectedBatteries; }
	float GetRoundElapsedSeconds() const;
	float GetLastPreparationSeconds() const { return LastPreparationSeconds; }
	int32 GetLastPreparationAttempts() const { return LastPreparationAttempts; }
	bool WasLastPreparationSuccessful() const { return bLastPreparationSucceeded; }
	FString GetLastPreparationFailReason() const { return LastPreparationFailReason; }
	EPreRoundState GetPreRoundState() const { return PreRoundState; }
	void HandlePreRoundEnterPressed();
	void RequestNewRound();

	/** Стартовое меню (до генерации кухни). */
	EFrontEndScreen GetFrontEndScreen() const { return FrontEndScreen; }
	bool HasSaveGameOnDisk() const;
	void StartNewGameFromMenu();
	void LoadGameFromMenu();
	void OpenCreditsFromMenu();
	void CloseCreditsToMainMenu();
	void OpenSettingsFromMenu();
	void OpenSettingsFromCredits();
	void OpenSettingsFromPause();
	void CloseSettings();
	bool IsSettingsOpen() const;
	void QuitFromMenu();
	void ReturnToMainMenu();
	bool TryWriteSaveGame();

	void ApplyFrontEndPresentation(bool bActive);

	bool IsGameSoundEnabled() const;
	void ToggleMenuSound();
	void OnPlayerSettingsChanged();

	AKitchenGenerator* GetKitchenGenerator() const { return SpawnedKitchenGenerator; }

	EHonorKitchenBootFlow GetBootFlow() const { return BootFlow; }
	bool IsFrontEndVideoBlockingUI() const;
	UMediaTexture* GetFrontEndFullscreenVideoTexture() const;
	UMediaTexture* GetMainMenuBackgroundVideoTexture() const;
	bool ShouldDrawMainMenuBackgroundVideo() const;

protected:
	virtual void BeginPlay() override;

	/** Процедурная расстановка: портал, предметы, позиции врагов на NavMesh. */
	void GenerateProceduralRound();
	void TryGenerateProceduralRoundDeferred();
	void TryKitchenFinalizeDeferred();

	/** Отложенный спавн Каравайчика (после появления игрока). */
	void SpawnKaravaychikIfConfigured();
	void SpawnTomatoesIfConfigured();

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Karavaychik")
	TSubclassOf<AKaravaychikCharacter> KaravaychikClass;

	/** Доля Каравайчиков от общего числа остальных врагов (Томатозавры + Вилохвосты). */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Karavaychik", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KaravaychikPopulationRatio = 0.35f;

	/** Смещение от позиции игрока при спавне (см). */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Karavaychik")
	FVector KaravaychikSpawnOffsetFromPlayer = FVector(900.f, 350.f, 128.f);

	FTimerHandle KaravaychikSpawnTimerHandle;

	void SpawnVilokhvostIfConfigured();

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Vilokhvost")
	TSubclassOf<AVilokhvostCharacter> VilokhvostClass;

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Vilokhvost", meta = (ClampMin = "0", ClampMax = "8"))
	int32 VilokhvostSpawnCount = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Vilokhvost")
	FVector VilokhvostSpawnOffsetFromPlayer = FVector(-600.f, -200.f, 180.f);

	FTimerHandle VilokhvostSpawnTimerHandle;

	/** Сколько томатозавров после GenerateProceduralRound (лабиринт без кухни). Редакторные на карте удаляются в BeginPlay. */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Tomato", meta = (ClampMin = "0", ClampMax = "6"))
	int32 DungeonTomatoSpawnCount = 2;

	/** Сколько томатозавров спавнить в режиме кухонной генерации. */
	UPROPERTY(EditDefaultsOnly, Category = "Spawn|Tomato", meta = (ClampMin = "0", ClampMax = "6"))
	int32 KitchenTomatoSpawnCount = 3;

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

	UPROPERTY(VisibleAnywhere, Category = "Win")
	bool bRoundLost = false;

	UPROPERTY(VisibleAnywhere, Category = "Win")
	bool bDeathStingActive = false;

	UPROPERTY(VisibleAnywhere, Category = "Metrics")
	int32 CrumbsThrownCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Metrics")
	int32 AIDetectionCount = 0;

	double RoundStartTime = 0.0;
	double PreparationStartTime = 0.0;
	float LastPreparationSeconds = 0.f;
	int32 LastPreparationAttempts = 0;
	bool bLastPreparationSucceeded = false;
	FString LastPreparationFailReason;

	FTimerHandle RespawnTimerHandle;
	FTimerHandle ProceduralInitTimerHandle;
	FTimerHandle WorldRestoreTimerHandle;
	FTimerHandle DungeonBootstrapTimerHandle;
	FTimerHandle TomatoSpawnTimerHandle;
	FTimerHandle PlayerReadyRetryTimerHandle;

	bool bProceduralRoundGenerated = false;
	int32 ProceduralInitAttempts = 0;

	/**
	 * Сеточная кухня vs лабиринт: при старте используется ровно одна ветка.
	 * Если в BP случайно включены оба флага, приоритет у кухни (лабиринт не стартует).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Kitchen")
	bool bUseKitchenGridGenerator = false;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Kitchen")
	TSubclassOf<AKitchenGenerator> KitchenGeneratorClass;

	/** Лабиринт (DFS). Игнорируется, пока включена кухня и задан KitchenGeneratorClass. */
	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Dungeon")
	bool bGenerateProceduralDungeonGeometry = true;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Dungeon")
	TSubclassOf<AProceduralDungeonGenerator> DungeonGeneratorClass;

	/** Сид лабиринта; 0 — случайный. */
	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Dungeon")
	int32 DungeonMapSeed = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Kitchen", meta = (ClampMin = "0.2", ClampMax = "5.0"))
	float ProceduralNavRebuildWaitSeconds = 0.85f;

	/** 0 — случайный сид каждый запуск (кухня). */
	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Kitchen")
	int32 KitchenMapSeed = 0;

	UPROPERTY()
	TObjectPtr<AKitchenGenerator> SpawnedKitchenGenerator;

	UPROPERTY()
	TObjectPtr<AProceduralDungeonGenerator> SpawnedDungeonGenerator;

	void SpawnKitchenRebuildNavThenBootstrap();
	void SpawnDungeonRebuildNavThenBootstrap();
	void FinishGameplayBootstrap();
	void StartPreparedRound();
	void BeginHiddenPreparation();
	void ContinuePreparationWhenPlayerReady();
	bool EnsurePlayerPawnReady(bool bForceRespawnIfMissing);
	void ApplyPreRoundPlayerLock(bool bLock);
	bool ValidatePreparedGeneration() const;
	void RetryPreparation();

	UPROPERTY(EditDefaultsOnly, Category = "Procedural|Flow", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxPreparationAttempts = 4;

	EPreRoundState PreRoundState = EPreRoundState::Preparing;
	int32 CurrentPreparationAttempt = 0;
	int32 PlayerReadyAttempts = 0;

	bool bPreRoundInputLocked = false;

	UPROPERTY(VisibleAnywhere, Category = "FrontEnd")
	EFrontEndScreen FrontEndScreen = EFrontEndScreen::MainMenu;

	UPROPERTY(VisibleAnywhere, Category = "FrontEnd")
	EFrontEndScreen FrontEndScreenBeforeSettings = EFrontEndScreen::MainMenu;

	UPROPERTY(VisibleAnywhere, Category = "FrontEnd")
	bool bPauseSettingsOpen = false;

	bool bPendingApplyLoadedSave = false;
	bool bPendingApplyWorldSnapshot = false;
	float PendingLoadedHealth = 100.f;
	float PendingLoadedMaxHealth = 100.f;
	FVector PendingPlayerLocation = FVector::ZeroVector;
	FRotator PendingPlayerRotation = FRotator::ZeroRotator;
	TArray<FHonorKitchenSavedHotbarSlot> PendingLoadedHotbar;
	TArray<FHonorKitchenSavedActorRecord> PendingWorldActors;

	void ApplyPendingLoadedSaveToPlayer();
	void CaptureWorldToSave(UHonorKitchenSaveGame* Save) const;
	void RestoreWorldFromPendingSave();
	void FinishWorldSpawnPipelineForLoad();
	void SetLevelTemplateHiddenForFrontEnd(bool bHide);
	void RefreshFrontEndMusic();
	USoundBase* ResolveMenuMusic() const;

	bool ShouldPlayKitchenReadyAtmosphere() const;
	void RefreshKitchenReadyAtmosphere();
	void StopKitchenReadyAtmosphere();
	void StartInRoundAtmosphereTimer();
	void StopInRoundAtmosphereTimer();
	void PlayRandomAtmosphereSting();
	bool CanPlayInRoundAtmosphere() const;

	bool ShouldPlayRoundAmbient() const;
	void StartRoundAmbientLoop();
	void StopRoundAmbientLoop();
	void RefreshRoundAmbientPlayback();

	UFUNCTION()
	void OnInRoundAtmosphereTimer();

	UFUNCTION()
	void OnKitchenReadyStingFinished();

	UPROPERTY(EditDefaultsOnly, Category = "FrontEnd|Audio")
	TSoftObjectPtr<USoundBase> MenuMusicAsset;

	UPROPERTY(EditDefaultsOnly, Category = "FrontEnd|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MenuMusicVolume = 0.45f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MenuMusicComponent;

	/** KitchenReady_Master — только экран «Кухня готова / Enter». */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> KitchenReadyAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AtmosphereStingAudioComponent;

	/** Тихий фоновый loop (sound_19805) после KitchenReady. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RoundAmbientAudioComponent;

	FTimerHandle InRoundAtmosphereTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Atmosphere|Audio", meta = (ClampMin = "30.0", ClampMax = "300.0"))
	float AtmosphereStingIntervalSeconds = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Atmosphere|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KitchenReadyVolume = 0.72f;

	UPROPERTY(EditDefaultsOnly, Category = "Atmosphere|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AtmosphereStingVolume = 0.58f;

	UPROPERTY(EditDefaultsOnly, Category = "Atmosphere|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoundAmbientVolume = 0.22f;

	TArray<TWeakObjectPtr<AActor>> LevelActorsHiddenForFrontEnd;

	UPROPERTY(Transient)
	TObjectPtr<UHonorKitchenFrontEndMedia> FrontEndMedia;

	EHonorKitchenBootFlow BootFlow = EHonorKitchenBootFlow::IntroVideo;
	bool bKitchenPrepComplete = false;
	bool bLoadingVideoTargetsMainMenu = true;

	void StartInitialBootSequence();
	void BeginLoadingVideoSequence(bool bTargetMainMenuAfterComplete);
	void OnIntroVideoFinished();
	void OnLoadingVideoPlaybackEnded();
	void OnKitchenPreparationComplete();
	void ShowMainMenuAfterBoot();
	void StopMainMenuBackgroundVideo();
	void RestartLoadingVideoFromBeginning();

	bool ShouldPlayFrontEndMenuMusic() const;

	void BeginPlayerDeathSting(APlayerController* PC);
	void FinishPlayerDeathSting();
	void CancelDeathStingIfActive();
	bool ShouldFreezeWorld() const;
	bool IsLocalGameplayPaused() const;
	void StopGameplayAudioForDeathSting();
	UFUNCTION()
	void OnDeathStingAudioFinished();

	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> DeathStingAudioComponent;

	FTSTicker::FDelegateHandle DeathStingRealtimeTickerHandle;
	float DeathStingRealtimeElapsed = 0.f;
	TWeakObjectPtr<APlayerController> DeathStingPlayerController;

	bool bWorldFrozenForMenus = false;
	float SavedTimeDilationBeforeWorldFreeze = 1.f;
};



