// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace HonorKitchenSettingsUI
{
	inline float TitleY(float ClipY) { return ClipY * 0.18f; }
	inline float DevModeRowY(float ClipY) { return ClipY * 0.32f; }
	inline float MusicRowY(float ClipY) { return ClipY * 0.40f; }
	inline float MonsterRowY(float ClipY) { return ClipY * 0.48f; }
	inline float BackRowY(float ClipY) { return ClipY * 0.58f; }

	inline void GetSliderBar(float ClipX, float RowY, float& OutX, float& OutY, float& OutW, float& OutH)
	{
		OutW = FMath::Min(420.f, ClipX * 0.55f);
		OutX = ClipX * 0.5f - OutW * 0.5f;
		OutY = RowY + 22.f;
		OutH = 12.f;
	}

	inline float VolumeFromMouseX(float MouseX, float BarX, float BarW)
	{
		return FMath::Clamp((MouseX - BarX) / FMath::Max(1.f, BarW), 0.f, 1.f);
	}

	inline bool HitTestMenuRow(float MouseX, float MouseY, float ClipX, float RowY)
	{
		const float CenterX = ClipX * 0.5f;
		const float X0 = CenterX - 190.f;
		const float X1 = CenterX + 260.f;
		const float H = 28.f;
		return MouseX >= X0 && MouseX <= X1 && MouseY >= RowY - 6.f && MouseY <= RowY + H;
	}

	inline bool HitTestDevToggle(float MouseX, float MouseY, float ClipX, float ClipY)
	{
		return HitTestMenuRow(MouseX, MouseY, ClipX, DevModeRowY(ClipY));
	}

	inline bool HitTestSlider(float MouseX, float MouseY, float ClipX, float RowY)
	{
		float BarX = 0.f;
		float BarY = 0.f;
		float BarW = 0.f;
		float BarH = 0.f;
		GetSliderBar(ClipX, RowY, BarX, BarY, BarW, BarH);
		const float Pad = 10.f;
		return MouseX >= BarX - Pad && MouseX <= BarX + BarW + Pad && MouseY >= RowY - Pad && MouseY <= BarY + BarH + Pad + 18.f;
	}

	inline bool HitTestBack(float MouseX, float MouseY, float ClipX, float ClipY)
	{
		return HitTestMenuRow(MouseX, MouseY, ClipX, BackRowY(ClipY));
	}

	// Main menu row Y (fraction of viewport height)
	inline float MainMenuRowNewGame(float ClipY) { return ClipY * 0.38f; }
	inline float MainMenuRowLoad(float ClipY) { return ClipY * 0.43f; }
	inline float MainMenuRowCredits(float ClipY) { return ClipY * 0.48f; }
	inline float MainMenuRowSettings(float ClipY) { return ClipY * 0.53f; }
	inline float MainMenuRowExit(float ClipY) { return ClipY * 0.58f; }

	// Pause menu row Y
	inline float PauseRowContinue(float ClipY) { return ClipY * 0.38f; }
	inline float PauseRowSave(float ClipY) { return ClipY * 0.44f; }
	inline float PauseRowSettings(float ClipY) { return ClipY * 0.50f; }
	inline float PauseRowMainMenu(float ClipY) { return ClipY * 0.56f; }
}
