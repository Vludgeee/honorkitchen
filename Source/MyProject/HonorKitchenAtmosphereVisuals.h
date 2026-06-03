// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AMyProjectPlayerController;
class ADynamicPostProcess;

namespace HonorKitchenAtmosphereVisuals
{
	/** Сглаженное напряжение 0..1 для пост-процесса и HUD (только локальный PC). */
	void UpdateLocalThreatVisuals(AMyProjectPlayerController* PC, float DeltaSeconds);

	ADynamicPostProcess* ResolvePostProcess(UWorld* World);
}
