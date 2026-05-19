// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HonorKitchenAudioSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

class UWorld;

/** On-screen / world overlays only when developer mode is enabled (non-shipping). */
namespace HonorKitchenDevDebug
{
	inline bool IsActive()
	{
		return HonorKitchenAudioSettings::IsDeveloperMode();
	}

	inline void OnScreen(int32 Key, float Duration, const FColor& Color, const FString& Message)
	{
#if !UE_BUILD_SHIPPING
		if (IsActive() && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(Key, Duration, Color, Message);
		}
#endif
	}

	inline void OnScreen(float Duration, const FColor& Color, const FString& Message)
	{
		OnScreen(-1, Duration, Color, Message);
	}

	inline void DrawWorldString(
		UWorld* World,
		const FVector& WorldLocation,
		const FString& Text,
		const FColor& Color,
		float HeightOffsetUU = 0.f)
	{
#if !UE_BUILD_SHIPPING
		if (!IsActive() || !World)
		{
			return;
		}
		DrawDebugString(
			World,
			WorldLocation + FVector(0.f, 0.f, HeightOffsetUU),
			Text,
			nullptr,
			Color,
			0.f,
			true);
#endif
	}

	inline void ClearOnScreenMessages()
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->ClearOnScreenDebugMessages();
		}
#endif
	}
}
