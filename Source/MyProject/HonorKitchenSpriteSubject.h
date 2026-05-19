// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HonorKitchenSpriteSubject.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UHonorKitchenSpriteSubject : public UInterface
{
	GENERATED_BODY()
};

/** Состояние врага для выбора кадра 2D-спрайта (GMod-style billboard). */
class MYPROJECT_API IHonorKitchenSpriteSubject
{
	GENERATED_BODY()

public:
	virtual bool GetSpritePlayerAware() const = 0;
	virtual bool GetSpriteAttackFrame() const = 0;
	virtual bool GetSpriteChaseFrame() const = 0;
};
