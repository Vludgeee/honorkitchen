// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenPortalNavigatorComponent.h"
#include "KitchenGenerator.h"
#include "MyProjectGameMode.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "HonorKitchenAudioSettings.h"
#include "HonorKitchenDevDebug.h"

UHonorKitchenPortalNavigatorComponent::UHonorKitchenPortalNavigatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UHonorKitchenPortalNavigatorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	if (!Owner || (Pawn && !Pawn->IsLocallyControlled()))
	{
		SetComponentTickEnabled(false);
		return;
	}

	StripISM = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("PortalNavigatorStrip"));
	if (!StripISM)
	{
		return;
	}

	StripISM->SetupAttachment(Owner->GetRootComponent());
	StripISM->SetMobility(EComponentMobility::Movable);
	StripISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StripISM->SetCastShadow(false);
	StripISM->SetGenerateOverlapEvents(false);
	StripISM->bReceivesDecals = false;

	if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		StripISM->SetStaticMesh(CubeMesh);
	}

	UMaterial* BaseMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMat)
	{
		StripMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (StripMaterial)
		{
			StripMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.92f, 0.28f, 0.9f));
			StripISM->SetMaterial(0, StripMaterial);
		}
	}

	StripISM->RegisterComponent();
	StripISM->SetVisibility(false, true);
}

void UHonorKitchenPortalNavigatorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bNavigatorEnabled)
	{
		return;
	}

	PathRefreshTimer -= DeltaTime;
	if (PathRefreshTimer <= 0.f)
	{
		PathRefreshTimer = PathRefreshIntervalSeconds;
		RebuildStrip();
	}
}

void UHonorKitchenPortalNavigatorComponent::ToggleNavigator()
{
	if (!HonorKitchenAudioSettings::IsDeveloperMode())
	{
		SetNavigatorEnabled(false);
		return;
	}
	SetNavigatorEnabled(!bNavigatorEnabled);
}

void UHonorKitchenPortalNavigatorComponent::SetNavigatorEnabled(bool bEnabled)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (!HonorKitchenAudioSettings::IsDeveloperMode())
	{
		bEnabled = false;
	}

	bNavigatorEnabled = bEnabled;
	PathRefreshTimer = 0.f;

	if (StripISM)
	{
		StripISM->SetVisibility(bNavigatorEnabled, true);
		if (!bNavigatorEnabled)
		{
			StripISM->ClearInstances();
		}
	}

	if (bNavigatorEnabled)
	{
		RebuildStrip();
	}

	if (Pawn && Pawn->IsLocallyControlled())
	{
		HonorKitchenDevDebug::OnScreen(
			22010,
			3.5f,
			bNavigatorEnabled ? FColor::Green : FColor::Silver,
			bNavigatorEnabled ? TEXT("Навигатор: вкл (-)") : TEXT("Навигатор: выкл (-)"));
	}
}

AKitchenGenerator* UHonorKitchenPortalNavigatorComponent::FindKitchenGenerator() const
{
	if (const UWorld* W = GetWorld())
	{
		if (const AMyProjectGameMode* GM = Cast<AMyProjectGameMode>(W->GetAuthGameMode()))
		{
			return GM->GetKitchenGenerator();
		}
	}
	return nullptr;
}

void UHonorKitchenPortalNavigatorComponent::RebuildStrip()
{
	if (!StripISM || !bNavigatorEnabled)
	{
		return;
	}

	AActor* Owner = GetOwner();
	AKitchenGenerator* Gen = FindKitchenGenerator();
	if (!Owner || !Gen)
	{
		StripISM->ClearInstances();
		return;
	}

	TArray<FVector> PathPoints;
	if (!Gen->BuildNavigatorPathFromWorld(Owner->GetActorLocation(), PathPoints) || PathPoints.Num() < 2)
	{
		StripISM->ClearInstances();
		return;
	}

	StripISM->ClearInstances();

	const float Inv = 1.f / 100.f;
	const FVector StripScaleBase(Inv, Inv, Inv);

	for (int32 I = 0; I < PathPoints.Num() - 1; ++I)
	{
		FVector A = PathPoints[I];
		FVector B = PathPoints[I + 1];
		A.Z += FloorLiftAboveTilesUU;
		B.Z += FloorLiftAboveTilesUU;

		const FVector Delta = B - A;
		const float Len = Delta.Size2D();
		if (Len < 8.f)
		{
			continue;
		}

		const FVector Mid = (A + B) * 0.5f;
		const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		const FRotator Rot(0.f, YawDeg, 0.f);
		const FVector Scale(Len * StripScaleBase.X, StripWidthUU * StripScaleBase.Y, StripHeightUU * StripScaleBase.Z);
		StripISM->AddInstance(FTransform(Rot, Mid, Scale));
	}
}
