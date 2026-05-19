// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace HonorKitchenFrontEndUI
{
	inline constexpr float SoundToggleBtnW = 220.f;
	inline constexpr float SoundToggleBtnH = 32.f;
	inline constexpr float SoundToggleMargin = 24.f;

	inline void GetSoundToggleBounds(float ClipX, float& OutX, float& OutY)
	{
		OutX = ClipX - SoundToggleBtnW - SoundToggleMargin;
		OutY = SoundToggleMargin;
	}

	inline bool HitTestSoundToggle(float MouseX, float MouseY, float ClipX)
	{
		float X = 0.f;
		float Y = 0.f;
		GetSoundToggleBounds(ClipX, X, Y);
		return MouseX >= X && MouseX <= X + SoundToggleBtnW && MouseY >= Y && MouseY <= Y + SoundToggleBtnH;
	}
}
