// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MediaPlayer.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "HonorKitchenFrontEndMedia.generated.h"

class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class AMyProjectGameMode;

DECLARE_DELEGATE(FHonorKitchenFrontEndMediaSimpleDelegate);

UCLASS()
class MYPROJECT_API UHonorKitchenFrontEndMedia : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	void Initialize(AMyProjectGameMode* InOwner);

	void PlayIntroVideo(FHonorKitchenFrontEndMediaSimpleDelegate OnFinished);
	void PlayLoadingVideo(FHonorKitchenFrontEndMediaSimpleDelegate OnPlaybackEnd);
	void StartMainMenuBackgroundLoop();
	void StopMainMenuBackground();
	bool IsMainMenuBackgroundActive() const;
	void StopLoadingVideo();
	void Shutdown();

	UMediaTexture* GetFullscreenVideoTexture() const;
	UMediaTexture* GetMainMenuBackgroundTexture() const;

	bool IsIntroPlaying() const { return ActivePlayback == EPlaybackKind::Intro; }
	bool IsLoadingPlaying() const { return ActivePlayback == EPlaybackKind::Loading; }

	// FTickableGameObject — WMF often never fires PlaybackEndReached for non-looping clips.
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }

private:
	enum class EPlaybackKind : uint8
	{
		None,
		Intro,
		Loading,
		MainMenuBackground
	};

	UPROPERTY()
	TWeakObjectPtr<AMyProjectGameMode> OwnerGameMode;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> IntroPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> LoadingPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> BackgroundPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> IntroTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> LoadingTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> BackgroundTexture;

	UPROPERTY(Transient)
	TObjectPtr<UFileMediaSource> IntroSource;

	UPROPERTY(Transient)
	TObjectPtr<UFileMediaSource> LoadingSource;

	EPlaybackKind ActivePlayback = EPlaybackKind::None;
	FHonorKitchenFrontEndMediaSimpleDelegate IntroFinishedDelegate;
	FHonorKitchenFrontEndMediaSimpleDelegate LoadingEndDelegate;

	FString ResolveMovieFilePath(const TCHAR* FileName) const;
	UFileMediaSource* CreateFileSource(const TCHAR* FileName);
	bool OpenOnPlayer(UMediaPlayer* Player, UFileMediaSource* Source, bool bLooping);
	void BindPlayer(UMediaPlayer* Player);
	void UnbindPlayer(UMediaPlayer* Player);

	void HandleIntroMediaEvent(EMediaEvent Event);
	void HandleLoadingMediaEvent(EMediaEvent Event);

	UFUNCTION()
	void HandleIntroEndReached();

	UFUNCTION()
	void HandleLoadingEndReached();

	bool TryDetectNonLoopingPlaybackEnd(UMediaPlayer* Player) const;
	void PollActivePlaybackEnd();

	void NotifyIntroFinished();
	void NotifyLoadingPlaybackEnd();
};
