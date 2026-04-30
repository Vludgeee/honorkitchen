// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectPlayerController.h"
#include "MyProjectCharacter.h"
#include "MyProjectHUD.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

AMyProjectPlayerController::AMyProjectPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMyProjectPlayerController::RestartCurrentLevel()
{
	if (UWorld* W = GetWorld())
	{
		const FString LevelName = UGameplayStatics::GetCurrentLevelName(this);
		UGameplayStatics::OpenLevel(this, FName(*LevelName));
	}
}

void AMyProjectPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController())
	{
		return;
	}

	if (WasInputKeyJustPressed(EKeys::R))
	{
		RestartCurrentLevel();
		return;
	}

	if (AMyProjectCharacter* P = GetPawn<AMyProjectCharacter>())
	{
		if (WasInputKeyJustPressed(EKeys::E))
		{
			if (!P->TryInteractPortal())
			{
				P->TryPickupNearbyItem();
			}
		}
		if (WasInputKeyJustPressed(EKeys::Q))
		{
			P->DropActiveItem();
		}
		if (WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			P->ThrowActiveItem();
		}

		if (WasInputKeyJustPressed(EKeys::One)) { P->SetActiveHotbarSlot(0); }
		else if (WasInputKeyJustPressed(EKeys::Two)) { P->SetActiveHotbarSlot(1); }
		else if (WasInputKeyJustPressed(EKeys::Three)) { P->SetActiveHotbarSlot(2); }
		else if (WasInputKeyJustPressed(EKeys::Four)) { P->SetActiveHotbarSlot(3); }
		else if (WasInputKeyJustPressed(EKeys::Five)) { P->SetActiveHotbarSlot(4); }
		else if (WasInputKeyJustPressed(EKeys::Six)) { P->SetActiveHotbarSlot(5); }
		else if (WasInputKeyJustPressed(EKeys::Seven)) { P->SetActiveHotbarSlot(6); }
		else if (WasInputKeyJustPressed(EKeys::Eight)) { P->SetActiveHotbarSlot(7); }
		else if (WasInputKeyJustPressed(EKeys::Nine)) { P->SetActiveHotbarSlot(8); }

		// Fallback: даже без Enhanced Input бросок крошки работает по клавише G.
		if (WasInputKeyJustPressed(EKeys::G))
		{
			P->TryThrowCrumb();
		}

		if (!P->IsDead() && P->GetActorLocation().Z < P->FallOffMapRestartZ)
		{
			RestartCurrentLevel();
		}
	}
}

void AMyProjectPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// BP_FirstPersonGameMode часто без HUD Class — иначе в exe нет полоски HP / рестарта.
	if (IsLocalController() && GetHUD() == nullptr)
	{
		ClientSetHUD(AMyProjectHUD::StaticClass());
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (InputMappingContext)
		{
			Subsystem->AddMappingContext(InputMappingContext, 0);
		}
	}
}