// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectPlayerController.h"
#include "MyProjectCharacter.h"
#include "MyProjectHUD.h"
#include "MyProjectGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "HonorKitchenAudioDefaults.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenFrontEndUI.h"
#include "HonorKitchenSettingsUI.h"
#include "Camera/PlayerCameraManager.h"
#include "HonorKitchenAtmosphereVisuals.h"

AMyProjectPlayerController::AMyProjectPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMyProjectPlayerController::ApplyFrontEndInputMode(bool bFrontEnd)
{
	if (bFrontEnd)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		FInputModeGameAndUI IM;
		IM.SetHideCursorDuringCapture(false);
		IM.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(IM);
		if (APlayerCameraManager* Cam = PlayerCameraManager)
		{
			Cam->StartCameraFade(0.f, 1.f, 0.f, FLinearColor::Black, false, true);
		}
	}
	else
	{
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		FInputModeGameOnly IM;
		SetInputMode(IM);
		if (APlayerCameraManager* Cam = PlayerCameraManager)
		{
			Cam->StartCameraFade(1.f, 0.f, 0.4f, FLinearColor::Black, false, false);
		}
	}
}

void AMyProjectPlayerController::PlayUiClickSound() const
{
	USoundBase* const S = UiClickSound ? UiClickSound.Get() : HonorKitchenAudioDefaults::GetUiClickSound();
	if (S)
	{
		UGameplayStatics::PlaySound2D(
			const_cast<AMyProjectPlayerController*>(this),
			S,
			HonorKitchenAudioSettings::ScaleVolume(0.75f));
	}
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

	AMyProjectGameMode* GM = GetWorld() ? Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;

	auto TryMenuClick = [&](float RowY) -> bool
	{
		float MX = 0.f, MY = 0.f;
		if (!GetMousePosition(MX, MY))
		{
			return false;
		}
		int32 SX = 0, SY = 0;
		GetViewportSize(SX, SY);
		const float CenterX = static_cast<float>(SX) * 0.5f;
		const float X0 = CenterX - 190.f;
		const float X1 = CenterX + 260.f;
		const float H = 28.f;
		return MX >= X0 && MX <= X1 && MY >= RowY - 6.f && MY <= RowY + H;
	};

	auto TryFrontEndSoundToggleClick = [&](int32 ViewSX, int32 ViewSY) -> bool
	{
		if (!GM || !WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			return false;
		}
		float MX = 0.f;
		float MY = 0.f;
		if (!GetMousePosition(MX, MY))
		{
			return false;
		}
		if (!HonorKitchenFrontEndUI::HitTestSoundToggle(MX, MY, static_cast<float>(ViewSX)))
		{
			return false;
		}
		const bool bWasEnabled = GM->IsGameSoundEnabled();
		GM->ToggleMenuSound();
		if (!bWasEnabled && GM->IsGameSoundEnabled())
		{
			PlayUiClickSound();
		}
		return true;
	};

	if (GM && GM->GetFrontEndScreen() == EFrontEndScreen::Settings)
	{
		int32 ViewSX = 0;
		int32 ViewSY = 0;
		GetViewportSize(ViewSX, ViewSY);
		if (WasInputKeyJustPressed(EKeys::Escape))
		{
			PlayUiClickSound();
			GM->CloseSettings();
			return;
		}
		ProcessSettingsInput(GM, ViewSX, ViewSY);
		return;
	}

	if (GM && GM->GetFrontEndScreen() == EFrontEndScreen::MainMenu)
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			int32 ViewSX = 0;
			int32 ViewSY = 0;
			GetViewportSize(ViewSX, ViewSY);
			if (TryFrontEndSoundToggleClick(ViewSX, ViewSY))
			{
				return;
			}
			const float ClipY = static_cast<float>(ViewSY);
			if (TryMenuClick(HonorKitchenSettingsUI::MainMenuRowNewGame(ClipY)))
			{
				PlayUiClickSound();
				GM->StartNewGameFromMenu();
			}
			else if (TryMenuClick(HonorKitchenSettingsUI::MainMenuRowLoad(ClipY)) && GM->HasSaveGameOnDisk())
			{
				PlayUiClickSound();
				GM->LoadGameFromMenu();
			}
			else if (TryMenuClick(HonorKitchenSettingsUI::MainMenuRowCredits(ClipY)))
			{
				PlayUiClickSound();
				GM->OpenCreditsFromMenu();
			}
			else if (TryMenuClick(HonorKitchenSettingsUI::MainMenuRowSettings(ClipY)))
			{
				PlayUiClickSound();
				GM->OpenSettingsFromMenu();
			}
			else if (TryMenuClick(HonorKitchenSettingsUI::MainMenuRowExit(ClipY)))
			{
				PlayUiClickSound();
				GM->QuitFromMenu();
			}
		}
		return;
	}

	if (GM && GM->GetFrontEndScreen() == EFrontEndScreen::Credits)
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton) || WasInputKeyJustPressed(EKeys::Escape))
		{
			int32 ViewSX = 0;
			int32 ViewSY = 0;
			GetViewportSize(ViewSX, ViewSY);
			if (TryFrontEndSoundToggleClick(ViewSX, ViewSY))
			{
				return;
			}
			if (WasInputKeyJustPressed(EKeys::Escape) || TryMenuClick(static_cast<float>(ViewSY) * 0.55f))
			{
				PlayUiClickSound();
				GM->CloseCreditsToMainMenu();
			}
		}
		return;
	}

	if (WasInputKeyJustPressed(EKeys::Zero) && HonorKitchenAudioSettings::IsDeveloperMode())
	{
		if (AMyProjectHUD* MyHud = Cast<AMyProjectHUD>(GetHUD()))
		{
			MyHud->ToggleDebugTelemetry();
		}
	}
	auto SetPauseInputMode = [&](bool bPaused)
	{
		if (bPaused)
		{
			bShowMouseCursor = true;
			bEnableClickEvents = true;
			bEnableMouseOverEvents = true;
			FInputModeGameAndUI IM;
			IM.SetHideCursorDuringCapture(false);
			IM.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(IM);
		}
		else
		{
			bShowMouseCursor = false;
			bEnableClickEvents = false;
			bEnableMouseOverEvents = false;
			FInputModeGameOnly IM;
			SetInputMode(IM);
		}
	};

	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		if (IsPaused())
		{
			SetPause(false);
			SetPauseInputMode(false);
			if (GM)
			{
				GM->RefreshWorldTimeFreeze();
			}
			return;
		}
		if (GM && GM->GetPreRoundState() == EPreRoundState::InRound && !GM->IsRoundWon() && !GM->IsRoundLost()
			&& !GM->IsDeathStingActive())
		{
			SetPause(true);
			SetPauseInputMode(true);
			GM->RefreshWorldTimeFreeze();
			return;
		}
	}

	if (IsPaused())
	{
		int32 ViewSX = 0;
		int32 ViewSY = 0;
		GetViewportSize(ViewSX, ViewSY);

		if (GM && GM->IsSettingsOpen())
		{
			if (WasInputKeyJustPressed(EKeys::Escape))
			{
				PlayUiClickSound();
				GM->CloseSettings();
				return;
			}
			ProcessSettingsInput(GM, ViewSX, ViewSY);
			return;
		}

		if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			float MX = 0.f;
			float MY = 0.f;
			if (GetMousePosition(MX, MY))
			{
				const float ClipY = static_cast<float>(ViewSY);
				const float ClipX = static_cast<float>(ViewSX);

				if (HonorKitchenSettingsUI::HitTestMenuRow(MX, MY, ClipX, HonorKitchenSettingsUI::PauseRowContinue(ClipY)))
				{
					PlayUiClickSound();
					SetPause(false);
					SetPauseInputMode(false);
					if (GM)
					{
						GM->RefreshWorldTimeFreeze();
					}
					return;
				}
				if (HonorKitchenSettingsUI::HitTestMenuRow(MX, MY, ClipX, HonorKitchenSettingsUI::PauseRowSave(ClipY)) && GM)
				{
					PlayUiClickSound();
					GM->TryWriteSaveGame();
					return;
				}
				if (HonorKitchenSettingsUI::HitTestMenuRow(MX, MY, ClipX, HonorKitchenSettingsUI::PauseRowSettings(ClipY)) && GM)
				{
					PlayUiClickSound();
					GM->OpenSettingsFromPause();
					return;
				}
				if (HonorKitchenSettingsUI::HitTestMenuRow(MX, MY, ClipX, HonorKitchenSettingsUI::PauseRowMainMenu(ClipY)) && GM)
				{
					PlayUiClickSound();
					SetPause(false);
					SetPauseInputMode(false);
					GM->RefreshWorldTimeFreeze();
					GM->ReturnToMainMenu();
					return;
				}
			}
		}

		return;
	}

	if (WasInputKeyJustPressed(EKeys::Enter))
	{
		if (GM)
		{
			if (GM->GetPreRoundState() == EPreRoundState::InRound
				&& (GM->IsRoundWon() || (GM->IsRoundLost() && !GM->IsDeathStingActive())))
			{
				GM->RequestNewRound();
			}
			else
			{
				GM->HandlePreRoundEnterPressed();
			}
			return;
		}
	}

	if (GM && GM->GetPreRoundState() != EPreRoundState::InRound)
	{
		return;
	}

	if (GM && GM->IsDeathStingActive())
	{
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

		if (HonorKitchenAudioSettings::IsDeveloperMode())
		{
			if (WasInputKeyJustPressed(EKeys::Hyphen) || WasInputKeyJustPressed(EKeys::Subtract))
			{
				P->TogglePortalNavigator();
			}

			if (WasInputKeyJustPressed(EKeys::Equals))
			{
				P->GrantTestLoadout();
			}
		}
		if (HonorKitchenAudioSettings::IsDeveloperMode())
		{
			if (WasInputKeyJustPressed(EKeys::NumPadOne))
			{
				P->GrantTestItem(EInventoryItemType::Medkit, 3);
			}
			else if (WasInputKeyJustPressed(EKeys::NumPadTwo))
			{
				P->GrantTestItem(EInventoryItemType::Battery, 3);
			}
			else if (WasInputKeyJustPressed(EKeys::NumPadThree))
			{
				P->GrantTestItem(EInventoryItemType::Water, 3);
			}
			else if (WasInputKeyJustPressed(EKeys::NumPadFour))
			{
				P->GrantTestItem(EInventoryItemType::Crumb, 3);
			}
			else if (WasInputKeyJustPressed(EKeys::NumPadFive))
			{
				P->GrantTestItem(EInventoryItemType::Salt, 3);
			}
		}
		if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			P->CycleActiveHotbarSlot(1);
		}
		else if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			P->CycleActiveHotbarSlot(-1);
		}

		if (!P->IsDead() && P->GetActorLocation().Z < P->FallOffMapRestartZ)
		{
			RestartCurrentLevel();
		}
	}

	HonorKitchenAtmosphereVisuals::UpdateLocalThreatVisuals(this, DeltaSeconds);
}

void AMyProjectPlayerController::BeginPlay()
{
	Super::BeginPlay();
	HonorKitchenAudioDefaults::AssignIfNull(UiClickSound, HonorKitchenAudioDefaults::GetUiClickSound());

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

	if (AMyProjectGameMode* GM = GetWorld() ? Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		if (GM->GetFrontEndScreen() != EFrontEndScreen::None)
		{
			ApplyFrontEndInputMode(true);
		}
		else
		{
			ApplyFrontEndInputMode(false);
		}
	}
	else
	{
		ApplyFrontEndInputMode(false);
	}

#if !UE_BUILD_SHIPPING
	if (GEngine && IsLocalController() && HonorKitchenAudioSettings::IsDeveloperMode())
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			7.0f,
			FColor::Cyan,
			TEXT("Навигатор к порталу: '-' | Тест-предметы: '=' | NumPad1-5"));
	}
#endif

	HonorKitchenAudioSettings::Load();
	if (AMyProjectGameMode* StartGM = GetWorld() ? Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		StartGM->OnPlayerSettingsChanged();
	}
}

void AMyProjectPlayerController::ProcessSettingsInput(AMyProjectGameMode* GM, int32 ViewSX, int32 ViewSY)
{
	if (!GM)
	{
		return;
	}

	const float ClipX = static_cast<float>(ViewSX);
	const float ClipY = static_cast<float>(ViewSY);

	float MX = 0.f;
	float MY = 0.f;
	const bool bHasMouse = GetMousePosition(MX, MY);
	const bool bLmbDown = IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bLmbPressed = WasInputKeyJustPressed(EKeys::LeftMouseButton);
	const bool bLmbReleased = WasInputKeyJustReleased(EKeys::LeftMouseButton);

	if (bLmbReleased)
	{
		bSettingsDraggingMusic = false;
		bSettingsDraggingMonster = false;
	}

	if (bHasMouse && bLmbPressed)
	{
		if (HonorKitchenFrontEndUI::HitTestSoundToggle(MX, MY, ClipX))
		{
			const bool bWasEnabled = GM->IsGameSoundEnabled();
			GM->ToggleMenuSound();
			if (!bWasEnabled && GM->IsGameSoundEnabled())
			{
				PlayUiClickSound();
			}
			return;
		}
		if (HonorKitchenSettingsUI::HitTestBack(MX, MY, ClipX, ClipY))
		{
			PlayUiClickSound();
			GM->CloseSettings();
			return;
		}
		if (HonorKitchenSettingsUI::HitTestDevToggle(MX, MY, ClipX, ClipY))
		{
			PlayUiClickSound();
			HonorKitchenAudioSettings::ToggleDeveloperMode();
			GM->OnPlayerSettingsChanged();
			return;
		}
		if (HonorKitchenSettingsUI::HitTestSlider(MX, MY, ClipX, HonorKitchenSettingsUI::MusicRowY(ClipY)))
		{
			bSettingsDraggingMusic = true;
			bSettingsDraggingMonster = false;
		}
		else if (HonorKitchenSettingsUI::HitTestSlider(MX, MY, ClipX, HonorKitchenSettingsUI::MonsterRowY(ClipY)))
		{
			bSettingsDraggingMonster = true;
			bSettingsDraggingMusic = false;
		}
	}

	if (bHasMouse && bLmbDown && (bSettingsDraggingMusic || bSettingsDraggingMonster))
	{
		float BarX = 0.f;
		float BarY = 0.f;
		float BarW = 0.f;
		float BarH = 0.f;
		if (bSettingsDraggingMusic)
		{
			HonorKitchenSettingsUI::GetSliderBar(ClipX, HonorKitchenSettingsUI::MusicRowY(ClipY), BarX, BarY, BarW, BarH);
			HonorKitchenAudioSettings::SetMusicVolume(HonorKitchenSettingsUI::VolumeFromMouseX(MX, BarX, BarW));
			GM->OnPlayerSettingsChanged();
		}
		else if (bSettingsDraggingMonster)
		{
			HonorKitchenSettingsUI::GetSliderBar(ClipX, HonorKitchenSettingsUI::MonsterRowY(ClipY), BarX, BarY, BarW, BarH);
			HonorKitchenAudioSettings::SetMonsterVolume(HonorKitchenSettingsUI::VolumeFromMouseX(MX, BarX, BarW));
			GM->OnPlayerSettingsChanged();
		}
	}
}