// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectGameMode.h"
#include "MyProjectCharacter.h"
#include "MyProjectHUD.h"
#include "MyProjectPlayerController.h"
#include "KaravaychikCharacter.h"
#include "VilokhvostCharacter.h"
#include "TomatoSaurusCharacter.h"
#include "Portal.h"
#include "NavigationSystem.h"
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
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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
}

void AMyProjectGameMode::BeginPlay()
{
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
	CrumbsThrownCount = 0;
	AIDetectionCount = 0;
	RoundStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (UWorld* W = GetWorld())
	{
		bProceduralRoundGenerated = false;
		ProceduralInitAttempts = 0;
		TryGenerateProceduralRoundDeferred();
		W->GetTimerManager().SetTimer(
			KaravaychikSpawnTimerHandle,
			this,
			&AMyProjectGameMode::SpawnKaravaychikIfConfigured,
			0.15f,
			false);
		W->GetTimerManager().SetTimer(
			VilokhvostSpawnTimerHandle,
			this,
			&AMyProjectGameMode::SpawnVilokhvostIfConfigured,
			0.2f,
			false);
	}
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
	if (!PlayerPawn)
	{
		++ProceduralInitAttempts;
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				11001,
				0.3f,
				FColor::Yellow,
				FString::Printf(TEXT("ProcGen waiting pawn... attempt %d"), ProceduralInitAttempts));
		}
#endif
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

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(11002, 3.5f, FColor::Green, TEXT("ProcGen DONE"));
	}
#endif
}

void AMyProjectGameMode::GenerateProceduralRound()
{
	UWorld* W = GetWorld();
	if (!W)
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
	TArray<AActor*> Portals;
	UGameplayStatics::GetAllActorsOfClass(W, APortal::StaticClass(), Portals);

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

	if (Portals.Num() > 0)
	{
		for (AActor* A : Portals)
		{
			if (APortal* P = Cast<APortal>(A))
			{
				P->SetActorLocation(PortalLoc);
			}
		}
	}
	else
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
		TArray<AActor*> ExistingPickups;
		UGameplayStatics::GetAllActorsOfClass(W, APickupBase::StaticClass(), ExistingPickups);
		for (AActor* A : ExistingPickups)
		{
			if (A)
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

	// 3) Если TomatoSaurus уже расставлен в сцене — перенести его в процедурную точку.
	// (Vilokhvost/Karavay будут заспавнены таймерами, но TomatoSaurus, скорее всего, размещён заранее в уровне.)
	TArray<AActor*> Tomatoes;
	UGameplayStatics::GetAllActorsOfClass(W, ATomatoSaurusCharacter::StaticClass(), Tomatoes);
	for (AActor* A : Tomatoes)
	{
		if (!A)
		{
			continue;
		}

		FVector P = FallbackPoint(ItemsRadius);
		FVector Candidate;
		if (TryGetRandomPoint(ItemsRadius, 650.f, 350.f, UsedLocs, Candidate))
		{
			P = Candidate;
		}

		A->SetActorLocation(P);
		UsedLocs.Add(P);
	}

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			11003,
			4.0f,
			FColor::Cyan,
			FString::Printf(TEXT("ProcGen: portal+items done, batteries=%d"), NumBatteries));
	}
#endif
}

void AMyProjectGameMode::SpawnVilokhvostIfConfigured()
{
	if (!VilokhvostClass || VilokhvostSpawnCount <= 0 || !GetWorld())
	{
		return;
	}

	UWorld* W = GetWorld();
	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	TArray<FVector> Used;
	Used.Reserve(4);
	Used.Add(PlayerLoc);

	auto FallbackLoc = [&](int32 Index) -> FVector
	{
		return PlayerLoc + VilokhvostSpawnOffsetFromPlayer + FVector(static_cast<float>(Index) * 100.f, 0.f, 0.f);
	};

	const float SpawnRadius = 2200.f;
	const float MinDist = 650.f;
	const float MinSpacing = 450.f;

	for (int32 i = 0; i < VilokhvostSpawnCount; ++i)
	{
		FVector Loc = FallbackLoc(i);
		if (NavSys)
		{
			for (int32 Attempt = 0; Attempt < 35; ++Attempt)
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
	if (!KaravaychikClass || KaravaychikSpawnCount <= 0 || !GetWorld())
	{
		return;
	}

	UWorld* W = GetWorld();
	APlayerController* PC = W->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	TArray<FVector> Used;
	Used.Reserve(4);
	Used.Add(PlayerLoc);

	auto FallbackLoc = [&](int32 Index) -> FVector
	{
		return PlayerLoc + KaravaychikSpawnOffsetFromPlayer + FVector(static_cast<float>(Index) * 140.f, 0.f, 0.f);
	};

	const float SpawnRadius = 2400.f;
	const float MinDist = 650.f;
	const float MinSpacing = 450.f;

	for (int32 i = 0; i < KaravaychikSpawnCount; ++i)
	{
		FVector Loc = FallbackLoc(i);
		if (NavSys)
		{
			for (int32 Attempt = 0; Attempt < 35; ++Attempt)
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
		W->SpawnActor<AKaravaychikCharacter>(KaravaychikClass, Loc, FRotator::ZeroRotator, Sp);
	}
}

void AMyProjectGameMode::NotifyPlayerDied(APlayerController* PC)
{
	if (bRoundWon || !PC || !GetWorld())
	{
		return;
	}

	APawn* OldPawn = PC->GetPawn();
	PC->UnPossess();
	if (OldPawn)
	{
		OldPawn->Destroy();
	}

	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle,
		FTimerDelegate::CreateLambda([this, WeakPC = TWeakObjectPtr<APlayerController>(PC)]()
		{
			if (WeakPC.IsValid())
			{
				WeakPC->SetIgnoreMoveInput(false);
				WeakPC->SetIgnoreLookInput(false);
				RestartPlayer(WeakPC.Get());
			}
		}),
		RespawnDelaySeconds,
		false);
}

void AMyProjectGameMode::NotifyCrumbsCollected(APlayerController* PC, int32 NewTotalCrumbs)
{
	// Оставлено только для метрик/совместимости. Победа теперь через портал + батарейки.
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
	if (bRoundWon || Amount <= 0)
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
	if (CollectedBatteries < RequiredBatteries)
	{
		return false;
	}
	bRoundWon = true;
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
	}
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	return true;
}

float AMyProjectGameMode::GetRoundElapsedSeconds() const
{
	if (const UWorld* W = GetWorld())
	{
		return FMath::Max(0.f, static_cast<float>(W->GetTimeSeconds() - RoundStartTime));
	}
	return 0.f;
}
