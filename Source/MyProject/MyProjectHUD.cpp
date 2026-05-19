// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectHUD.h"
#include "MyProjectCharacter.h"
#include "MyProjectGameMode.h"
#include "HonorKitchenFrontEndUI.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenSettingsUI.h"
#include "HonorKitchenPickupIconCatalog.h"
#include "MediaTexture.h"
#include "Engine/Texture.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

namespace DamageScreenVignette
{
	/** Длительность HUD-вспышки (сек) — короткий импульс. */
	static constexpr float FlashDurationSeconds = 0.35f;

	static float Smoothstep(float Edge0, float Edge1, float X)
	{
		const float T = FMath::Clamp((X - Edge0) / FMath::Max(KINDA_SMALL_NUMBER, Edge1 - Edge0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** SDF скруглённого прямоугольника (внутренняя граница «окна» виньетки). */
	static float SdRoundRect(FVector2D P, FVector2D HalfExtent, float Radius)
	{
		const FVector2D Q(FMath::Abs(P.X), FMath::Abs(P.Y));
		const FVector2D Ext(HalfExtent.X, HalfExtent.Y);
		const FVector2D Inner = Q - Ext + FVector2D(Radius, Radius);
		return FVector2D(FMath::Max(Inner.X, 0.f), FMath::Max(Inner.Y, 0.f)).Size()
			+ FMath::Min(FMath::Max(Inner.X, Inner.Y), 0.f) - Radius;
	}

	static void DrawRoundedOverlay(AHUD* HUD, UCanvas* Canvas, float ScreenW, float ScreenH, float FlashAlpha)
	{
		if (!HUD || !Canvas || FlashAlpha <= 0.01f)
		{
			return;
		}

		// Яркость падает быстрее геометрии — короткий «удар», дольше — только съём рамки.
		const float ImpulseDim = FMath::Pow(FlashAlpha, 2.25f);
		const float EdgeStrength = FMath::Lerp(0.14f, 0.5f, FlashAlpha) * ImpulseDim;
		const FLinearColor EdgeRgb(0.92f, 0.06f, 0.04f, 1.f);
		const float BandFracY = FMath::Lerp(0.12f, 0.28f, FlashAlpha);
		const float BandFracX = FMath::Lerp(0.10f, 0.22f, FlashAlpha);
		const float InnerHalfX = ScreenW * (0.5f - BandFracX);
		const float InnerHalfY = ScreenH * (0.5f - BandFracY);
		const float CornerR = FMath::Min(InnerHalfX, InnerHalfY) * FMath::Lerp(0.38f, 0.58f, FlashAlpha);
		const float FeatherPx = FMath::Lerp(28.f, 80.f, FlashAlpha);
		const float Cx = ScreenW * 0.5f;
		const float Cy = ScreenH * 0.5f;

		const float Cell = FMath::Clamp(FMath::Min(ScreenW, ScreenH) / 28.f, 28.f, 52.f);
		const int32 CellsX = FMath::Clamp(FMath::CeilToInt(ScreenW / Cell), 8, 64);
		const int32 CellsY = FMath::Clamp(FMath::CeilToInt(ScreenH / Cell), 8, 64);
		const float StepX = ScreenW / static_cast<float>(CellsX);
		const float StepY = ScreenH / static_cast<float>(CellsY);
		const float StripWidth = FMath::Max(StepX, StepY);
		constexpr int32 EdgeStripCount = 3;
		const float StripZonePx = StripWidth * static_cast<float>(EdgeStripCount) + 1.f;
		constexpr float StripFadeStep = 0.078f;
		constexpr float StripFadeSpan = 0.088f;

		for (int32 Iy = 0; Iy < CellsY; ++Iy)
		{
			for (int32 Ix = 0; Ix < CellsX; ++Ix)
			{
				const float Px = (static_cast<float>(Ix) + 0.5f) * StepX;
				const float Py = (static_cast<float>(Iy) + 0.5f) * StepY;
				const FVector2D P(Px - Cx, Py - Cy);
				const float Sd = SdRoundRect(P, FVector2D(InnerHalfX, InnerHalfY), CornerR);

				const float InnerFade = Smoothstep(-FeatherPx * 0.35f, FeatherPx, Sd);
				if (InnerFade <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float Nx = FMath::Abs(P.X) / FMath::Max(1.f, ScreenW * 0.5f);
				const float Ny = FMath::Abs(P.Y) / FMath::Max(1.f, ScreenH * 0.5f);
				const float EdgeBoost = FMath::Pow(FMath::Max(Nx, Ny), 1.15f);

				float SeqStrip = 1.f;
				const float EdgeDistPx = FMath::Min(
					FMath::Min(Px, ScreenW - Px),
					FMath::Min(Py, ScreenH - Py));
				if (EdgeDistPx < StripZonePx)
				{
					const float StripIdx = EdgeDistPx / FMath::Max(1.f, StripWidth);
					const float FadeStart = StripIdx * StripFadeStep;
					SeqStrip = Smoothstep(FadeStart, FadeStart + StripFadeSpan, FlashAlpha);
				}

				const float Alpha = InnerFade * EdgeBoost * EdgeStrength * SeqStrip;
				if (Alpha <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				FLinearColor C = EdgeRgb;
				C.A = Alpha;
				HUD->DrawRect(C, Px - StepX * 0.5f, Py - StepY * 0.5f, StepX + 1.f, StepY + 1.f);
			}
		}

		HUD->DrawRect(
			FLinearColor(0.75f, 0.02f, 0.02f, ImpulseDim * 0.24f),
			0.f,
			0.f,
			ScreenW,
			ScreenH);
	}
}

void AMyProjectHUD::ToggleDebugTelemetry()
{
	if (!HonorKitchenAudioSettings::IsDeveloperMode())
	{
		bShowDebugTelemetry = false;
		return;
	}
	bShowDebugTelemetry = !bShowDebugTelemetry;
}

void AMyProjectHUD::ForceDebugTelemetryOff()
{
	bShowDebugTelemetry = false;
}

void AMyProjectHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	if (!Font)
	{
		return;
	}

	const float Scale = 1.25f;
	const float BarW = 280.f;
	const float BarH = 18.f;
	const float BarGap = 8.f;
	const float Margin = 32.f;
	const float H = Canvas->ClipY;

	const FLinearColor HealthFill = FLinearColor(Theme.HealthBarColor);
	const FLinearColor StaminaFill = FLinearColor(Theme.StaminaBarColor);

	AMyProjectCharacter* Char = PC->GetPawn<AMyProjectCharacter>();
	AMyProjectGameMode* GM = GetWorld() ? Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;

	auto DrawMediaTextureFullscreen = [&](UMediaTexture* MediaTex)
	{
		if (!MediaTex || !MediaTex->GetResource())
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
			return;
		}
		DrawTexture(
			MediaTex,
			0.f,
			0.f,
			Canvas->ClipX,
			Canvas->ClipY,
			0.f,
			0.f,
			1.f,
			1.f,
			FLinearColor::White,
			BLEND_Opaque);
	};

	auto DrawFrontEndVideoBackdrop = [&]()
	{
		if (GM && GM->ShouldDrawMainMenuBackgroundVideo())
		{
			DrawMediaTextureFullscreen(GM->GetMainMenuBackgroundVideoTexture());
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		}
	};

	auto DrawSoundToggle = [&]()
	{
		float ToggleX = 0.f;
		float ToggleY = 0.f;
		HonorKitchenFrontEndUI::GetSoundToggleBounds(Canvas->ClipX, ToggleX, ToggleY);
		const bool bSoundOn = GM->IsGameSoundEnabled();
		DrawText(
			bSoundOn ? TEXT("Звук: ВКЛ") : TEXT("Звук: ВЫКЛ"),
			bSoundOn ? FColor(220, 225, 235) : FColor(150, 155, 170),
			ToggleX,
			ToggleY,
			Font,
			Scale * 0.9f,
			false);
	};

	if (GM && GM->IsFrontEndVideoBlockingUI())
	{
		DrawMediaTextureFullscreen(GM->GetFrontEndFullscreenVideoTexture());
		return;
	}

	if (GM && GM->GetFrontEndScreen() == EFrontEndScreen::MainMenu && GM->GetBootFlow() == EHonorKitchenBootFlow::MainMenu)
	{
		DrawFrontEndVideoBackdrop();
		DrawText(TEXT("HonorKitchen"), FColor(235, 245, 255), Canvas->ClipX * 0.5f - 120.f, Canvas->ClipY * 0.22f, Font, Scale * 1.65f, false);
		DrawText(TEXT("Новая игра"), FColor(245, 245, 245), Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::MainMenuRowNewGame(Canvas->ClipY), Font, Scale * 1.15f, false);
		const bool bHasSave = GM->HasSaveGameOnDisk();
		DrawText(
			bHasSave ? TEXT("Загрузить сохранение") : TEXT("Загрузить сохранение (нет)"),
			bHasSave ? FColor(220, 225, 235) : FColor(150, 155, 170),
			Canvas->ClipX * 0.5f - 170.f,
			HonorKitchenSettingsUI::MainMenuRowLoad(Canvas->ClipY),
			Font,
			Scale * 1.15f,
			false);
		DrawText(TEXT("Авторы"), FColor(220, 225, 235), Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::MainMenuRowCredits(Canvas->ClipY), Font, Scale * 1.15f, false);
		DrawText(TEXT("Настройки"), FColor(220, 225, 235), Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::MainMenuRowSettings(Canvas->ClipY), Font, Scale * 1.15f, false);
		DrawText(TEXT("Выход"), FColor(220, 225, 235), Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::MainMenuRowExit(Canvas->ClipY), Font, Scale * 1.15f, false);
		DrawText(TEXT("(клик мышью по пункту)"), FColor(235, 235, 240), Canvas->ClipX * 0.5f - 150.f, Canvas->ClipY * 0.62f, Font, Scale * 0.9f, false);
		DrawSoundToggle();
		return;
	}
	auto DrawSettingsContent = [&]()
	{
		DrawText(TEXT("Настройки"), FColor(235, 245, 255), Canvas->ClipX * 0.5f - 90.f, HonorKitchenSettingsUI::TitleY(Canvas->ClipY), Font, Scale * 1.45f, false);

		const bool bDev = HonorKitchenAudioSettings::IsDeveloperMode();
		const FString DevLine = FString::Printf(TEXT("Режим разработчика: %s"), bDev ? TEXT("ВКЛ") : TEXT("ВЫКЛ"));
		DrawText(*DevLine, bDev ? FColor(140, 255, 170) : FColor(220, 225, 235), Canvas->ClipX * 0.5f - 220.f, HonorKitchenSettingsUI::DevModeRowY(Canvas->ClipY), Font, Scale * 1.05f, false);

		auto DrawVolumeSlider = [&](const TCHAR* Label, float Value01, float RowY)
		{
			const FString LabelLine = FString::Printf(TEXT("%s: %d%%"), Label, FMath::RoundToInt(Value01 * 100.f));
			DrawText(*LabelLine, FColor(220, 225, 235), Canvas->ClipX * 0.5f - 220.f, RowY, Font, Scale * 1.0f, false);
			float BarX = 0.f;
			float BarY = 0.f;
			float BarW = 0.f;
			float BarH = 0.f;
			HonorKitchenSettingsUI::GetSliderBar(Canvas->ClipX, RowY, BarX, BarY, BarW, BarH);
			DrawRect(FLinearColor(0.12f, 0.14f, 0.2f, 0.95f), BarX, BarY, BarW, BarH);
			const float FillW = BarW * FMath::Clamp(Value01, 0.f, 1.f);
			DrawRect(FLinearColor(0.35f, 0.55f, 0.85f, 0.95f), BarX, BarY, FillW, BarH);
			const float KnobX = BarX + FillW - 4.f;
			DrawRect(FLinearColor::White, KnobX, BarY - 3.f, 8.f, BarH + 6.f);
		};

		DrawVolumeSlider(TEXT("Громкость музыки"), HonorKitchenAudioSettings::GetMusicVolume(), HonorKitchenSettingsUI::MusicRowY(Canvas->ClipY));
		DrawVolumeSlider(TEXT("Громкость монстров"), HonorKitchenAudioSettings::GetMonsterVolume(), HonorKitchenSettingsUI::MonsterRowY(Canvas->ClipY));
		DrawText(TEXT("Назад"), FColor(245, 245, 245), Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::BackRowY(Canvas->ClipY), Font, Scale * 1.1f, false);
		DrawSoundToggle();
	};
	if (GM && GM->GetFrontEndScreen() == EFrontEndScreen::Settings)
	{
		DrawFrontEndVideoBackdrop();
		DrawSettingsContent();
		return;
	}
	if (GM && GM->IsSettingsOpen() && PC->IsPaused())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.92f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawSettingsContent();
		return;
	}
	if (GM && GM->GetFrontEndScreen() == EFrontEndScreen::Credits)
	{
		DrawFrontEndVideoBackdrop();
		DrawText(TEXT("Авторы"), FColor(235, 245, 255), Canvas->ClipX * 0.5f - 55.f, Canvas->ClipY * 0.18f, Font, Scale * 1.45f, false);
		DrawText(TEXT("Сипатров Владислав Фёдорович"), FColor(220, 225, 235), Canvas->ClipX * 0.5f - 220.f, Canvas->ClipY * 0.30f, Font, Scale * 1.05f, false);
		DrawText(TEXT("Unreal Engine 5.3 — HonorKitchen / MyProject"), FColor(220, 225, 235), Canvas->ClipX * 0.5f - 280.f, Canvas->ClipY * 0.36f, Font, Scale * 1.0f, false);
		DrawText(TEXT("Практика: ООО «СИБИНТЕК-СОФТ»"), FColor(220, 225, 235), Canvas->ClipX * 0.5f - 200.f, Canvas->ClipY * 0.42f, Font, Scale * 1.0f, false);
		DrawText(TEXT("Назад"), FColor(245, 245, 245), Canvas->ClipX * 0.5f - 170.f, Canvas->ClipY * 0.55f, Font, Scale * 1.1f, false);
		DrawSoundToggle();
		return;
	}

	if (PC->IsPaused())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.92f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawText(TEXT("Пауза"), FColor::White, Canvas->ClipX * 0.5f - 55.f, Canvas->ClipY * 0.33f, Font, Scale * 1.4f, false);
		DrawText(TEXT("Продолжить"), FColor::Silver, Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::PauseRowContinue(Canvas->ClipY), Font, Scale * 1.0f, false);
		DrawText(TEXT("Сохранить"), FColor::Silver, Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::PauseRowSave(Canvas->ClipY), Font, Scale * 1.0f, false);
		DrawText(TEXT("Настройки"), FColor::Silver, Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::PauseRowSettings(Canvas->ClipY), Font, Scale * 1.0f, false);
		DrawText(TEXT("Главное меню"), FColor::Silver, Canvas->ClipX * 0.5f - 170.f, HonorKitchenSettingsUI::PauseRowMainMenu(Canvas->ClipY), Font, Scale * 1.0f, false);
		DrawText(TEXT("(клик мышью по пункту)"), FColor::White, Canvas->ClipX * 0.5f - 150.f, Canvas->ClipY * 0.61f, Font, Scale * 0.8f, false);
		return;
	}
	if (GM)
	{
		if (GM->GetPreRoundState() == EPreRoundState::Preparing && !GM->IsFrontEndVideoBlockingUI())
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
			DrawText(TEXT("Подготовка кухни..."), FColor::White, Canvas->ClipX * 0.5f - 170.f, Canvas->ClipY * 0.45f, Font, Scale * 1.25f, false);
			return;
		}
		if (GM->GetPreRoundState() == EPreRoundState::WaitingPlayerReady)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
			DrawText(TEXT("Инициализация игрока..."), FColor::White, Canvas->ClipX * 0.5f - 170.f, Canvas->ClipY * 0.45f, Font, Scale * 1.15f, false);
			return;
		}
		if (GM->GetPreRoundState() == EPreRoundState::ReadyToStart)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
			DrawText(TEXT("Кухня готова"), FColor::Green, Canvas->ClipX * 0.5f - 120.f, Canvas->ClipY * 0.42f, Font, Scale * 1.2f, false);
			DrawText(TEXT("Нажмите Enter, чтобы начать"), FColor::White, Canvas->ClipX * 0.5f - 230.f, Canvas->ClipY * 0.48f, Font, Scale * 1.1f, false);
			return;
		}
		if (GM->GetPreRoundState() == EPreRoundState::Failed)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
			DrawText(TEXT("Ошибка генерации"), FColor::Red, Canvas->ClipX * 0.5f - 130.f, Canvas->ClipY * 0.42f, Font, Scale * 1.2f, false);
			DrawText(TEXT("Нажмите Enter для повторной генерации"), FColor::White, Canvas->ClipX * 0.5f - 300.f, Canvas->ClipY * 0.48f, Font, Scale * 1.0f, false);
			return;
		}
	}
	if (GM && GM->GetPreRoundState() == EPreRoundState::InRound && GM->IsRoundLost())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.92f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawText(TEXT("ПОРАЖЕНИЕ"), FColor::Red, Canvas->ClipX * 0.5f - 110.f, Canvas->ClipY * 0.40f, Font, Scale * 1.3f, false);
		DrawText(TEXT("Нажмите Enter для новой игры"), FColor::White, Canvas->ClipX * 0.5f - 240.f, Canvas->ClipY * 0.47f, Font, Scale * 1.0f, false);
		return;
	}
	if (GM && GM->GetPreRoundState() == EPreRoundState::InRound && GM->IsRoundWon())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.86f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawText(TEXT("ПОБЕДА"), FColor::Green, Canvas->ClipX * 0.5f - 80.f, Canvas->ClipY * 0.40f, Font, Scale * 1.3f, false);
		DrawText(TEXT("Нажмите Enter для новой игры"), FColor::White, Canvas->ClipX * 0.5f - 240.f, Canvas->ClipY * 0.47f, Font, Scale * 1.0f, false);
		return;
	}
	if (Char && !Char->IsDead())
	{
		const float Pct = FMath::Clamp(Char->MaxHealth > 0.f ? Char->CurrentHealth / Char->MaxHealth : 0.f, 0.f, 1.f);
		const float BarX = Margin;
		const float BarY = Margin;

		DrawRect(FLinearColor(0.12f, 0.12f, 0.14f, 0.88f), BarX, BarY, BarW, BarH);
		DrawRect(HealthFill, BarX, BarY, BarW * Pct, BarH);

		const FString HpLine = FString::Printf(TEXT("%.0f / %.0f"), Char->CurrentHealth, Char->MaxHealth);
		DrawText(HpLine, FColor::White, BarX + 8.f, BarY - 2.f, Font, Scale, false);

		if (UWorld* World = GetWorld())
		{
			const double Now = World->GetTimeSeconds();
			if (Now < Char->DamageHudExpiresTime)
			{
				const FString DmgLine = FString::Printf(TEXT("-%.0f"), Char->DamageHudLastAmount);
				DrawText(DmgLine, FColor(255, 115, 51), BarX + BarW + 16.f, BarY, Font, Scale * 1.1f, false);
			}
		}

		const float StaminaY = BarY + BarH + BarGap;
		const float StaminaPct = 1.f;
		DrawRect(FLinearColor(0.12f, 0.12f, 0.14f, 0.88f), BarX, StaminaY, BarW, BarH);
		DrawRect(StaminaFill, BarX, StaminaY, BarW * StaminaPct, BarH);
		DrawText(TEXT("Выносливость"), FColor(220, 225, 235), BarX + 8.f, StaminaY - 2.f, Font, Scale * 0.78f, false);

		const float BelowBarsY = StaminaY + BarH + 10.f;

		if (GM)
		{
			const FString GoalLine = FString::Printf(TEXT("Батарейки: %d / %d"), GM->GetCollectedBatteries(), GM->GetRequiredBatteries());
			DrawText(GoalLine, Theme.BatteryTextColor, BarX, BelowBarsY, Font, Scale * 0.9f, false);
		}

		if (Pct <= 0.35f)
		{
			DrawText(TEXT("ОПАСНО: мало HP"), FColor(255, 105, 105), BarX, BelowBarsY + 25.f, Font, Scale * 0.9f, false);
		}
	}

	if (GM && GM->IsRoundWon())
	{
		const FString WinLine = TEXT("ПОБЕДА! Портал активирован.");
		const float TextX = Canvas->ClipX * 0.5f - 220.f;
		const float TextY = Canvas->ClipY * 0.35f;
		DrawRect(FLinearColor(0.08f, 0.22f, 0.12f, 0.82f), TextX - 20.f, TextY - 10.f, 460.f, 54.f);
		DrawText(WinLine, FColor(120, 255, 160), TextX, TextY, Font, Scale * 1.2f, false);
	}
	else if (GM && Char && !Char->IsDead())
	{
		const float Elapsed = GM->GetRoundElapsedSeconds();
		if (Elapsed <= 14.f)
		{
			const FString IntroLine = TEXT("Собери батарейки и активируй портал клавишей E.");
			const float TipX = Canvas->ClipX * 0.5f - 290.f;
			const float TipY = Canvas->ClipY * 0.18f;
			DrawRect(FLinearColor(0.06f, 0.09f, 0.15f, 0.76f), TipX - 16.f, TipY - 8.f, 600.f, 40.f);
			DrawText(IntroLine, FColor(205, 225, 255), TipX, TipY, Font, Scale * 0.82f, false);
		}
	}

	if (GM && HonorKitchenAudioSettings::IsDeveloperMode())
	{
		const float PanelW = 305.f;
		const float PanelH = bShowDebugTelemetry ? 184.f : 92.f;
		const float PanelX = Canvas->ClipX - Margin - PanelW;
		const float PanelY = Margin;
		DrawRect(FLinearColor(0.08f, 0.10f, 0.16f, 0.78f), PanelX, PanelY, PanelW, PanelH);

		const FString TLine = FString::Printf(TEXT("Время: %.1fс"), GM->GetRoundElapsedSeconds());
		const FString DLine = FString::Printf(TEXT("Детекты AI: %d"), GM->GetAIDetectionCount());
		const FString ThLine = FString::Printf(TEXT("Брошено крошек: %d"), GM->GetCrumbsThrownCount());
		if (bShowDebugTelemetry)
		{
			const float DeltaSec = FMath::Max(KINDA_SMALL_NUMBER, GetWorld() ? GetWorld()->GetDeltaSeconds() : 1.f / 60.f);
			const float Fps = 1.f / DeltaSec;
			const float FrameMs = DeltaSec * 1000.f;
			const FString PerfLine = FString::Printf(TEXT("FPS: %.0f | Frame: %.1f ms"), Fps, FrameMs);
			const FString PrepLineA = FString::Printf(TEXT("Prep: %.2fс, attempts: %d"),
				GM->GetLastPreparationSeconds(), GM->GetLastPreparationAttempts());
			const FString PrepLineB = FString::Printf(TEXT("Prep status: %s"),
				GM->WasLastPreparationSuccessful() ? TEXT("OK") : TEXT("FAIL"));
			const FString PrepLineC = FString::Printf(TEXT("Prep fail: %s"), *GM->GetLastPreparationFailReason());
			DrawText(PerfLine, FColor(200, 245, 255), PanelX + 12.f, PanelY + 10.f, Font, Scale * 0.8f, false);
			DrawText(TLine, FColor(205, 225, 255), PanelX + 12.f, PanelY + 35.f, Font, Scale * 0.82f, false);
			DrawText(DLine, FColor(255, 215, 145), PanelX + 12.f, PanelY + 60.f, Font, Scale * 0.82f, false);
			DrawText(ThLine, FColor(208, 255, 176), PanelX + 12.f, PanelY + 85.f, Font, Scale * 0.82f, false);
			DrawText(PrepLineA, FColor(220, 220, 255), PanelX + 12.f, PanelY + 110.f, Font, Scale * 0.76f, false);
			DrawText(PrepLineB, GM->WasLastPreparationSuccessful() ? FColor::Green : FColor::Red, PanelX + 12.f, PanelY + 132.f, Font, Scale * 0.76f, false);
			DrawText(PrepLineC, FColor(200, 200, 200), PanelX + 12.f, PanelY + 150.f, Font, Scale * 0.64f, false);
		}
		else
		{
			DrawText(TLine, FColor(205, 225, 255), PanelX + 12.f, PanelY + 10.f, Font, Scale * 0.82f, false);
			DrawText(DLine, FColor(255, 215, 145), PanelX + 12.f, PanelY + 35.f, Font, Scale * 0.82f, false);
			DrawText(ThLine, FColor(208, 255, 176), PanelX + 12.f, PanelY + 60.f, Font, Scale * 0.82f, false);
		}
	}

	if (Char)
	{
		const TArray<FHotbarSlot>& Slots = Char->GetHotbarSlots();
		const int32 Active = Char->GetActiveHotbarIndex();
		const float SlotW = 88.f;
		const float SlotH = 56.f;
		const float Gap = 6.f;
		const float TotalW = SlotW * 9 + Gap * 8;
		const float StartX = Canvas->ClipX * 0.5f - TotalW * 0.5f;
		const float StartY = Canvas->ClipY - 92.f;
		for (int32 i = 0; i < 9; ++i)
		{
			const float X = StartX + i * (SlotW + Gap);
			const float Y = StartY;
			const bool bActive = (i == Active);
			DrawRect(bActive ? Theme.SlotBorderColor : FLinearColor(0.07f, 0.08f, 0.11f, 0.78f), X, Y, SlotW, SlotH);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X + 2.f, Y + 2.f, SlotW - 4.f, SlotH - 4.f);

			const FString IndexText = FString::Printf(TEXT("%d"), i + 1);
			DrawText(IndexText, FColor::Silver, X + 6.f, Y + 4.f, Font, Scale * 0.7f, false);

			if (Slots.IsValidIndex(i) && !Slots[i].IsEmpty())
			{
				if (UTexture2D* ItemIcon = HonorKitchenPickupIconCatalog::GetPickupIcon(Slots[i].ItemType))
				{
					if (HonorKitchenPickupIconCatalog::IsRenderable(ItemIcon))
					{
						const float IconSize = 32.f;
						const float IconX = X + 8.f;
						const float IconY = Y + 19.f;
						DrawTexture(ItemIcon, IconX, IconY, IconSize, IconSize, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Translucent);
					}
				}

				FString ItemText = TEXT("Предмет");
				switch (Slots[i].ItemType)
				{
				case EInventoryItemType::Crumb: ItemText = TEXT("Крошка"); break;
				case EInventoryItemType::Medkit: ItemText = TEXT("Аптечка"); break;
				case EInventoryItemType::Battery: ItemText = TEXT("Батарейка"); break;
				case EInventoryItemType::Salt: ItemText = TEXT("Соль"); break;
				case EInventoryItemType::Water: ItemText = TEXT("Вода"); break;
				case EInventoryItemType::Magnet: ItemText = TEXT("Магнит"); break;
				default: break;
				}
				const FString QtyText = FString::Printf(TEXT("x%d"), Slots[i].Amount);
				DrawText(ItemText, FColor::White, X + 42.f, Y + 24.f, Font, Scale * 0.52f, false);
				DrawText(QtyText, FColor(180, 220, 255), X + SlotW - 34.f, Y + 4.f, Font, Scale * 0.62f, false);
			}
		}
	}

	if (Char && !Char->IsDead() && !PC->IsPaused())
	{
		const float FlashAlpha = Char->GetDamageScreenFlashAlpha();
		if (FlashAlpha > 0.01f)
		{
			DamageScreenVignette::DrawRoundedOverlay(this, Canvas, Canvas->ClipX, Canvas->ClipY, FlashAlpha);
		}
	}
}


