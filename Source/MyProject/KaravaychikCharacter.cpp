// Copyright Epic Games, Inc. All Rights Reserved.

#include "KaravaychikCharacter.h"
#include "TomatoSaurusAIController.h"
#include "MyProjectCharacter.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AKaravaychikCharacter::AKaravaychikCharacter()
{
	// ФТ-4.2: зрение 500 см, конус 45° (половина угла в AISense — 22.5°), слух 1000 см
	if (SightConfig)
	{
		SightConfig->SightRadius = 500.f;
		SightConfig->LoseSightRadius = 620.f;
		SightConfig->PeripheralVisionAngleDegrees = 22.5f;
	}
	if (HearingConfig)
	{
		HearingConfig->HearingRange = 1000.f;
	}
	if (PerceptionComponent)
	{
		if (SightConfig)
		{
			PerceptionComponent->ConfigureSense(*SightConfig);
		}
		if (HearingConfig)
		{
			PerceptionComponent->ConfigureSense(*HearingConfig);
		}
	}

	IdleMoveSpeed = 150.f;
	InvestigateMoveSpeed = 200.f;
	ChaseMoveSpeed = 300.f;
	DamagePerTick = 15.f;
	BodyMeshScale = FVector(0.42f, 0.58f, 0.32f);
}

void AKaravaychikCharacter::OnPlayerSightGained(AMyProjectCharacter* Player)
{
	Super::OnPlayerSightGained(Player);

	if (bHasPlayedCrunchAlert || bShoutBlockedByWater)
	{
		return;
	}
	BroadcastCrunchAlert();
	bHasPlayedCrunchAlert = true;
}

void AKaravaychikCharacter::BroadcastCrunchAlert()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	const FVector Loc = GetActorLocation();
	// UE 5.3: Loudness, Instigator, MaxRange (<=0 = по настройке слуха), Tag
	UAISense_Hearing::ReportNoiseEvent(this, Loc, CrunchNoiseLoudness, this, 0.f, FName(TEXT("KaravaychikCrunch")));

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(W, ATomatoSaurusCharacter::StaticClass(), Found);
	for (AActor* A : Found)
	{
		if (!A || A == this)
		{
			continue;
		}
		if (FVector::Dist(Loc, A->GetActorLocation()) > CrunchAlertRadiusUU)
		{
			continue;
		}
		APawn* Pawn = Cast<APawn>(A);
		if (!Pawn)
		{
			continue;
		}
		if (ATomatoSaurusAIController* AC = Cast<ATomatoSaurusAIController>(Pawn->GetController()))
		{
			AC->NotifyHeardNoise(Loc, AllyInvestigateDurationSec);
		}
	}

	if (CrunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CrunchSound, Loc);
	}
}

void AKaravaychikCharacter::ApplyKaravaychikWaterDebuff()
{
	CrowdControlSpeedMultiplier = 0.3f;
	bShoutBlockedByWater = true;
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(SaltSlowTimerHandle);
		W->GetTimerManager().ClearTimer(KaravaychikWaterTimerHandle);
		W->GetTimerManager().SetTimer(KaravaychikWaterTimerHandle, this, &AKaravaychikCharacter::ClearKaravaychikWaterDebuff, 3.f, false);
	}
	RefreshAIMovementSpeed();
}

void AKaravaychikCharacter::ClearKaravaychikWaterDebuff()
{
	bShoutBlockedByWater = false;
	CrowdControlSpeedMultiplier = 1.f;
	RefreshAIMovementSpeed();

	if (!bHasPlayedCrunchAlert && bHasSightOnPlayer)
	{
		BroadcastCrunchAlert();
		bHasPlayedCrunchAlert = true;
	}
}
