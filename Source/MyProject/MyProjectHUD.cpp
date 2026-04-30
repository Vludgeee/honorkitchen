// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectHUD.h"
#include "MyProjectCharacter.h"
#include "MyProjectGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

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

	const float Scale = 1.1f;
	const float BarW = 280.f;
	const float BarH = 18.f;
	const float BarGap = 8.f;
	const float Margin = 32.f;
	const float H = Canvas->ClipY;

	const FLinearColor HealthFill = FLinearColor(Theme.HealthBarColor);
	const FLinearColor StaminaFill = FLinearColor(Theme.StaminaBarColor);

	AMyProjectCharacter* Char = PC->GetPawn<AMyProjectCharacter>();
	const AMyProjectGameMode* GM = GetWorld() ? Cast<AMyProjectGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
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

		// Выносливость: в геймплее отдельного ресурса пока нет — полоска 100% как визуальный слот (цвет из Theme).
		const float StaminaY = BarY + BarH + BarGap;
		const float StaminaPct = 1.f;
		DrawRect(FLinearColor(0.12f, 0.12f, 0.14f, 0.88f), BarX, StaminaY, BarW, BarH);
		DrawRect(StaminaFill, BarX, StaminaY, BarW * StaminaPct, BarH);
		DrawText(TEXT("Выносливость"), FColor(220, 225, 235), BarX + 8.f, StaminaY - 2.f, Font, Scale * 0.78f, false);

		const float BelowBarsY = StaminaY + BarH + 10.f;

		const FString CrumbLine = FString::Printf(TEXT("Крошки: %d / %d"), Char->CrumbCount, Char->MaxCrumbs);
		DrawText(CrumbLine, FColor(242, 230, 179), BarX, BelowBarsY, Font, Scale * 0.95f, false);

		if (GM)
		{
			const FString GoalLine = FString::Printf(TEXT("Батарейки: %d / %d"), GM->GetCollectedBatteries(), GM->GetRequiredBatteries());
			DrawText(GoalLine, Theme.BatteryTextColor, BarX, BelowBarsY + 25.f, Font, Scale * 0.9f, false);
		}

		if (Pct <= 0.35f)
		{
			DrawText(TEXT("ОПАСНО: мало HP"), FColor(255, 105, 105), BarX, BelowBarsY + 50.f, Font, Scale * 0.9f, false);
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

	if (GM)
	{
		const float PanelW = 305.f;
		const float PanelH = 92.f;
		const float PanelX = Canvas->ClipX - Margin - PanelW;
		const float PanelY = Margin;
		DrawRect(FLinearColor(0.08f, 0.10f, 0.16f, 0.78f), PanelX, PanelY, PanelW, PanelH);

		const FString TLine = FString::Printf(TEXT("Время: %.1fс"), GM->GetRoundElapsedSeconds());
		const FString DLine = FString::Printf(TEXT("Детекты AI: %d"), GM->GetAIDetectionCount());
		const FString ThLine = FString::Printf(TEXT("Брошено крошек: %d"), GM->GetCrumbsThrownCount());
		DrawText(TLine, FColor(205, 225, 255), PanelX + 12.f, PanelY + 10.f, Font, Scale * 0.82f, false);
		DrawText(DLine, FColor(255, 215, 145), PanelX + 12.f, PanelY + 35.f, Font, Scale * 0.82f, false);
		DrawText(ThLine, FColor(208, 255, 176), PanelX + 12.f, PanelY + 60.f, Font, Scale * 0.82f, false);
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
				DrawText(ItemText, FColor::White, X + 8.f, Y + 24.f, Font, Scale * 0.68f, false);
				DrawText(QtyText, FColor(180, 220, 255), X + SlotW - 34.f, Y + 4.f, Font, Scale * 0.62f, false);
			}
		}
	}

	const FString RestartHint = TEXT("[ R ]  Рестарт уровня");
	const float BtnW = 320.f;
	const float BtnH = 36.f;
	const float BtnX = Margin;
	const float BtnY = H - Margin - 80.f;

	DrawRect(FLinearColor(0.15f, 0.22f, 0.35f, 0.8f), BtnX, BtnY, BtnW, BtnH);
	DrawText(RestartHint, FColor::White, BtnX + 14.f, BtnY + 6.f, Font, Scale * 0.95f, false);
}
