// Copyright Epic Games, Inc. All Rights Reserved.

#include "KaravaychikCharacter.h"
#include "TomatoSaurusAIController.h"
#include "MyProjectCharacter.h"
#include "GameFramework/Pawn.h"
#include "Perception/AISense_Hearing.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "HonorKitchenAudioDefaults.h"
#include "HonorKitchenAudioSettings.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HonorKitchenEnemySpriteCatalog.h"
#include "HonorKitchenEnemySpriteComponent.h"
#include "HonorKitchenEnemySoundCatalog.h"

AKaravaychikCharacter::AKaravaychikCharacter()
{
	// Усиленный пресет обнаружения: шире конус/дальности и дольше расследование.
	SightRadiusUU = 1400.f;
	LoseSightRadiusUU = 2100.f;
	SightHalfAngleDegrees = 170.f;
	HearingRangeUU = 1700.f;
	AutoSuccessRangeFromLastSeenUU = 380.f;
	NoisePursuitSeconds = 8.f;

	IdleMoveSpeed = 150.f;
	InvestigateMoveSpeed = 200.f;
	ChaseMoveSpeed = 560.f;
	ChaseSpeedVsPlayerRatio = 0.94f;
	DamagePerTick = 15.f;
	BodyMeshScale = FVector(0.42f, 0.58f, 0.32f);

	if (EnemySprite)
	{
		EnemySprite->SpriteWorldSizeUU = 300.f;
		EnemySprite->bBillboardFaceCamera = true;
		EnemySprite->SpriteFacingYawOffset = 0.f;
		EnemySprite->SpriteFacingRollOffset = 180.f;
		EnemySprite->bFlipSpriteHorizontal = false;
		EnemySprite->bFlipSpriteVertical = false;
		EnemySprite->SetSpriteFrames(HonorKitchenEnemySpriteCatalog::MakeKaravaychickFrames());
	}
}

void AKaravaychikCharacter::BeginPlay()
{
	Super::BeginPlay();
	HonorKitchenAudioDefaults::AssignIfNull(CrunchSound, HonorKitchenAudioDefaults::GetEnemyGruntSound());
	SyncChaseSpeedToPlayer();

	if (EnemySprite)
	{
		EnemySprite->RefreshSpriteVisual();
	}

	if (!TomatoMesh || (EnemySprite && EnemySprite->IsSpriteActive()))
	{
		return;
	}

	if (UMaterialInterface* BaseShapeMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		TomatoMesh->SetMaterial(0, BaseShapeMaterial);
		if (UMaterialInstanceDynamic* Mid = TomatoMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			const FLinearColor KaravColor(0.80f, 0.66f, 0.36f, 1.f);
			Mid->SetVectorParameterValue(TEXT("Color"), KaravColor);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), KaravColor);
			Mid->SetVectorParameterValue(TEXT("EmissiveColor"), KaravColor * 0.08f);
		}
	}
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

	for (TActorIterator<ATomatoSaurusCharacter> It(W); It; ++It)
	{
		AActor* A = *It;
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

	HonorKitchenEnemySoundCatalog::PlayAt(
		this, Loc, EHonorKitchenEnemySpecies::Karavaychik, EHonorKitchenEnemySoundEvent::Detected, 1.f);
}

void AKaravaychikCharacter::ApplyKaravaychikWaterDebuff()
{
	HonorKitchenEnemySoundCatalog::PlayAt(
		this, GetActorLocation(), EHonorKitchenEnemySpecies::Karavaychik, EHonorKitchenEnemySoundEvent::Damage, 1.f);

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
