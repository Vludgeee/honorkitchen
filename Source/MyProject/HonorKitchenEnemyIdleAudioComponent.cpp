// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenEnemyIdleAudioComponent.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenMonsterAudio.h"
#include "KaravaychikCharacter.h"
#include "TomatoSaurusAIController.h"
#include "TomatoSaurusCharacter.h"
#include "VilokhvostCharacter.h"
#include "MyProjectCharacter.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UHonorKitchenEnemyIdleAudioComponent::UHonorKitchenEnemyIdleAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UHonorKitchenEnemyIdleAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Имя не должно совпадать с субобъектом TEXT("EnemyIdleAudio") на том же акторе.
	IdleAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("EnemyIdleLoopPlayback"));
	if (!IdleAudioComponent)
	{
		return;
	}

	IdleAudioComponent->bAutoActivate = false;
	IdleAudioComponent->bAutoDestroy = false;
	IdleAudioComponent->bAllowSpatialization = true;
	IdleAudioComponent->bIsUISound = false;
	HonorKitchenMonsterAudio::ConfigureSpatialAudio(IdleAudioComponent);
	IdleAudioComponent->SetupAttachment(Owner->GetRootComponent());
	IdleAudioComponent->RegisterComponent();
}

EHonorKitchenEnemySpecies UHonorKitchenEnemyIdleAudioComponent::ResolveSpecies() const
{
	AActor* Owner = GetOwner();
	if (Owner && Owner->IsA(AKaravaychikCharacter::StaticClass()))
	{
		return EHonorKitchenEnemySpecies::Karavaychik;
	}
	if (Owner && Owner->IsA(AVilokhvostCharacter::StaticClass()))
	{
		return EHonorKitchenEnemySpecies::Vilokhvost;
	}
	return EHonorKitchenEnemySpecies::TomatoSaurus;
}

bool UHonorKitchenEnemyIdleAudioComponent::IsOwnerCalm() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	if (const ATomatoSaurusCharacter* Tomato = Cast<ATomatoSaurusCharacter>(Owner))
	{
		if (const ATomatoSaurusAIController* AI = Cast<ATomatoSaurusAIController>(Tomato->GetController()))
		{
			return AI->GetTomatoAIState() == ATomatoSaurusAIController::ETomatoAIState::IdlePatrol;
		}
		return true;
	}

	if (const AVilokhvostCharacter* Vilokhvost = Cast<AVilokhvostCharacter>(Owner))
	{
		return Vilokhvost->IsIdleHoveringForAmbientAudio();
	}

	return false;
}

bool UHonorKitchenEnemyIdleAudioComponent::IsPlayerInAudibleRange(float& OutDistanceSq) const
{
	OutDistanceSq = TNumericLimits<float>::Max();
	UWorld* W = GetWorld();
	AActor* Owner = GetOwner();
	if (!W || !Owner)
	{
		return false;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0);
	if (!PC)
	{
		return false;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn)
	{
		return false;
	}

	OutDistanceSq = FVector::DistSquared(Owner->GetActorLocation(), PlayerPawn->GetActorLocation());
	const float MaxDistSq = MaxAudibleDistanceUU * MaxAudibleDistanceUU;
	return OutDistanceSq <= MaxDistSq;
}

void UHonorKitchenEnemyIdleAudioComponent::StartIdleLoop()
{
	if (!IdleAudioComponent || !HonorKitchenAudioSettings::IsSoundEnabled())
	{
		return;
	}

	USoundBase* Sound = HonorKitchenEnemySoundCatalog::PickSound(ResolveSpecies(), EHonorKitchenEnemySoundEvent::Idle);

	if (!Sound)
	{
		return;
	}

	bIdleStopFadePending = false;
	IdleAudioComponent->OnAudioFinished.Clear();

	HonorKitchenMonsterAudio::ConfigureSpatialAudio(IdleAudioComponent);
	IdleAudioComponent->SetSound(Sound);
	const float ScaledVolume = HonorKitchenAudioSettings::ScaleMonsterVolume(IdleVolume);
	IdleAudioComponent->SetVolumeMultiplier(ScaledVolume);
	IdleAudioComponent->Activate();
	if (!IdleAudioComponent->IsPlaying())
	{
		IdleAudioComponent->Play();
	}
	bWasPlayingIdle = true;
}

void UHonorKitchenEnemyIdleAudioComponent::StopIdleLoop()
{
	if (!IdleAudioComponent || !IdleAudioComponent->IsPlaying())
	{
		bWasPlayingIdle = false;
		bIdleStopFadePending = false;
		return;
	}

	if (bIdleStopFadePending)
	{
		return;
	}

	bIdleStopFadePending = true;
	IdleAudioComponent->OnAudioFinished.Clear();
	IdleAudioComponent->OnAudioFinished.AddUniqueDynamic(this, &UHonorKitchenEnemyIdleAudioComponent::OnIdleLoopFadeOutFinished);
	IdleAudioComponent->FadeOut(HonorKitchenMonsterAudio::FadeOutSeconds, 0.f);
}

void UHonorKitchenEnemyIdleAudioComponent::OnIdleLoopFadeOutFinished()
{
	bWasPlayingIdle = false;
	bIdleStopFadePending = false;
	if (IdleAudioComponent)
	{
		IdleAudioComponent->OnAudioFinished.RemoveDynamic(this, &UHonorKitchenEnemyIdleAudioComponent::OnIdleLoopFadeOutFinished);
		IdleAudioComponent->Stop();
	}
}

void UHonorKitchenEnemyIdleAudioComponent::RefreshIdlePlayback()
{
	float DistSq = 0.f;
	const bool bInRange = IsPlayerInAudibleRange(DistSq);
	const bool bCalm = IsOwnerCalm();
	if (HonorKitchenAudioSettings::IsDeathStingActive())
	{
		if (bWasPlayingIdle)
		{
			StopIdleLoop();
		}
		return;
	}

	const bool bShouldPlay = bInRange && bCalm && HonorKitchenAudioSettings::IsSoundEnabled();

	if (bShouldPlay)
	{
		if (!bWasPlayingIdle)
		{
			StartIdleLoop();
		}
		else if (IdleAudioComponent && !IdleAudioComponent->IsPlaying())
		{
			StartIdleLoop();
		}
	}
	else if (bWasPlayingIdle)
	{
		StopIdleLoop();
	}
}

void UHonorKitchenEnemyIdleAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshIdlePlayback();
}
