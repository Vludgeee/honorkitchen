// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectGameMode.h"
#include "MyProjectCharacter.h"
#include "MyProjectHUD.h"
#include "MyProjectPlayerController.h"
#include "KaravaychikCharacter.h"
#include "VilokhvostCharacter.h"
#include "TomatoSaurusCharacter.h"
#include "Portal.h"
#include "Components/CapsuleComponent.h"
#include "KitchenGenerator.h"
#include "ProceduralDungeonGenerator.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "PickupBase.h"
#include "BatteryPickup.h"
#include "MedkitPickup.h"
#include "SaltPickup.h"
#include "WaterPickup.h"
#include "CrumbPickup.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "HonorKitchenSaveGame.h"
#include "HonorKitchenFrontEndMedia.h"
#include "MediaTexture.h"
#include "Engine/Texture.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Portal.h"
#include "CrumbProjectile.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenAtmosphereAudio.h"
#include "HonorKitchenAudioDefaults.h"
#include "HonorKitchenDevDebug.h"
#include "UObject/UObjectIterator.h"
#include "Containers/Ticker.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

namespace
{
	APlayerStart* FindFirstPlayerStartInWorld(UWorld* W)
	{
		if (!W)
		{
			return nullptr;
		}
		for (TActorIterator<APlayerStart> It(W); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	void DestroyAllActorsOfClassInWorld(UWorld* W, UClass* Cls)
	{
		if (!W || !Cls)
		{
			return;
		}
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			AActor* A = *It;
			if (A)
			{
				if (A->IsA(Cls))
				{
					A->Destroy();
				}
			}
		}
	}
}

AMyProjectGameMode::AMyProjectGameMode()
	: Super()
{
	KaravaychikClass = AKaravaychikCharacter::StaticClass();
	VilokhvostClass = AVilokhvostCharacter::StaticClass();
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	if (PlayerPawnClassFinder.Succeeded())
	{
		DefaultPawnClass = PlayerPawnClassFinder.Class;
	}
	else
	{
		DefaultPawnClass = AMyProjectCharacter::StaticClass();
	}

	PlayerControllerClass = AMyProjectPlayerController::StaticClass();
	HUDClass = AMyProjectHUD::StaticClass();
	KitchenGeneratorClass = AKitchenGenerator::StaticClass();
	DungeonGeneratorClass = AProceduralDungeonGenerator::StaticClass();

	MenuMusicAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Menu/Clarinet.Clarinet")));
}

void AMyProjectGameMode::BeginPlay()
{
	HonorKitchenAudioSettings::Load();
	Super::BeginPlay();
	MinRequiredBatteries = FMath::Clamp(MinRequiredBatteries, 2, 3);
	MaxRequiredBatteries = FMath::Clamp(MaxRequiredBatteries, 2, 3);
	if (MinRequiredBatteries > MaxRequiredBatteries)
	{
		Swap(MinRequiredBatteries, MaxRequiredBatteries);
	}
	RequiredBatteries = FMath::RandRange(MinRequiredBatteries, MaxRequiredBatteries);
	CollectedBatteries = 0;
	bRoundWon = false;
	bRoundLost = false;
	CrumbsThrownCount = 0;
	AIDetectionCount = 0;
	RoundStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (UWorld* W = GetWorld())
	{
		// Убрать мелькание: враги с карты не показываются до процедурной расстановки / спавна после генерации.
		DestroyAllActorsOfClassInWorld(W, ATomatoSaurusCharacter::StaticClass());
		DestroyAllActorsOfClassInWorld(W, AKaravaychikCharacter::StaticClass());
		DestroyAllActorsOfClassInWorld(W, AVilokhvostCharacter::StaticClass());

		PreRoundState = EPreRoundState::Preparing;
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			EnsurePlayerPawnReady(false);
		}
		StartInitialBootSequence();
	}
}

void AMyProjectGameMode::BeginHiddenPreparation()
{
	StopKitchenReadyAtmosphere();
	StopInRoundAtmosphereTimer();
	StopRoundAmbientLoop();
	bKitchenPrepComplete = false;
	bPreRoundInputLocked = false;
	PreRoundState = EPreRoundState::Preparing;
	CurrentPreparationAttempt = 0;
	bProceduralRoundGenerated = false;
	ProceduralInitAttempts = 0;
	PlayerReadyAttempts = 0;
	PreparationStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	LastPreparationAttempts = 0;
	bLastPreparationSucceeded = false;
	LastPreparationFailReason = TEXT("None");
	UE_LOG(LogTemp, Log, TEXT("[Prep] Start: kitchen=%d dungeon=%d"), bUseKitchenGridGenerator ? 1 : 0, bGenerateProceduralDungeonGeometry ? 1 : 0);
	GetWorldTimerManager().ClearTimer(PlayerReadyRetryTimerHandle);
	ContinuePreparationWhenPlayerReady();
}

void AMyProjectGameMode::ContinuePreparationWhenPlayerReady()
{
	if (!EnsurePlayerPawnReady(true))
	{
		PreRoundState = EPreRoundState::WaitingPlayerReady;
		++PlayerReadyAttempts;
		if (PlayerReadyAttempts > 50)
		{
			LastPreparationAttempts = PlayerReadyAttempts;
			LastPreparationSeconds = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - PreparationStartTime) : 0.f;
			bLastPreparationSucceeded = false;
			LastPreparationFailReason = TEXT("PlayerNotReadyTimeout");
			UE_LOG(LogTemp, Error, TEXT("[Prep] FAIL reason=%s attempts=%d time=%.2fs"),
				*LastPreparationFailReason, LastPreparationAttempts, LastPreparationSeconds);
			PreRoundState = EPreRoundState::Failed;
			OnKitchenPreparationComplete();
			return;
		}
		GetWorldTimerManager().SetTimer(PlayerReadyRetryTimerHandle, this, &AMyProjectGameMode::ContinuePreparationWhenPlayerReady, 0.2f, false);
		return;
	}

	PreRoundState = EPreRoundState::Preparing;
	ApplyPreRoundPlayerLock(true);

	UWorld* W = GetWorld();
	if (!W)
	{
		PreRoundState = EPreRoundState::Failed;
		LastPreparationFailReason = TEXT("NoWorldBeforeBootstrap");
		UE_LOG(LogTemp, Error, TEXT("[Prep] FAIL reason=%s"), *LastPreparationFailReason);
		OnKitchenPreparationComplete();
		return;
	}
	const bool bKitchenPath = bUseKitchenGridGenerator && KitchenGeneratorClass != nullptr;
	const bool bDungeonPath = !bKitchenPath && bGenerateProceduralDungeonGeometry && DungeonGeneratorClass != nullptr;

	if (bKitchenPath)
	{
		W->GetTimerManager().SetTimer(DungeonBootstrapTimerHandle, this, &AMyProjectGameMode::SpawnKitchenRebuildNavThenBootstrap, 0.05f, false);
	}
	else if (bDungeonPath)
	{
		W->GetTimerManager().SetTimer(DungeonBootstrapTimerHandle, this, &AMyProjectGameMode::SpawnDungeonRebuildNavThenBootstrap, 0.05f, false);
	}
	else
	{
		LastPreparationAttempts = CurrentPreparationAttempt + 1;
		LastPreparationSeconds = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - PreparationStartTime) : 0.f;
		bLastPreparationSucceeded = false;
		LastPreparationFailReason = TEXT("NoGeneratorPathConfigured");
		UE_LOG(LogTemp, Error, TEXT("[Prep] FAIL reason=%s attempts=%d time=%.2fs"),
			*LastPreparationFailReason, LastPreparationAttempts, LastPreparationSeconds);
		PreRoundState = EPreRoundState::Failed;
		OnKitchenPreparationComplete();
	}
}

bool AMyProjectGameMode::EnsurePlayerPawnReady(bool bForceRespawnIfMissing)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return false;
	}
	APlayerController* PC = W->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn && bForceRespawnIfMissing)
	{
		if (AActor* StartSpot = FindFirstPlayerStartInWorld(W))
		{
			RestartPlayerAtPlayerStart(PC, StartSpot);
		}
		else
		{
			RestartPlayer(PC);
		}
		Pawn = PC->GetPawn();
	}
	if (!Pawn && bForceRespawnIfMissing && DefaultPawnClass)
	{
		const APlayerStart* PS = FindFirstPlayerStartInWorld(W);
		const FTransform SpawnTransform = PS ? PS->GetActorTransform() : FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 300.f));
		if (APawn* SpawnedPawn = SpawnDefaultPawnAtTransform(PC, SpawnTransform))
		{
			PC->Possess(SpawnedPawn);
			Pawn = SpawnedPawn;
		}
	}
	if (!Pawn && bForceRespawnIfMissing && DefaultPawnClass)
	{
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector FallbackLoc(0.f, 0.f, 300.f);
		if (APawn* SpawnedPawn = W->SpawnActor<APawn>(DefaultPawnClass, FallbackLoc, FRotator::ZeroRotator, Sp))
		{
			PC->Possess(SpawnedPawn);
			Pawn = SpawnedPawn;
		}
	}
	if (!Pawn)
	{
		return false;
	}

	if (PC->GetPawn() != Pawn)
	{
		PC->Possess(Pawn);
	}

	PC->SetViewTarget(Pawn);
	if (!bPreRoundInputLocked)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		PC->SetCinematicMode(false, false, false, true, true);
	}
	return PC->GetPawn() != nullptr;
}

void AMyProjectGameMode::SpawnKitchenRebuildNavThenBootstrap()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		LastPreparationAttempts = CurrentPreparationAttempt + 1;
		LastPreparationSeconds = 0.f;
		bLastPreparationSucceeded = false;
		LastPreparationFailReason = TEXT("NoWorldKitchenBootstrap");
		UE_LOG(LogTemp, Error, TEXT("[Prep] FAIL reason=%s"), *LastPreparationFailReason);
		PreRoundState = EPreRoundState::Failed;
		OnKitchenPreparationComplete();
		return;
	}

	for (; CurrentPreparationAttempt < MaxPreparationAttempts; ++CurrentPreparationAttempt)
	{
		{
			for (TActorIterator<AKitchenGenerator> It(W); It; ++It)
			{
				if (AKitchenGenerator* A = *It)
				{
					A->Destroy();
				}
			}
			SpawnedKitchenGenerator = nullptr;
		}

		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AKitchenGenerator* Gen = W->SpawnActor<AKitchenGenerator>(
			KitchenGeneratorClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Sp);

		if (!Gen)
		{
			continue;
		}

		SpawnedKitchenGenerator = Gen;
		Gen->Regenerate(KitchenMapSeed, RequiredBatteries);
		if (ValidatePreparedGeneration())
		{
			W->GetTimerManager().SetTimer(
				DungeonBootstrapTimerHandle,
				this,
				&AMyProjectGameMode::FinishGameplayBootstrap,
				ProceduralNavRebuildWaitSeconds,
				false);
			return;
		}
	}

	PreRoundState = EPreRoundState::Failed;
	LastPreparationAttempts = CurrentPreparationAttempt;
	LastPreparationSeconds = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - PreparationStartTime) : 0.f;
	bLastPreparationSucceeded = false;
	LastPreparationFailReason = TEXT("KitchenValidationFailed");
	UE_LOG(LogTemp, Error, TEXT("[Prep] FAIL reason=%s attempts=%d time=%.2fs"),
		*LastPreparationFailReason, LastPreparationAttempts, LastPreparationSeconds);
	OnKitchenPreparationComplete();
}

void AMyProjectGameMode::SpawnDungeonRebuildNavThenBootstrap()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		LastPreparationAttempts = CurrentPreparationAttempt + 1;
		LastPreparationSeconds = 0.f;
		bLastPreparationSucceeded = false;
		LastPreparationFailReason = TEXT("NoWorldDungeonBootstrap");
		UE_LOG(LogTemp, Error, TEXT("[Prep] FAIL reason=%s"), *LastPreparationFailReason);
		FinishGameplayBootstrap();
		return;
	}

	{
		for (TActorIterator<AProceduralDungeonGenerator> It(W); It; ++It)
		{
			if (AProceduralDungeonGenerator* A = *It)
			{
				A->Destroy();
			}
		}
		SpawnedDungeonGenerator = nullptr;
	}

	APlayerStart* FoundStart = nullptr;
	for (TActorIterator<APlayerStart> It(W); It; ++It)
	{
		FoundStart = *It;
		break;
	}

	const FVector StartLoc = FoundStart ? FoundStart->GetActorLocation() : FVector::ZeroVector;

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AProceduralDungeonGenerator* Gen = W->SpawnActor<AProceduralDungeonGenerator>(
		DungeonGeneratorClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		Sp);

	if (!Gen)
	{
		LastPreparationAttempts = CurrentPreparationAttempt + 1;
		LastPreparationSeconds = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - PreparationStartTime) : 0.f;
		bLastPreparationSucceeded = false;
		LastPreparationFailReason = TEXT("DungeonGeneratorSpawnFailed");
		UE_LOG(LogTemp, Error, TEXT("[Prep] FAIL reason=%s attempts=%d time=%.2fs"),
			*LastPreparationFailReason, LastPreparationAttempts, LastPreparationSeconds);
		PreRoundState = EPreRoundState::Failed;
		OnKitchenPreparationComplete();
		return;
	}

	SpawnedDungeonGenerator = Gen;
	Gen->StripTemplateStaticMeshesIfConfigured();
	Gen->AlignStartCellToWorldLocation(StartLoc);
	Gen->GenerateFromSeed(DungeonMapSeed);

	UNavigationSystemV1::UpdateActorAndComponentsInNavOctree(*Gen);
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
	{
		NavSys->Build();
	}

	W->GetTimerManager().SetTimer(
		DungeonBootstrapTimerHandle,
		this,
		&AMyProjectGameMode::FinishGameplayBootstrap,
		ProceduralNavRebuildWaitSeconds,
		false);
}

void AMyProjectGameMode::FinishGameplayBootstrap()
{
	bProceduralRoundGenerated = false;
	ProceduralInitAttempts = 0;
	LastPreparationAttempts = FMath::Max(1, CurrentPreparationAttempt + 1);
	LastPreparationSeconds = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - PreparationStartTime) : 0.f;
	bLastPreparationSucceeded = true;
	LastPreparationFailReason = TEXT("None");
	UE_LOG(LogTemp, Log, TEXT("[Prep] OK attempts=%d time=%.2fs"), LastPreparationAttempts, LastPreparationSeconds);
	PreRoundState = EPreRoundState::ReadyToStart;
	OnKitchenPreparationComplete();
	RefreshKitchenReadyAtmosphere();
}

void AMyProjectGameMode::StartPreparedRound()
{
	if (PreRoundState != EPreRoundState::ReadyToStart)
	{
		return;
	}
	StopKitchenReadyAtmosphere();
	PreRoundState = EPreRoundState::InRound;
	RoundStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ApplyPreRoundPlayerLock(false);
	ApplyPendingLoadedSaveToPlayer();
	RefreshWorldTimeFreeze();
	if (bUseKitchenGridGenerator && SpawnedKitchenGenerator)
	{
		TryKitchenFinalizeDeferred();
	}
	else
	{
		TryGenerateProceduralRoundDeferred();
	}
	StartRoundAmbientLoop();
	StartInRoundAtmosphereTimer();
	RefreshRoundAmbientPlayback();
}

void AMyProjectGameMode::HandlePreRoundEnterPressed()
{
	if (PreRoundState == EPreRoundState::ReadyToStart)
	{
		StartPreparedRound();
	}
	else if (PreRoundState == EPreRoundState::Failed)
	{
		RetryPreparation();
	}
}

void AMyProjectGameMode::RequestNewRound()
{
	CancelDeathStingIfActive();
	MinRequiredBatteries = FMath::Clamp(MinRequiredBatteries, 2, 3);
	MaxRequiredBatteries = FMath::Clamp(MaxRequiredBatteries, 2, 3);
	if (MinRequiredBatteries > MaxRequiredBatteries)
	{
		Swap(MinRequiredBatteries, MaxRequiredBatteries);
	}
	if (!bPendingApplyLoadedSave)
	{
		RequiredBatteries = FMath::RandRange(MinRequiredBatteries, MaxRequiredBatteries);
		CollectedBatteries = 0;
	}
	bRoundWon = false;
	bRoundLost = false;
	bDeathStingActive = false;
	CrumbsThrownCount = 0;
	AIDetectionCount = 0;
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorldTimerManager().ClearTimer(ProceduralInitTimerHandle);
	GetWorldTimerManager().ClearTimer(WorldRestoreTimerHandle);
	GetWorldTimerManager().ClearTimer(DungeonBootstrapTimerHandle);
	GetWorldTimerManager().ClearTimer(TomatoSpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(KaravaychikSpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(VilokhvostSpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(PlayerReadyRetryTimerHandle);

	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			PC->SetPause(false);
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			PC->SetCinematicMode(false, false, false, true, true);

			APawn* OldPawn = PC->GetPawn();
			if (OldPawn)
			{
				PC->UnPossess();
				OldPawn->Destroy();
			}

			EnsurePlayerPawnReady(true);
		}
		RefreshWorldTimeFreeze();
	}
	BeginHiddenPreparation();
}

void AMyProjectGameMode::RetryPreparation()
{
	const bool bTargetMainMenu =
		FrontEndScreen == EFrontEndScreen::MainMenu || bLoadingVideoTargetsMainMenu || BootFlow == EHonorKitchenBootFlow::MainMenu;
	BeginLoadingVideoSequence(bTargetMainMenu);
	BeginHiddenPreparation();
}

void AMyProjectGameMode::ApplyPreRoundPlayerLock(bool bLock)
{
	bPreRoundInputLocked = bLock;

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	APlayerController* PC = W->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	PC->SetIgnoreMoveInput(bLock);
	PC->SetIgnoreLookInput(bLock);
	if (APawn* P = PC->GetPawn())
	{
		// Не выключаем коллизию и не DisableMovement: иначе капсула «засыпает» и пропадают все ходьба + прыжок.
		P->SetActorHiddenInGame(bLock);
		if (bLock)
		{
			if (ACharacter* Ch = Cast<ACharacter>(P))
			{
				if (UCharacterMovementComponent* Mov = Ch->GetCharacterMovement())
				{
					Mov->StopMovementImmediately();
					Mov->Velocity = FVector::ZeroVector;
				}
			}
		}
	}
}

bool AMyProjectGameMode::ValidatePreparedGeneration() const
{
	if (bUseKitchenGridGenerator)
	{
		return SpawnedKitchenGenerator != nullptr && SpawnedKitchenGenerator->HasValidCoreLoop();
	}
	return SpawnedDungeonGenerator != nullptr;
}

void AMyProjectGameMode::TryGenerateProceduralRoundDeferred()
{
	if (bProceduralRoundGenerated)
	{
		return;
	}

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

	// GameMode BeginPlay часто раньше появления/possession pawn.
	// Делаем несколько отложенных попыток, чтобы генерация гарантированно сработала.
	if (!PlayerPawn && EnsurePlayerPawnReady(true))
	{
		PlayerPawn = PC ? PC->GetPawn() : nullptr;
	}
	if (!PlayerPawn)
	{
		++ProceduralInitAttempts;
		HonorKitchenDevDebug::OnScreen(
			11001,
			0.3f,
			FColor::Yellow,
			FString::Printf(TEXT("ProcGen waiting pawn... attempt %d"), ProceduralInitAttempts));
		if (ProceduralInitAttempts <= 40) // ~10 секунд при интервале 0.25с
		{
			W->GetTimerManager().SetTimer(
				ProceduralInitTimerHandle,
				this,
				&AMyProjectGameMode::TryGenerateProceduralRoundDeferred,
				0.25f,
				false);
		}
		return;
	}

	GenerateProceduralRound();
	bProceduralRoundGenerated = true;
	W->GetTimerManager().ClearTimer(ProceduralInitTimerHandle);

	W->GetTimerManager().SetTimer(
		KaravaychikSpawnTimerHandle,
		this,
		&AMyProjectGameMode::SpawnKaravaychikIfConfigured,
		0.05f,
		false);
	W->GetTimerManager().SetTimer(
		VilokhvostSpawnTimerHandle,
		this,
		&AMyProjectGameMode::SpawnVilokhvostIfConfigured,
		0.1f,
		false);
	W->GetTimerManager().SetTimer(
		TomatoSpawnTimerHandle,
		this,
		&AMyProjectGameMode::SpawnTomatoesIfConfigured,
		0.08f,
		false);
	W->GetTimerManager().SetTimer(
		WorldRestoreTimerHandle,
		this,
		&AMyProjectGameMode::FinishWorldSpawnPipelineForLoad,
		0.35f,
		false);

	HonorKitchenDevDebug::OnScreen(11002, 3.5f, FColor::Green, TEXT("ProcGen DONE"));
}

void AMyProjectGameMode::SpawnTomatoesIfConfigured()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const int32 DesiredTomatoesRaw = bUseKitchenGridGenerator ? KitchenTomatoSpawnCount : DungeonTomatoSpawnCount;
	const int32 DesiredTomatoes = bUseKitchenGridGenerator ? FMath::Max(1, DesiredTomatoesRaw) : FMath::Clamp(DesiredTomatoesRaw, 0, 6);
	if (DesiredTomatoes <= 0)
	{
		FinishWorldSpawnPipelineForLoad();
		return;
	}

	for (TActorIterator<ATomatoSaurusCharacter> It(W); It; ++It)
	{
		AActor* A = *It;
		// Важно: Каравайчик наследуется от TomatoSaurusCharacter, его удалять здесь нельзя.
		if (A && A->GetClass() == ATomatoSaurusCharacter::StaticClass())
		{
			A->Destroy();
		}
	}

	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	TArray<FVector> Used;
	Used.Reserve(8);
	Used.Add(PlayerLoc);

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Та же схема, что у Каравайчика: kitchen точки генератора -> fallback через NavMesh вокруг игрока.
	if (bUseKitchenGridGenerator && SpawnedKitchenGenerator)
	{
		const TArray<FVector>& Locs = SpawnedKitchenGenerator->GetEnemySpawnWorldLocations();
		if (Locs.Num() > 0)
		{
			int32 SpawnedCount = 0;
			for (int32 i = 0; i < DesiredTomatoes; ++i)
			{
				const FVector Loc = Locs[(i + 2) % Locs.Num()] + FVector(0.f, 0.f, 40.f);
				if (ATomatoSaurusCharacter* T = W->SpawnActor<ATomatoSaurusCharacter>(ATomatoSaurusCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Sp))
				{
					Used.Add(T->GetActorLocation());
					++SpawnedCount;
				}
			}
			if (SpawnedCount > 0)
			{
#if !UE_BUILD_SHIPPING
				if (SpawnedKitchenGenerator)
				{
					SpawnedKitchenGenerator->LastTomatoSpawnFailures =
						FMath::Max(0, DesiredTomatoes - SpawnedCount);
					SpawnedKitchenGenerator->DebugPrintGenerationSummary(true);
				}
#endif
				return;
			}
		}
	}

	const float SpawnRadius = 2400.f;
	const float MinDist = 650.f;
	const float MinSpacing = 450.f;

	auto FallbackLoc = [&](int32 Index) -> FVector
	{
		const int32 N = FMath::Max(1, DesiredTomatoes);
		const float Angle = (2.f * UE_PI / static_cast<float>(N)) * static_cast<float>(Index) + 0.2f + FMath::FRandRange(-0.2f, 0.2f);
		const float R = FMath::Max(MinDist, 900.f);
		return PlayerLoc + FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 40.f);
	};

	int32 SpawnedCount = 0;
	for (int32 i = 0; i < DesiredTomatoes; ++i)
	{
		FVector Loc = FallbackLoc(i);
		if (NavSys)
		{
			for (int32 Attempt = 0; Attempt < 55; ++Attempt)
			{
				FNavLocation NavLoc;
				if (!NavSys->GetRandomReachablePointInRadius(PlayerLoc, SpawnRadius, NavLoc))
				{
					continue;
				}
				const FVector P = NavLoc.Location;
				if (FVector::DistSquared(P, PlayerLoc) < FMath::Square(MinDist))
				{
					continue;
				}
				bool bOk = true;
				for (const FVector& U : Used)
				{
					if (FVector::DistSquared(P, U) < FMath::Square(MinSpacing))
					{
						bOk = false;
						break;
					}
				}
				if (!bOk)
				{
					continue;
				}
				Loc = P + FVector(0.f, 0.f, 40.f);
				break;
			}
		}

		if (ATomatoSaurusCharacter* T = W->SpawnActor<ATomatoSaurusCharacter>(ATomatoSaurusCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Sp))
		{
			Used.Add(T->GetActorLocation());
			++SpawnedCount;
		}
	}

	// Аварийный гарант: хотя бы один томат должен появиться.
	int32 TotalTomatoesSpawned = SpawnedCount;
	if (SpawnedCount == 0 && PlayerPawn)
	{
		const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
		const FVector SafeForward = Forward.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Forward;
		const FVector EmergencyLoc = PlayerLoc + SafeForward * 420.f + FVector(0.f, 0.f, 40.f);
		if (W->SpawnActor<ATomatoSaurusCharacter>(ATomatoSaurusCharacter::StaticClass(), EmergencyLoc, FRotator::ZeroRotator, Sp))
		{
			TotalTomatoesSpawned = 1;
		}
	}

#if !UE_BUILD_SHIPPING
	if (bUseKitchenGridGenerator && SpawnedKitchenGenerator)
	{
		SpawnedKitchenGenerator->LastTomatoSpawnFailures =
			FMath::Max(0, DesiredTomatoes - TotalTomatoesSpawned);
		SpawnedKitchenGenerator->DebugPrintGenerationSummary(true);
	}
#endif

	FinishWorldSpawnPipelineForLoad();
}

void AMyProjectGameMode::FinishWorldSpawnPipelineForLoad()
{
	if (bPendingApplyWorldSnapshot)
	{
		RestoreWorldFromPendingSave();
	}
}

void AMyProjectGameMode::TryKitchenFinalizeDeferred()
{
	if (bProceduralRoundGenerated)
	{
		return;
	}

	UWorld* W = GetWorld();
	if (!W || !SpawnedKitchenGenerator)
	{
		return;
	}

	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn && EnsurePlayerPawnReady(true))
	{
		PlayerPawn = PC ? PC->GetPawn() : nullptr;
	}
	if (!PlayerPawn)
	{
		++ProceduralInitAttempts;
		HonorKitchenDevDebug::OnScreen(
			11011,
			0.3f,
			FColor::Yellow,
			FString::Printf(TEXT("Kitchen: waiting pawn... %d"), ProceduralInitAttempts));
		if (ProceduralInitAttempts <= 40)
		{
			W->GetTimerManager().SetTimer(
				ProceduralInitTimerHandle,
				this,
				&AMyProjectGameMode::TryKitchenFinalizeDeferred,
				0.25f,
				false);
		}
		return;
	}

	FVector Dest = SpawnedKitchenGenerator->GetStartCellWorldLocation();
	if (const ACharacter* Ch = Cast<ACharacter>(PlayerPawn))
	{
		if (const UCapsuleComponent* Cap = Ch->GetCapsuleComponent())
		{
			Dest.Z += Cap->GetScaledCapsuleHalfHeight() + 4.f;
		}
	}
	if (UCharacterMovementComponent* Mov = PlayerPawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		Mov->StopMovementImmediately();
	}
	PlayerPawn->SetActorLocation(Dest, false, nullptr, ETeleportType::ResetPhysics);

	bProceduralRoundGenerated = true;
	W->GetTimerManager().ClearTimer(ProceduralInitTimerHandle);

	W->GetTimerManager().SetTimer(
		KaravaychikSpawnTimerHandle,
		this,
		&AMyProjectGameMode::SpawnKaravaychikIfConfigured,
		0.05f,
		false);
	W->GetTimerManager().SetTimer(
		VilokhvostSpawnTimerHandle,
		this,
		&AMyProjectGameMode::SpawnVilokhvostIfConfigured,
		0.1f,
		false);
	W->GetTimerManager().SetTimer(
		TomatoSpawnTimerHandle,
		this,
		&AMyProjectGameMode::SpawnTomatoesIfConfigured,
		0.08f,
		false);

	HonorKitchenDevDebug::OnScreen(11012, 3.5f, FColor::Green, TEXT("Kitchen layout ready"));
}

void AMyProjectGameMode::GenerateProceduralRound()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	// Кухня уже ставит портал/батарейки по сетке — не переносить на случайный NavMesh.
	if (bUseKitchenGridGenerator && SpawnedKitchenGenerator)
	{
		return;
	}

	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		return;
	}

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);

	auto TryGetRandomPoint = [&](float Radius, float MinDistFromPlayer, float MinSpacing, const TArray<FVector>& Used, FVector& OutPoint) -> bool
	{
		if (!NavSys)
		{
			return false;
		}

		for (int32 Attempt = 0; Attempt < 45; ++Attempt)
		{
			FNavLocation NavLoc;
			if (!NavSys->GetRandomReachablePointInRadius(PlayerLoc, Radius, NavLoc))
			{
				continue;
			}

			const FVector P = NavLoc.Location;
			if (FVector::DistSquared(P, PlayerLoc) < FMath::Square(MinDistFromPlayer))
			{
				continue;
			}

			bool bOk = true;
			for (const FVector& U : Used)
			{
				if (FVector::DistSquared(P, U) < FMath::Square(MinSpacing))
				{
					bOk = false;
					break;
				}
			}

			if (!bOk)
			{
				continue;
			}

			OutPoint = P;
			return true;
		}
		return false;
	};

	auto FallbackPoint = [&](float Radius) -> FVector
	{
		const FVector2D Rand2D = FVector2D(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius));
		FVector P = PlayerLoc + FVector(Rand2D, 0.f);
		P.Z = PlayerLoc.Z;
		return P;
	};

	// 1) Портал: перенести в случайную точку на NavMesh.
	const float PortalRadius = 2800.f;
	const float MinPortalDist = 900.f;
	FVector PortalLoc = FallbackPoint(PortalRadius);
	TArray<FVector> UsedLocs;
	UsedLocs.Reserve(16);
	UsedLocs.Add(PlayerLoc);

	{
		FVector Candidate;
		if (TryGetRandomPoint(PortalRadius, MinPortalDist, 700.f, UsedLocs, Candidate))
		{
			PortalLoc = Candidate;
		}
	}

	bool bHasPortal = false;
	for (TActorIterator<APortal> It(W); It; ++It)
	{
		if (APortal* P = *It)
		{
			bHasPortal = true;
			P->SetActorLocation(PortalLoc);
		}
	}
	if (!bHasPortal)
	{
		W->SpawnActor<APortal>(APortal::StaticClass(), PortalLoc, FRotator::ZeroRotator);
	}

	// 2) Предметы: батарейки/медкит/соль/вода/крошки.
	// Victory: нужен портал + батарейки. Остальное — чтобы игрок показал core-loop в демо.
	const int32 NumBatteries = FMath::Clamp(RequiredBatteries, 2, 3);
	const int32 NumCrumbs = 3;

	auto SpawnPickupAt = [&](UClass* PickupClass, const FVector& Loc)
	{
		if (!PickupClass)
		{
			return;
		}

		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		W->SpawnActor<APickupBase>(PickupClass, Loc, FRotator::ZeroRotator, Sp);
	};

	const float ItemsRadius = 2400.f;
	const float MinItemDist = 450.f;
	const float MinItemSpacing = 250.f;

	UsedLocs.Add(PortalLoc);

	// Удаляем существующие пикапы в уровне, чтобы не оставались "старые" статичные точки.
	{
		for (TActorIterator<APickupBase> It(W); It; ++It)
		{
			if (APickupBase* A = *It)
			{
				A->Destroy();
			}
		}
	}

	for (int32 i = 0; i < NumBatteries; ++i)
	{
		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, MinItemDist, MinItemSpacing, UsedLocs, Candidate))
		{
			P = Candidate;
		}
		SpawnPickupAt(ABatteryPickup::StaticClass(), P);
		UsedLocs.Add(P);
	}

	{
		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, MinItemDist, MinItemSpacing, UsedLocs, Candidate))
		{
			P = Candidate;
		}
		SpawnPickupAt(AMedkitPickup::StaticClass(), P);
		UsedLocs.Add(P);
	}

	{
		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, MinItemDist, MinItemSpacing, UsedLocs, Candidate))
		{
			P = Candidate;
		}
		SpawnPickupAt(ASaltPickup::StaticClass(), P);
		UsedLocs.Add(P);
	}

	{
		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, MinItemDist, MinItemSpacing, UsedLocs, Candidate))
		{
			P = Candidate;
		}
		SpawnPickupAt(AWaterPickup::StaticClass(), P);
		UsedLocs.Add(P);
	}

	for (int32 i = 0; i < NumCrumbs; ++i)
	{
		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, MinItemDist, MinItemSpacing, UsedLocs, Candidate))
		{
			P = Candidate;
		}
		SpawnPickupAt(ACrumbPickup::StaticClass(), P);
		UsedLocs.Add(P);
	}

	// 3) Томатозавры: редакторные к этому моменту уже сняты в BeginPlay; переносим оставшихся и доспавниваем до DungeonTomatoSpawnCount.
	const int32 DesiredTomatoes = FMath::Clamp(DungeonTomatoSpawnCount, 0, 6);

	int32 ExistingTomatoCount = 0;
	for (TActorIterator<ATomatoSaurusCharacter> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		++ExistingTomatoCount;

		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, 650.f, 350.f, UsedLocs, Candidate))
		{
			P = Candidate;
		}

		A->SetActorLocation(P);
		UsedLocs.Add(P);
	}

	FActorSpawnParameters TomatoSpawnParams;
	TomatoSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (int32 i = ExistingTomatoCount; i < DesiredTomatoes; ++i)
	{
		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, 650.f, 350.f, UsedLocs, Candidate))
		{
			P = Candidate;
		}
		if (ATomatoSaurusCharacter* T = W->SpawnActor<ATomatoSaurusCharacter>(
				ATomatoSaurusCharacter::StaticClass(),
				P,
				FRotator::ZeroRotator,
				TomatoSpawnParams))
		{
			UsedLocs.Add(T->GetActorLocation());
		}
	}

	HonorKitchenDevDebug::OnScreen(
		11003,
		4.0f,
		FColor::Cyan,
		FString::Printf(TEXT("ProcGen: portal+items done, batteries=%d"), NumBatteries));
}

void AMyProjectGameMode::SpawnVilokhvostIfConfigured()
{
	if (!VilokhvostClass || VilokhvostSpawnCount <= 0 || !GetWorld())
	{
		return;
	}

	UWorld* W = GetWorld();
	{
		for (TActorIterator<AVilokhvostCharacter> It(W); It; ++It)
		{
			if (AVilokhvostCharacter* A = *It)
			{
				A->Destroy();
			}
		}
	}

	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	if (bUseKitchenGridGenerator && SpawnedKitchenGenerator)
	{
		const FVector PortalLoc = SpawnedKitchenGenerator->GetPortalWorldLocation();
		const FVector IntoRoom = SpawnedKitchenGenerator->GetPortalIntoRoom().GetSafeNormal2D();
		const FVector RoomCenter = SpawnedKitchenGenerator->GetPortalRoomFloorCenter();
		const float HoverZ = 230.f;
		const float BackOffset = FMath::Clamp(SpawnedKitchenGenerator->CellSizeUU * 0.28f, 70.f, 140.f);
		FVector SpawnLoc = RoomCenter + FVector(0.f, 0.f, HoverZ) + IntoRoom * BackOffset;
		if (FVector::DistSquared2D(SpawnLoc, PortalLoc) < FMath::Square(90.f))
		{
			SpawnLoc = PortalLoc + FVector(0.f, 0.f, HoverZ) + IntoRoom * 90.f;
		}
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		W->SpawnActor<AVilokhvostCharacter>(VilokhvostClass, SpawnLoc, FRotator::ZeroRotator, Sp);
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	TArray<FVector> Used;
	Used.Reserve(4);
	Used.Add(PlayerLoc);

	const float SpawnRadius = 2200.f;
	const float MinDist = 650.f;
	const float MinSpacing = 450.f;

	auto FallbackLoc = [&](int32 Index) -> FVector
	{
		const int32 N = FMath::Max(1, VilokhvostSpawnCount);
		const float Angle = (2.f * UE_PI / static_cast<float>(N)) * static_cast<float>(Index) + FMath::FRandRange(-0.2f, 0.2f);
		const float R = FMath::Max(MinDist, VilokhvostSpawnOffsetFromPlayer.Size2D());
		return PlayerLoc + FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 40.f);
	};

	for (int32 i = 0; i < VilokhvostSpawnCount; ++i)
	{
		FVector Loc = FallbackLoc(i);
		if (NavSys)
		{
			for (int32 Attempt = 0; Attempt < 55; ++Attempt)
			{
				FNavLocation NavLoc;
				if (!NavSys->GetRandomReachablePointInRadius(PlayerLoc, SpawnRadius, NavLoc))
				{
					continue;
				}
				const FVector P = NavLoc.Location;
				if (FVector::DistSquared(P, PlayerLoc) < FMath::Square(MinDist))
				{
					continue;
				}
				bool bOk = true;
				for (const FVector& U : Used)
				{
					if (FVector::DistSquared(P, U) < FMath::Square(MinSpacing))
					{
						bOk = false;
						break;
					}
				}
				if (!bOk)
				{
					continue;
				}

				Loc = P;
				break;
			}
		}

		Used.Add(Loc);
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		W->SpawnActor<AVilokhvostCharacter>(VilokhvostClass, Loc, FRotator::ZeroRotator, Sp);
	}
}

void AMyProjectGameMode::SpawnKaravaychikIfConfigured()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const int32 DesiredTomatoesRaw = bUseKitchenGridGenerator ? KitchenTomatoSpawnCount : DungeonTomatoSpawnCount;
	const int32 DesiredTomatoes = bUseKitchenGridGenerator ? FMath::Max(1, DesiredTomatoesRaw) : FMath::Clamp(DesiredTomatoesRaw, 0, 6);
	const int32 DesiredVilokhvost = FMath::Max(0, VilokhvostSpawnCount);
	const int32 OtherEnemiesTotal = DesiredTomatoes + DesiredVilokhvost;
	const int32 KaravaychikSpawnCount = FMath::Clamp(FMath::RoundToInt(static_cast<float>(OtherEnemiesTotal) * KaravaychikPopulationRatio), 1, 2);

	TSubclassOf<AKaravaychikCharacter> SpawnClass = KaravaychikClass;
	if (!SpawnClass)
	{
		SpawnClass = AKaravaychikCharacter::StaticClass();
	}

	for (TActorIterator<AKaravaychikCharacter> It(W); It; ++It)
	{
		if (AKaravaychikCharacter* A = *It)
		{
			A->Destroy();
		}
	}

	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	TArray<FVector> Used;
	Used.Reserve(4);
	Used.Add(PlayerLoc);

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (bUseKitchenGridGenerator && SpawnedKitchenGenerator)
	{
		const TArray<FVector>& Locs = SpawnedKitchenGenerator->GetEnemySpawnWorldLocations();
		if (Locs.Num() > 0)
		{
			for (int32 i = 0; i < KaravaychikSpawnCount; ++i)
			{
				const FVector Loc = Locs[(i + 1) % Locs.Num()] + FVector(0.f, 0.f, 40.f);
				if (AKaravaychikCharacter* K = W->SpawnActor<AKaravaychikCharacter>(SpawnClass, Loc, FRotator::ZeroRotator, Sp))
				{
					Used.Add(K->GetActorLocation());
				}
			}
			return;
		}
	}

	const float SpawnRadius = 2400.f;
	const float MinDist = 650.f;
	const float MinSpacing = 450.f;
	auto FallbackLoc = [&](int32 Index) -> FVector
	{
		const int32 N = FMath::Max(1, KaravaychikSpawnCount);
		const float Angle = (2.f * UE_PI / static_cast<float>(N)) * static_cast<float>(Index) + 0.35f + FMath::FRandRange(-0.2f, 0.2f);
		const float R = FMath::Max(MinDist, KaravaychikSpawnOffsetFromPlayer.Size2D());
		return PlayerLoc + FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 40.f);
	};

	int32 SpawnedCount = 0;
	for (int32 i = 0; i < KaravaychikSpawnCount; ++i)
	{
		FVector Loc = FallbackLoc(i);
		if (NavSys)
		{
			for (int32 Attempt = 0; Attempt < 55; ++Attempt)
			{
				FNavLocation NavLoc;
				if (!NavSys->GetRandomReachablePointInRadius(PlayerLoc, SpawnRadius, NavLoc))
				{
					continue;
				}
				const FVector P = NavLoc.Location;
				if (FVector::DistSquared(P, PlayerLoc) < FMath::Square(MinDist))
				{
					continue;
				}
				bool bOk = true;
				for (const FVector& U : Used)
				{
					if (FVector::DistSquared(P, U) < FMath::Square(MinSpacing))
					{
						bOk = false;
						break;
					}
				}
				if (!bOk)
				{
					continue;
				}
				Loc = P + FVector(0.f, 0.f, 40.f);
				break;
			}
		}

		if (AKaravaychikCharacter* K = W->SpawnActor<AKaravaychikCharacter>(SpawnClass, Loc, FRotator::ZeroRotator, Sp))
		{
			Used.Add(K->GetActorLocation());
			++SpawnedCount;
		}
	}

	if (SpawnedCount == 0 && PlayerPawn)
	{
		const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
		const FVector SafeForward = Forward.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Forward;
		const FVector EmergencyLoc = PlayerLoc + SafeForward * 450.f + FVector(0.f, 0.f, 40.f);
		W->SpawnActor<AKaravaychikCharacter>(SpawnClass, EmergencyLoc, FRotator::ZeroRotator, Sp);
	}
}

void AMyProjectGameMode::NotifyPlayerDied(APlayerController* PC)
{
	BeginPlayerDeathSting(PC);
}

void AMyProjectGameMode::StopGameplayAudioForDeathSting()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	RefreshFrontEndMusic();

	for (TObjectIterator<UAudioComponent> It; It; ++It)
	{
		UAudioComponent* const AC = *It;
		if (!AC || !IsValid(AC) || AC == DeathStingAudioComponent || AC == RoundAmbientAudioComponent)
		{
			continue;
		}
		if (!AC->GetWorld() || AC->GetWorld() != World)
		{
			continue;
		}
		if (AC->IsPlaying())
		{
			AC->Stop();
		}
	}
}

void AMyProjectGameMode::BeginPlayerDeathSting(APlayerController* PC)
{
	if (bRoundWon || bRoundLost || bDeathStingActive || !PC || !GetWorld())
	{
		return;
	}

	StopInRoundAtmosphereTimer();
	StopKitchenReadyAtmosphere();
	RefreshRoundAmbientPlayback();
	bDeathStingActive = true;
	DeathStingRealtimeElapsed = 0.f;
	DeathStingPlayerController = PC;
	HonorKitchenAudioSettings::SetDeathStingActive(true);

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (PC->IsPaused())
	{
		PC->SetPause(false);
	}

	RefreshWorldTimeFreeze();
	StopGameplayAudioForDeathSting();

	USoundBase* const DeathSound = HonorKitchenAudioDefaults::GetPlayerDeathSound();
	if (!DeathSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Death sting: Death_Master missing — run Tools/import_enemy_sounds.py"));
		FinishPlayerDeathSting();
		return;
	}

	const float Volume = HonorKitchenAudioSettings::ScaleVolume(1.f);
	DeathStingAudioComponent = UGameplayStatics::SpawnSound2D(
		GetWorld(),
		DeathSound,
		Volume,
		1.f,
		0.f,
		nullptr,
		false,
		false);
	if (DeathStingAudioComponent)
	{
		DeathStingAudioComponent->bIsUISound = true;
		DeathStingAudioComponent->bAutoDestroy = false;
		DeathStingAudioComponent->OnAudioFinished.AddDynamic(this, &AMyProjectGameMode::OnDeathStingAudioFinished);
		DeathStingAudioComponent->Play();
	}

	const float Duration = FMath::Max(0.1f, DeathSound->GetDuration());
	if (DeathStingRealtimeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeathStingRealtimeTickerHandle);
		DeathStingRealtimeTickerHandle.Reset();
	}
	const float FallbackSeconds = Duration + 0.35f;
	TWeakObjectPtr<AMyProjectGameMode> WeakThis(this);
	DeathStingRealtimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis, FallbackSeconds](float DeltaTime) -> bool
		{
			AMyProjectGameMode* GM = WeakThis.Get();
			if (!GM || !GM->bDeathStingActive)
			{
				return false;
			}
			GM->DeathStingRealtimeElapsed += DeltaTime;
			if (GM->DeathStingRealtimeElapsed >= FallbackSeconds)
			{
				GM->FinishPlayerDeathSting();
				return false;
			}
			return true;
		}));
}

void AMyProjectGameMode::OnDeathStingAudioFinished()
{
	FinishPlayerDeathSting();
}

void AMyProjectGameMode::FinishPlayerDeathSting()
{
	if (!bDeathStingActive)
	{
		return;
	}

	if (DeathStingRealtimeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeathStingRealtimeTickerHandle);
		DeathStingRealtimeTickerHandle.Reset();
	}
	DeathStingRealtimeElapsed = 0.f;

	if (DeathStingAudioComponent && DeathStingAudioComponent->IsPlaying())
	{
		DeathStingAudioComponent->Stop();
	}
	DeathStingAudioComponent = nullptr;

	HonorKitchenAudioSettings::SetDeathStingActive(false);
	bDeathStingActive = false;
	bRoundLost = true;

	if (APlayerController* PC = DeathStingPlayerController.Get())
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		if (AMyProjectCharacter* Char = PC->GetPawn<AMyProjectCharacter>())
		{
			Char->ClearDeathVignetteHold();
		}
	}
	DeathStingPlayerController = nullptr;
	RefreshWorldTimeFreeze();
}

void AMyProjectGameMode::CancelDeathStingIfActive()
{
	if (!bDeathStingActive)
	{
		return;
	}
	FinishPlayerDeathSting();
}

void AMyProjectGameMode::NotifyCrumbThrown()
{
	++CrumbsThrownCount;
}

void AMyProjectGameMode::NotifyAIDetectedPlayer()
{
	++AIDetectionCount;
}

void AMyProjectGameMode::NotifyBatteryCollected(int32 Amount)
{
	if (bRoundWon || bRoundLost || Amount <= 0)
	{
		return;
	}
	CollectedBatteries = FMath::Clamp(CollectedBatteries + Amount, 0, 999);
}

bool AMyProjectGameMode::TryActivatePortal(APlayerController* PC)
{
	if (!PC)
	{
		return false;
	}
	if (bRoundLost)
	{
		return false;
	}
	if (CollectedBatteries < RequiredBatteries)
	{
		return false;
	}
	bRoundWon = true;
	StopInRoundAtmosphereTimer();
	RefreshRoundAmbientPlayback();
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
	}
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	RefreshWorldTimeFreeze();
	return true;
}

bool AMyProjectGameMode::IsLocalGameplayPaused() const
{
	if (const UWorld* W = GetWorld())
	{
		if (const APlayerController* PC = W->GetFirstPlayerController())
		{
			return PC->IsPaused();
		}
	}
	return false;
}

bool AMyProjectGameMode::ShouldFreezeWorld() const
{
	if (PreRoundState != EPreRoundState::InRound)
	{
		return false;
	}
	if (bDeathStingActive || bRoundWon || bRoundLost)
	{
		return true;
	}
	return IsLocalGameplayPaused();
}

void AMyProjectGameMode::RefreshWorldTimeFreeze()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bWantFreeze = ShouldFreezeWorld();
	if (bWantFreeze)
	{
		if (!bWorldFrozenForMenus)
		{
			const AWorldSettings* WS = World->GetWorldSettings();
			SavedTimeDilationBeforeWorldFreeze = WS ? WS->TimeDilation : 1.f;
			if (SavedTimeDilationBeforeWorldFreeze <= KINDA_SMALL_NUMBER)
			{
				SavedTimeDilationBeforeWorldFreeze = 1.f;
			}
			UGameplayStatics::SetGlobalTimeDilation(World, 0.f);
			bWorldFrozenForMenus = true;
		}
	}
	else if (bWorldFrozenForMenus)
	{
		const float Restore = FMath::Max(SavedTimeDilationBeforeWorldFreeze, 1.f);
		UGameplayStatics::SetGlobalTimeDilation(World, Restore);
		bWorldFrozenForMenus = false;
		SavedTimeDilationBeforeWorldFreeze = 1.f;
	}

	RefreshRoundAmbientPlayback();
}

float AMyProjectGameMode::GetRoundElapsedSeconds() const
{
	if (const UWorld* W = GetWorld())
	{
		return FMath::Max(0.f, static_cast<float>(W->GetTimeSeconds() - RoundStartTime));
	}
	return 0.f;
}

bool AMyProjectGameMode::HasSaveGameOnDisk() const
{
	return UGameplayStatics::DoesSaveGameExist(UHonorKitchenSaveGame::DefaultSlotName(), 0);
}

void AMyProjectGameMode::StartNewGameFromMenu()
{
	StopKitchenReadyAtmosphere();
	StopRoundAmbientLoop();
	FrontEndScreen = EFrontEndScreen::None;
	bPendingApplyLoadedSave = false;
	bPendingApplyWorldSnapshot = false;
	PendingLoadedHotbar.Reset();
	PendingWorldActors.Reset();
	KitchenMapSeed = 0;
	ApplyFrontEndPresentation(false);
	StopMainMenuBackgroundVideo();
	BeginLoadingVideoSequence(false);
	RequestNewRound();
}

void AMyProjectGameMode::LoadGameFromMenu()
{
	StopKitchenReadyAtmosphere();
	StopRoundAmbientLoop();
	if (!HasSaveGameOnDisk())
	{
		return;
	}
	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(UHonorKitchenSaveGame::DefaultSlotName(), 0);
	UHonorKitchenSaveGame* Save = Cast<UHonorKitchenSaveGame>(Loaded);
	if (!Save || Save->MapSeed == 0)
	{
		return;
	}
	FrontEndScreen = EFrontEndScreen::None;
	bPendingApplyLoadedSave = true;
	bPendingApplyWorldSnapshot = true;
	KitchenMapSeed = Save->MapSeed;
	RequiredBatteries = FMath::Clamp(Save->RequiredBatteries, 2, 3);
	CollectedBatteries = FMath::Max(0, Save->CollectedBatteries);
	PendingLoadedHealth = Save->PlayerHealth;
	PendingLoadedMaxHealth = Save->PlayerMaxHealth;
	PendingLoadedHotbar = Save->Hotbar;
	PendingPlayerLocation = Save->PlayerLocation;
	PendingPlayerRotation = Save->PlayerRotation;
	PendingWorldActors = Save->WorldActors;
	ApplyFrontEndPresentation(false);
	StopMainMenuBackgroundVideo();
	BeginLoadingVideoSequence(false);
	RequestNewRound();
}

void AMyProjectGameMode::OpenCreditsFromMenu()
{
	FrontEndScreen = EFrontEndScreen::Credits;
	RefreshFrontEndMusic();
}

void AMyProjectGameMode::CloseCreditsToMainMenu()
{
	FrontEndScreen = EFrontEndScreen::MainMenu;
	bPauseSettingsOpen = false;
	RefreshFrontEndMusic();
}

void AMyProjectGameMode::OpenSettingsFromMenu()
{
	FrontEndScreenBeforeSettings = EFrontEndScreen::MainMenu;
	FrontEndScreen = EFrontEndScreen::Settings;
	RefreshFrontEndMusic();
}

void AMyProjectGameMode::OpenSettingsFromCredits()
{
	FrontEndScreenBeforeSettings = EFrontEndScreen::Credits;
	FrontEndScreen = EFrontEndScreen::Settings;
	RefreshFrontEndMusic();
}

void AMyProjectGameMode::OpenSettingsFromPause()
{
	bPauseSettingsOpen = true;
}

void AMyProjectGameMode::CloseSettings()
{
	if (bPauseSettingsOpen)
	{
		bPauseSettingsOpen = false;
	}
	else
	{
		FrontEndScreen = FrontEndScreenBeforeSettings;
		RefreshFrontEndMusic();
	}
	HonorKitchenAudioSettings::EnforceDeveloperModeRuntime(GetWorld());
}

bool AMyProjectGameMode::IsSettingsOpen() const
{
	return FrontEndScreen == EFrontEndScreen::Settings || bPauseSettingsOpen;
}

bool AMyProjectGameMode::IsGameSoundEnabled() const
{
	return HonorKitchenAudioSettings::IsSoundEnabled();
}

void AMyProjectGameMode::ToggleMenuSound()
{
	HonorKitchenAudioSettings::ToggleSound();
	RefreshFrontEndMusic();
}

void AMyProjectGameMode::OnPlayerSettingsChanged()
{
	if (FrontEndScreen != EFrontEndScreen::None)
	{
		RefreshFrontEndMusic();
	}
	RefreshKitchenReadyAtmosphere();
	RefreshRoundAmbientPlayback();
	HonorKitchenAudioSettings::EnforceDeveloperModeRuntime(GetWorld());
}

namespace HonorKitchenMenuMusicPrivate
{
	static bool IsPlayableMenuMusic(const USoundBase* Sound)
	{
		const USoundWave* Wave = Cast<USoundWave>(Sound);
		return Wave && Wave->GetDuration() > 1.f;
	}

	static void StopComponent(TObjectPtr<UAudioComponent>& Component)
	{
		if (!Component)
		{
			return;
		}
		Component->Stop();
		Component->DestroyComponent();
		Component = nullptr;
	}
}

USoundBase* AMyProjectGameMode::ResolveMenuMusic() const
{
	using namespace HonorKitchenMenuMusicPrivate;

	if (MenuMusicAsset.ToSoftObjectPath().IsValid())
	{
		if (USoundBase* Loaded = MenuMusicAsset.LoadSynchronous())
		{
			if (IsPlayableMenuMusic(Loaded))
			{
				return Loaded;
			}
		}
	}

	static const TCHAR* FallbackPaths[] = {
		TEXT("/Game/Audio/Menu/Clarinet.Clarinet"),
		TEXT("/Game/Audio/Menu/Clarinet"),
	};
	for (const TCHAR* Path : FallbackPaths)
	{
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, Path))
		{
			if (IsPlayableMenuMusic(S))
			{
				return S;
			}
		}
	}
	return nullptr;
}

bool AMyProjectGameMode::ShouldPlayFrontEndMenuMusic() const
{
	if (!HonorKitchenAudioSettings::IsSoundEnabled())
	{
		return false;
	}

	if (BootFlow == EHonorKitchenBootFlow::IntroVideo || BootFlow == EHonorKitchenBootFlow::LoadingVideo)
	{
		return true;
	}

	if (BootFlow == EHonorKitchenBootFlow::MainMenu)
	{
		return FrontEndScreen == EFrontEndScreen::MainMenu
			|| FrontEndScreen == EFrontEndScreen::Credits
			|| FrontEndScreen == EFrontEndScreen::Settings;
	}

	return false;
}

void AMyProjectGameMode::RefreshFrontEndMusic()
{
	using namespace HonorKitchenMenuMusicPrivate;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bShouldPlay = ShouldPlayFrontEndMenuMusic();
	if (!bShouldPlay)
	{
		StopComponent(MenuMusicComponent);
		return;
	}

	USoundBase* const Sound = ResolveMenuMusic();
	if (!Sound)
	{
		StopComponent(MenuMusicComponent);
		UE_LOG(LogTemp, Warning, TEXT("Menu music: no valid SoundWave at /Game/Audio/Menu/Serial2 — re-run Tools/import_menu_music.py"));
		return;
	}

	const float Volume = MenuMusicVolume * HonorKitchenAudioSettings::GetMusicVolume() * HonorKitchenAudioSettings::GetMasterVolume();
	if (MenuMusicComponent && MenuMusicComponent->IsPlaying() && MenuMusicComponent->Sound == Sound)
	{
		MenuMusicComponent->SetVolumeMultiplier(Volume);
		return;
	}

	StopComponent(MenuMusicComponent);
	MenuMusicComponent = UGameplayStatics::SpawnSound2D(
		World,
		Sound,
		Volume,
		1.f,
		0.f,
		nullptr,
		false,
		false);
	if (MenuMusicComponent)
	{
		MenuMusicComponent->bAutoDestroy = false;
	}
}

namespace HonorKitchenAtmosphereAudioPrivate
{
	static void StopComponent(TObjectPtr<UAudioComponent>& Component)
	{
		if (!Component)
		{
			return;
		}
		Component->Stop();
		Component->DestroyComponent();
		Component = nullptr;
	}
}

bool AMyProjectGameMode::CanPlayInRoundAtmosphere() const
{
	if (!HonorKitchenAudioSettings::IsSoundEnabled() || HonorKitchenAudioSettings::IsDeathStingActive())
	{
		return false;
	}
	if (PreRoundState != EPreRoundState::InRound || bRoundWon || bRoundLost)
	{
		return false;
	}
	return !IsLocalGameplayPaused();
}

void AMyProjectGameMode::StopKitchenReadyAtmosphere()
{
	using namespace HonorKitchenAtmosphereAudioPrivate;
	StopComponent(KitchenReadyAudioComponent);
}

bool AMyProjectGameMode::ShouldPlayKitchenReadyAtmosphere() const
{
	if (!HonorKitchenAudioSettings::IsSoundEnabled())
	{
		return false;
	}
	if (PreRoundState != EPreRoundState::ReadyToStart)
	{
		return false;
	}
	// Как в HUD: экран «Кухня готова» не поверх intro/loading и не в главном меню.
	if (IsFrontEndVideoBlockingUI())
	{
		return false;
	}
	if (FrontEndScreen != EFrontEndScreen::None)
	{
		return false;
	}
	return true;
}

void AMyProjectGameMode::RefreshKitchenReadyAtmosphere()
{
	using namespace HonorKitchenAtmosphereAudioPrivate;

	UWorld* World = GetWorld();
	if (!World || !ShouldPlayKitchenReadyAtmosphere())
	{
		StopKitchenReadyAtmosphere();
		return;
	}

	USoundBase* const Sound = HonorKitchenAtmosphereAudio::GetKitchenReadySound();
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("KitchenReady: missing /Game/Audio/Atmosphere/KitchenReady_Master — run Tools/import_atmosphere_sounds.py"));
		return;
	}

	const float Volume = KitchenReadyVolume * HonorKitchenAudioSettings::GetMusicVolume()
		* HonorKitchenAudioSettings::GetMasterVolume();

	if (KitchenReadyAudioComponent && KitchenReadyAudioComponent->IsPlaying()
		&& KitchenReadyAudioComponent->Sound == Sound)
	{
		KitchenReadyAudioComponent->SetVolumeMultiplier(Volume);
		return;
	}

	StopKitchenReadyAtmosphere();
	KitchenReadyAudioComponent = UGameplayStatics::SpawnSound2D(
		World,
		Sound,
		Volume,
		1.f,
		0.f,
		nullptr,
		false,
		false);
	if (KitchenReadyAudioComponent)
	{
		KitchenReadyAudioComponent->bAutoDestroy = false;
		KitchenReadyAudioComponent->OnAudioFinished.Clear();
		KitchenReadyAudioComponent->OnAudioFinished.AddUniqueDynamic(
			this,
			&AMyProjectGameMode::OnKitchenReadyStingFinished);
	}
}

void AMyProjectGameMode::OnKitchenReadyStingFinished()
{
	StartRoundAmbientLoop();
}

bool AMyProjectGameMode::ShouldPlayRoundAmbient() const
{
	if (!HonorKitchenAudioSettings::IsSoundEnabled() || HonorKitchenAudioSettings::IsDeathStingActive()
		|| bDeathStingActive)
	{
		return false;
	}
	if (bRoundWon || bRoundLost || IsLocalGameplayPaused())
	{
		return false;
	}
	if (PreRoundState != EPreRoundState::InRound && PreRoundState != EPreRoundState::ReadyToStart)
	{
		return false;
	}
	return RoundAmbientAudioComponent != nullptr;
}

void AMyProjectGameMode::StopRoundAmbientLoop()
{
	using namespace HonorKitchenAtmosphereAudioPrivate;
	StopComponent(RoundAmbientAudioComponent);
}

void AMyProjectGameMode::StartRoundAmbientLoop()
{
	using namespace HonorKitchenAtmosphereAudioPrivate;

	if (!HonorKitchenAudioSettings::IsSoundEnabled())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USoundBase* const Sound = HonorKitchenAtmosphereAudio::GetRoundAmbientLoop();
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Round ambient: missing sound_19805 — run Tools/import_atmosphere_sounds.py"));
		return;
	}

	const float Volume = RoundAmbientVolume * HonorKitchenAudioSettings::GetMusicVolume()
		* HonorKitchenAudioSettings::GetMasterVolume();

	if (RoundAmbientAudioComponent && RoundAmbientAudioComponent->Sound == Sound)
	{
		RoundAmbientAudioComponent->SetVolumeMultiplier(Volume);
		RoundAmbientAudioComponent->SetPaused(false);
		if (!RoundAmbientAudioComponent->IsPlaying())
		{
			RoundAmbientAudioComponent->Play();
		}
		return;
	}

	StopRoundAmbientLoop();
	RoundAmbientAudioComponent = UGameplayStatics::SpawnSound2D(
		World,
		Sound,
		Volume,
		1.f,
		0.f,
		nullptr,
		false,
		false);
	if (RoundAmbientAudioComponent)
	{
		RoundAmbientAudioComponent->bAutoDestroy = false;
		RoundAmbientAudioComponent->Play();
	}
}

void AMyProjectGameMode::RefreshRoundAmbientPlayback()
{
	if (!RoundAmbientAudioComponent)
	{
		return;
	}

	const float Volume = RoundAmbientVolume * HonorKitchenAudioSettings::GetMusicVolume()
		* HonorKitchenAudioSettings::GetMasterVolume();
	RoundAmbientAudioComponent->SetVolumeMultiplier(Volume);

	const bool bShouldPlay = ShouldPlayRoundAmbient();
	if (bShouldPlay)
	{
		RoundAmbientAudioComponent->SetPaused(false);
		if (!RoundAmbientAudioComponent->IsPlaying())
		{
			RoundAmbientAudioComponent->Play();
		}
	}
	else
	{
		RoundAmbientAudioComponent->SetPaused(true);
	}
}

void AMyProjectGameMode::StopInRoundAtmosphereTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InRoundAtmosphereTimerHandle);
	}
	using namespace HonorKitchenAtmosphereAudioPrivate;
	StopComponent(AtmosphereStingAudioComponent);
}

void AMyProjectGameMode::StartInRoundAtmosphereTimer()
{
	StopInRoundAtmosphereTimer();
	if (!CanPlayInRoundAtmosphere())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Interval = FMath::Max(30.f, AtmosphereStingIntervalSeconds);
	World->GetTimerManager().SetTimer(
		InRoundAtmosphereTimerHandle,
		this,
		&AMyProjectGameMode::OnInRoundAtmosphereTimer,
		Interval,
		true);
}

void AMyProjectGameMode::PlayRandomAtmosphereSting()
{
	using namespace HonorKitchenAtmosphereAudioPrivate;

	if (!CanPlayInRoundAtmosphere())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USoundBase* const Sound = HonorKitchenAtmosphereAudio::PickRandomRoundSting();
	if (!Sound)
	{
		return;
	}

	StopComponent(AtmosphereStingAudioComponent);

	const float Volume = AtmosphereStingVolume * HonorKitchenAudioSettings::GetMusicVolume()
		* HonorKitchenAudioSettings::GetMasterVolume();

	AtmosphereStingAudioComponent = UGameplayStatics::SpawnSound2D(
		World,
		Sound,
		Volume,
		1.f,
		0.f,
		nullptr,
		false,
		true);
}

void AMyProjectGameMode::OnInRoundAtmosphereTimer()
{
	PlayRandomAtmosphereSting();
}

void AMyProjectGameMode::QuitFromMenu()
{
	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			UKismetSystemLibrary::QuitGame(W, PC, EQuitPreference::Quit, false);
		}
	}
}

bool AMyProjectGameMode::TryWriteSaveGame()
{
	if (PreRoundState != EPreRoundState::InRound || bRoundWon || bRoundLost || bDeathStingActive)
	{
		return false;
	}
	UWorld* W = GetWorld();
	if (!W)
	{
		return false;
	}
	AMyProjectCharacter* Char = nullptr;
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		Char = PC->GetPawn<AMyProjectCharacter>();
	}
	if (!Char || Char->IsDead())
	{
		return false;
	}
	UHonorKitchenSaveGame* Save = Cast<UHonorKitchenSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UHonorKitchenSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->MapSeed = SpawnedKitchenGenerator ? SpawnedKitchenGenerator->LastUsedSeed : KitchenMapSeed;
	if (Save->MapSeed == 0)
	{
		return false;
	}
	Save->RequiredBatteries = RequiredBatteries;
	Save->CollectedBatteries = CollectedBatteries;
	Save->PlayerHealth = Char->CurrentHealth;
	Save->PlayerMaxHealth = Char->MaxHealth;
	Char->BuildHotbarForSave(Save->Hotbar);
	Save->PlayerLocation = Char->GetActorLocation();
	Save->PlayerRotation = Char->GetActorRotation();
	Save->SavedWorldTimeSeconds = W->GetTimeSeconds();
	CaptureWorldToSave(Save);
	return UGameplayStatics::SaveGameToSlot(Save, UHonorKitchenSaveGame::DefaultSlotName(), 0);
}

void AMyProjectGameMode::ApplyPendingLoadedSaveToPlayer()
{
	if (!bPendingApplyLoadedSave)
	{
		return;
	}
	bPendingApplyLoadedSave = false;
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	APlayerController* PC = W->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	AMyProjectCharacter* Char = PC->GetPawn<AMyProjectCharacter>();
	if (!Char)
	{
		return;
	}
	Char->RestoreStatsFromSave(PendingLoadedHealth, PendingLoadedMaxHealth, PendingLoadedHotbar);
	if (bPendingApplyWorldSnapshot)
	{
		Char->SetActorLocation(PendingPlayerLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Char->SetActorRotation(PendingPlayerRotation);
	}
	PendingLoadedHotbar.Reset();
}

void AMyProjectGameMode::ApplyFrontEndPresentation(bool bActive)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	SetLevelTemplateHiddenForFrontEnd(bActive);
	ApplyPreRoundPlayerLock(bActive);

	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (AMyProjectPlayerController* MPC = Cast<AMyProjectPlayerController>(PC))
		{
			MPC->ApplyFrontEndInputMode(bActive);
		}
		else if (bActive)
		{
			PC->bShowMouseCursor = true;
			PC->bEnableClickEvents = true;
		}
	}

	RefreshFrontEndMusic();
}

void AMyProjectGameMode::SetLevelTemplateHiddenForFrontEnd(bool bHide)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	if (!bHide)
	{
		for (const TWeakObjectPtr<AActor>& Weak : LevelActorsHiddenForFrontEnd)
		{
			if (AActor* A = Weak.Get())
			{
				A->SetActorHiddenInGame(false);
			}
		}
		LevelActorsHiddenForFrontEnd.Reset();
		return;
	}

	LevelActorsHiddenForFrontEnd.Reset();
	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		AStaticMeshActor* SMA = *It;
		if (!SMA || SMA->IsHidden())
		{
			continue;
		}
		SMA->SetActorHiddenInGame(true);
		LevelActorsHiddenForFrontEnd.Add(SMA);
	}
}

void AMyProjectGameMode::ReturnToMainMenu()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorldTimerManager().ClearTimer(ProceduralInitTimerHandle);
	GetWorldTimerManager().ClearTimer(WorldRestoreTimerHandle);
	GetWorldTimerManager().ClearTimer(DungeonBootstrapTimerHandle);
	GetWorldTimerManager().ClearTimer(TomatoSpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(KaravaychikSpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(VilokhvostSpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(PlayerReadyRetryTimerHandle);

	DestroyAllActorsOfClassInWorld(W, ATomatoSaurusCharacter::StaticClass());
	DestroyAllActorsOfClassInWorld(W, AKaravaychikCharacter::StaticClass());
	DestroyAllActorsOfClassInWorld(W, AVilokhvostCharacter::StaticClass());
	DestroyAllActorsOfClassInWorld(W, APickupBase::StaticClass());
	DestroyAllActorsOfClassInWorld(W, APortal::StaticClass());
	DestroyAllActorsOfClassInWorld(W, ACrumbProjectile::StaticClass());

	if (SpawnedKitchenGenerator)
	{
		SpawnedKitchenGenerator->Destroy();
		SpawnedKitchenGenerator = nullptr;
	}
	if (SpawnedDungeonGenerator)
	{
		SpawnedDungeonGenerator->Destroy();
		SpawnedDungeonGenerator = nullptr;
	}

	bProceduralRoundGenerated = false;
	bRoundWon = false;
	bRoundLost = false;
	bPendingApplyLoadedSave = false;
	bPendingApplyWorldSnapshot = false;
	PendingWorldActors.Reset();
	bPauseSettingsOpen = false;
	PreRoundState = EPreRoundState::Preparing;
	FrontEndScreen = EFrontEndScreen::None;

	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		PC->SetPause(false);
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}

	ApplyFrontEndPresentation(true);
	BeginLoadingVideoSequence(true);
	BeginHiddenPreparation();
}

bool AMyProjectGameMode::IsFrontEndVideoBlockingUI() const
{
	return BootFlow == EHonorKitchenBootFlow::IntroVideo || BootFlow == EHonorKitchenBootFlow::LoadingVideo;
}

UMediaTexture* AMyProjectGameMode::GetFrontEndFullscreenVideoTexture() const
{
	return FrontEndMedia ? FrontEndMedia->GetFullscreenVideoTexture() : nullptr;
}

UMediaTexture* AMyProjectGameMode::GetMainMenuBackgroundVideoTexture() const
{
	return FrontEndMedia ? FrontEndMedia->GetMainMenuBackgroundTexture() : nullptr;
}

bool AMyProjectGameMode::ShouldDrawMainMenuBackgroundVideo() const
{
	if (BootFlow != EHonorKitchenBootFlow::MainMenu)
	{
		return false;
	}

	return FrontEndScreen == EFrontEndScreen::MainMenu
		|| FrontEndScreen == EFrontEndScreen::Credits
		|| FrontEndScreen == EFrontEndScreen::Settings;
}

void AMyProjectGameMode::StartInitialBootSequence()
{
	FrontEndScreen = EFrontEndScreen::None;
	BootFlow = EHonorKitchenBootFlow::IntroVideo;
	bLoadingVideoTargetsMainMenu = true;
	bKitchenPrepComplete = false;

	FrontEndMedia = NewObject<UHonorKitchenFrontEndMedia>(this);
	if (FrontEndMedia)
	{
		FrontEndMedia->Initialize(this);
	}

	BeginHiddenPreparation();
	ApplyFrontEndPresentation(true);

	if (FrontEndMedia)
	{
		FrontEndMedia->PlayIntroVideo(FHonorKitchenFrontEndMediaSimpleDelegate::CreateUObject(
			this, &AMyProjectGameMode::OnIntroVideoFinished));
	}
	else
	{
		OnIntroVideoFinished();
	}

	RefreshFrontEndMusic();
}

void AMyProjectGameMode::BeginLoadingVideoSequence(bool bTargetMainMenuAfterComplete)
{
	bLoadingVideoTargetsMainMenu = bTargetMainMenuAfterComplete;
	BootFlow = EHonorKitchenBootFlow::LoadingVideo;
	FrontEndScreen = EFrontEndScreen::None;

	if (!FrontEndMedia)
	{
		FrontEndMedia = NewObject<UHonorKitchenFrontEndMedia>(this);
		if (FrontEndMedia)
		{
			FrontEndMedia->Initialize(this);
		}
	}

	if (FrontEndMedia)
	{
		FrontEndMedia->StopMainMenuBackground();
		FrontEndMedia->PlayLoadingVideo(FHonorKitchenFrontEndMediaSimpleDelegate::CreateUObject(
			this, &AMyProjectGameMode::OnLoadingVideoPlaybackEnded));
	}
	else
	{
		OnLoadingVideoPlaybackEnded();
	}
}

void AMyProjectGameMode::OnIntroVideoFinished()
{
	BeginLoadingVideoSequence(true);
}

void AMyProjectGameMode::OnLoadingVideoPlaybackEnded()
{
	if (!bKitchenPrepComplete)
	{
		RestartLoadingVideoFromBeginning();
		return;
	}

	if (FrontEndMedia)
	{
		FrontEndMedia->StopLoadingVideo();
	}

	if (bLoadingVideoTargetsMainMenu)
	{
		if (PreRoundState == EPreRoundState::Failed)
		{
			BootFlow = EHonorKitchenBootFlow::MainMenu;
			FrontEndScreen = EFrontEndScreen::None;
		}
		else
		{
			ShowMainMenuAfterBoot();
		}
	}
	else
	{
		BootFlow = EHonorKitchenBootFlow::MainMenu;
		FrontEndScreen = EFrontEndScreen::None;
		ApplyFrontEndPresentation(false);
	}

	RefreshFrontEndMusic();
	RefreshKitchenReadyAtmosphere();
}

void AMyProjectGameMode::OnKitchenPreparationComplete()
{
	bKitchenPrepComplete = true;
}

void AMyProjectGameMode::ShowMainMenuAfterBoot()
{
	StopKitchenReadyAtmosphere();
	StopRoundAmbientLoop();
	BootFlow = EHonorKitchenBootFlow::MainMenu;
	FrontEndScreen = EFrontEndScreen::MainMenu;
	bPauseSettingsOpen = false;

	if (FrontEndMedia && !FrontEndMedia->IsMainMenuBackgroundActive())
	{
		FrontEndMedia->StartMainMenuBackgroundLoop();
	}

	ApplyFrontEndPresentation(true);
	RefreshFrontEndMusic();
}

void AMyProjectGameMode::StopMainMenuBackgroundVideo()
{
	if (FrontEndMedia)
	{
		FrontEndMedia->StopMainMenuBackground();
	}
}

void AMyProjectGameMode::RestartLoadingVideoFromBeginning()
{
	if (FrontEndMedia)
	{
		FrontEndMedia->PlayLoadingVideo(FHonorKitchenFrontEndMediaSimpleDelegate::CreateUObject(
			this, &AMyProjectGameMode::OnLoadingVideoPlaybackEnded));
	}
}

namespace HonorKitchenSaveHelpers
{
	static FString ActorClassPath(const AActor* Actor)
	{
		return Actor ? Actor->GetClass()->GetPathName() : FString();
	}

	static UClass* ResolveClassFromPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		if (UClass* Found = FindObject<UClass>(nullptr, *Path))
		{
			return Found;
		}
		return LoadObject<UClass>(nullptr, *Path);
	}

	static void DestroyActorsOfClass(UWorld* W, UClass* Cls)
	{
		if (!W || !Cls)
		{
			return;
		}
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			AActor* A = *It;
			if (A && A->IsA(Cls))
			{
				ToDestroy.Add(A);
			}
		}
		for (AActor* A : ToDestroy)
		{
			A->Destroy();
		}
	}
}

void AMyProjectGameMode::CaptureWorldToSave(UHonorKitchenSaveGame* Save) const
{
	if (!Save)
	{
		return;
	}
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	Save->WorldActors.Reset();

	auto AddActor = [&](AActor* A)
	{
		if (!A || A->IsA<AMyProjectCharacter>())
		{
			return;
		}
		FHonorKitchenSavedActorRecord Rec;
		Rec.ClassPath = HonorKitchenSaveHelpers::ActorClassPath(A);
		Rec.Location = A->GetActorLocation();
		Rec.Rotation = A->GetActorRotation();
		if (const APickupBase* P = Cast<APickupBase>(A))
		{
			Rec.ItemType = static_cast<uint8>(P->GetItemType());
			Rec.ItemAmount = P->GetAmount();
		}
		Save->WorldActors.Add(Rec);
	};

	for (TActorIterator<APickupBase> It(W); It; ++It)
	{
		AddActor(*It);
	}
	for (TActorIterator<ATomatoSaurusCharacter> It(W); It; ++It)
	{
		ATomatoSaurusCharacter* T = *It;
		if (T && T->GetClass() == ATomatoSaurusCharacter::StaticClass())
		{
			AddActor(T);
		}
	}
	for (TActorIterator<AKaravaychikCharacter> It(W); It; ++It)
	{
		AddActor(*It);
	}
	for (TActorIterator<AVilokhvostCharacter> It(W); It; ++It)
	{
		AddActor(*It);
	}
	for (TActorIterator<APortal> It(W); It; ++It)
	{
		AddActor(*It);
	}
	for (TActorIterator<ACrumbProjectile> It(W); It; ++It)
	{
		AddActor(*It);
	}
}

void AMyProjectGameMode::RestoreWorldFromPendingSave()
{
	if (!bPendingApplyWorldSnapshot)
	{
		return;
	}
	bPendingApplyWorldSnapshot = false;

	UWorld* W = GetWorld();
	if (!W || PendingWorldActors.Num() == 0)
	{
		PendingWorldActors.Reset();
		return;
	}

	HonorKitchenSaveHelpers::DestroyActorsOfClass(W, APickupBase::StaticClass());
	HonorKitchenSaveHelpers::DestroyActorsOfClass(W, ACrumbProjectile::StaticClass());
	HonorKitchenSaveHelpers::DestroyActorsOfClass(W, APortal::StaticClass());
	for (TActorIterator<ATomatoSaurusCharacter> It(W); It; ++It)
	{
		if (AActor* A = *It)
		{
			if (A->GetClass() == ATomatoSaurusCharacter::StaticClass())
			{
				A->Destroy();
			}
		}
	}
	HonorKitchenSaveHelpers::DestroyActorsOfClass(W, AKaravaychikCharacter::StaticClass());
	HonorKitchenSaveHelpers::DestroyActorsOfClass(W, AVilokhvostCharacter::StaticClass());

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (const FHonorKitchenSavedActorRecord& Rec : PendingWorldActors)
	{
		UClass* Cls = HonorKitchenSaveHelpers::ResolveClassFromPath(Rec.ClassPath);
		if (!Cls)
		{
			continue;
		}
		AActor* Spawned = W->SpawnActor<AActor>(Cls, Rec.Location, Rec.Rotation, Sp);
		if (APickupBase* P = Cast<APickupBase>(Spawned))
		{
			const EInventoryItemType T = static_cast<EInventoryItemType>(Rec.ItemType);
			if (T != EInventoryItemType::None && Rec.ItemAmount > 0)
			{
				P->ConfigurePickup(T, Rec.ItemAmount);
			}
		}
	}

	PendingWorldActors.Reset();

	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (AMyProjectCharacter* Char = PC->GetPawn<AMyProjectCharacter>())
		{
			if (!PendingPlayerLocation.IsNearlyZero())
			{
				Char->SetActorLocation(PendingPlayerLocation, false, nullptr, ETeleportType::TeleportPhysics);
				Char->SetActorRotation(PendingPlayerRotation);
			}
		}
	}
}

