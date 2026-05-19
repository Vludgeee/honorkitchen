// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractiveProp.h"
#include "Components/SceneComponent.h"

AInteractiveProp::AInteractiveProp()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* R = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(R);
}

void AInteractiveProp::OnInteract(AActor* Interactor)
{
}
