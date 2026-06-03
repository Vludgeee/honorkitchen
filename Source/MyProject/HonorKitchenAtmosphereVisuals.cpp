// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenAtmosphereVisuals.h"
#include "MyProjectPlayerController.h"
#include "MyProjectCharacter.h"
#include "MyProjectGameMode.h"
#include "MyProjectHUD.h"
#include "DynamicPostProcess.h"
#include "TomatoSaurusCharacter.h"
#include "TomatoSaurusAIController.h"
#include "VilokhvostCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	float HorizontalDistSq(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	float ProximityThreat01(float DistSq, float NearUU, float FarUU)
	{
		const float NearSq = NearUU * NearUU;
		const float FarSq = FarUU * FarUU;
		if (DistSq <= NearSq)
		{
			return 1.f;
		}
		if (DistSq >= FarSq)
		{
			return 0.f;
		}
		const float Dist = FMath::Sqrt(DistSq);
		return 1.f - FMath::Clamp((Dist - NearUU) / FMath::Max(1.f, FarUU - NearUU), 0.f, 1.f);
	}

	float ThreatFromTomatoLike(const ATomatoSaurusCharacter* Enemy, const FVector& PlayerLoc)
	{
		if (!IsValid(Enemy))
		{
			return 0.f;
		}

		const float Prox = ProximityThreat01(HorizontalDistSq(Enemy->GetActorLocation(), PlayerLoc), 450.f, 2600.f);
		if (Prox <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}

		float StateWeight = 0.06f;
		if (const ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(Enemy->GetController()))
		{
			switch (AI->GetTomatoAIState())
			{
			case ATomatoSaurusAIController::ETomatoAIState::ChaseTarget:
				StateWeight = 0.88f;
				break;
			case ATomatoSaurusAIController::ETomatoAIState::InvestigateNoise:
				StateWeight = 0.42f;
				break;
			default:
				StateWeight = 0.1f;
				break;
			}
		}

		return FMath::Clamp(Prox * StateWeight, 0.f, 1.f);
	}

	float ThreatFromVilokhvost(const AVilokhvostCharacter* Vil, const FVector& PlayerLoc)
	{
		if (!IsValid(Vil))
		{
			return 0.f;
		}

		const float Prox = ProximityThreat01(HorizontalDistSq(Vil->GetActorLocation(), PlayerLoc), 350.f, 1800.f);
		const float StateWeight = Vil->GetAtmosphereThreatWeight();
		return FMath::Clamp(Prox * FMath::Max(StateWeight, 0.12f), 0.f, 1.f);
	}

	float LowHealthThreat01(const AMyProjectCharacter* Player)
	{
		if (!Player || Player->MaxHealth <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		const float Ratio = Player->CurrentHealth / Player->MaxHealth;
		if (Ratio >= 0.4f)
		{
			return 0.f;
		}
		return FMath::GetMappedRangeValueClamped(FVector2D(0.4f, 0.12f), FVector2D(0.f, 0.32f), Ratio);
	}

	bool ShouldUpdateThreatVisuals(const AMyProjectPlayerController* PC, const AMyProjectGameMode* GM, const AMyProjectCharacter* Player)
	{
		if (!PC || !GM || !Player || Player->IsDead())
		{
			return false;
		}
		if (PC->IsPaused() || GM->GetFrontEndScreen() != EFrontEndScreen::None)
		{
			return false;
		}
		if (GM->GetPreRoundState() != EPreRoundState::InRound)
		{
			return false;
		}
		if (GM->IsRoundWon() || GM->IsRoundLost() || GM->IsDeathStingActive())
		{
			return false;
		}
		return true;
	}
}

ADynamicPostProcess* HonorKitchenAtmosphereVisuals::ResolvePostProcess(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADynamicPostProcess> It(World); It; ++It)
	{
		return *It;
	}
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ADynamicPostProcess>(ADynamicPostProcess::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Sp);
}

void HonorKitchenAtmosphereVisuals::UpdateLocalThreatVisuals(AMyProjectPlayerController* PC, float DeltaSeconds)
{
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UWorld* World = PC->GetWorld();
	AMyProjectGameMode* GM = World ? Cast<AMyProjectGameMode>(World->GetAuthGameMode()) : nullptr;
	AMyProjectCharacter* Player = PC->GetPawn<AMyProjectCharacter>();

	static float SmoothedThreat01 = 0.f;
	static TWeakObjectPtr<ADynamicPostProcess> CachedPostProcess;

	float TargetThreat01 = 0.f;
	if (ShouldUpdateThreatVisuals(PC, GM, Player))
	{
		const FVector PlayerLoc = Player->GetActorLocation();
		for (TActorIterator<ATomatoSaurusCharacter> It(World); It; ++It)
		{
			TargetThreat01 = FMath::Max(TargetThreat01, ThreatFromTomatoLike(*It, PlayerLoc));
		}
		for (TActorIterator<AVilokhvostCharacter> ItV(World); ItV; ++ItV)
		{
			TargetThreat01 = FMath::Max(TargetThreat01, ThreatFromVilokhvost(*ItV, PlayerLoc));
		}
		TargetThreat01 = FMath::Clamp(FMath::Max(TargetThreat01, LowHealthThreat01(Player)), 0.f, 1.f);
	}

	const float InterpSpeed = (TargetThreat01 > SmoothedThreat01) ? 5.5f : 3.2f;
	SmoothedThreat01 = FMath::FInterpTo(SmoothedThreat01, TargetThreat01, DeltaSeconds, InterpSpeed);

	if (ADynamicPostProcess* PP = CachedPostProcess.Get())
	{
		if (!IsValid(PP))
		{
			CachedPostProcess = nullptr;
		}
	}
	if (!CachedPostProcess.IsValid())
	{
		CachedPostProcess = ResolvePostProcess(World);
	}
	if (ADynamicPostProcess* PP = CachedPostProcess.Get())
	{
		PP->SetThreatStress(SmoothedThreat01);
	}

	if (AMyProjectHUD* Hud = Cast<AMyProjectHUD>(PC->GetHUD()))
	{
		const float HudPulse = SmoothedThreat01 > 0.08f ? FMath::Pow(SmoothedThreat01, 1.35f) : 0.f;
		Hud->SetThreatScreenPulseAlpha(HudPulse);
	}
}
