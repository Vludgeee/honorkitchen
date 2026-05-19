// Copyright Epic Games, Inc. All Rights Reserved.

#include "HonorKitchenFrontEndMedia.h"
#include "MyProjectGameMode.h"
#include "FileMediaSource.h"
#include "IMediaEventSink.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"

namespace HonorKitchenFrontEndMediaPrivate
{
	static const TCHAR* BackgroundFileName = TEXT("HonorKitchenBackground.mp4");
	static const TCHAR* LoadingFileName = TEXT("HonorKitchenLoading.mp4");
}

void UHonorKitchenFrontEndMedia::Initialize(AMyProjectGameMode* InOwner)
{
	OwnerGameMode = InOwner;
	if (!InOwner)
	{
		return;
	}

	IntroPlayer = NewObject<UMediaPlayer>(InOwner, TEXT("IntroMediaPlayer"));
	LoadingPlayer = NewObject<UMediaPlayer>(InOwner, TEXT("LoadingMediaPlayer"));
	BackgroundPlayer = NewObject<UMediaPlayer>(InOwner, TEXT("BackgroundMediaPlayer"));

	IntroTexture = NewObject<UMediaTexture>(InOwner, TEXT("IntroMediaTexture"));
	LoadingTexture = NewObject<UMediaTexture>(InOwner, TEXT("LoadingMediaTexture"));
	BackgroundTexture = NewObject<UMediaTexture>(InOwner, TEXT("BackgroundMediaTexture"));

	if (IntroTexture && IntroPlayer)
	{
		IntroTexture->SetMediaPlayer(IntroPlayer);
		IntroTexture->UpdateResource();
	}
	if (LoadingTexture && LoadingPlayer)
	{
		LoadingTexture->SetMediaPlayer(LoadingPlayer);
		LoadingTexture->UpdateResource();
	}
	if (BackgroundTexture && BackgroundPlayer)
	{
		BackgroundTexture->SetMediaPlayer(BackgroundPlayer);
		BackgroundTexture->UpdateResource();
	}

	IntroSource = CreateFileSource(HonorKitchenFrontEndMediaPrivate::BackgroundFileName);
	LoadingSource = CreateFileSource(HonorKitchenFrontEndMediaPrivate::LoadingFileName);

	BindPlayer(IntroPlayer);
	BindPlayer(LoadingPlayer);
}

void UHonorKitchenFrontEndMedia::Shutdown()
{
	StopMainMenuBackground();
	StopLoadingVideo();
	if (IntroPlayer)
	{
		IntroPlayer->Close();
	}
	ActivePlayback = EPlaybackKind::None;
}

FString UHonorKitchenFrontEndMedia::ResolveMovieFilePath(const TCHAR* FileName) const
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies"), FileName));
}

UFileMediaSource* UHonorKitchenFrontEndMedia::CreateFileSource(const TCHAR* FileName)
{
	AMyProjectGameMode* Owner = OwnerGameMode.Get();
	if (!Owner)
	{
		return nullptr;
	}

	const FString FullPath = ResolveMovieFilePath(FileName);
	if (!FPaths::FileExists(FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("FrontEnd video missing: %s"), *FullPath);
		return nullptr;
	}

	UFileMediaSource* Source = NewObject<UFileMediaSource>(Owner);
	if (Source)
	{
		Source->SetFilePath(FullPath);
	}
	return Source;
}

bool UHonorKitchenFrontEndMedia::OpenOnPlayer(UMediaPlayer* Player, UFileMediaSource* Source, bool bLooping)
{
	if (!Player || !Source)
	{
		return false;
	}

	Player->SetLooping(bLooping);
	Player->PlayOnOpen = true;
	return Player->OpenSource(Source);
}

void UHonorKitchenFrontEndMedia::BindPlayer(UMediaPlayer* Player)
{
	if (!Player)
	{
		return;
	}

	if (Player == IntroPlayer)
	{
		Player->OnMediaEvent().AddUObject(this, &UHonorKitchenFrontEndMedia::HandleIntroMediaEvent);
		Player->OnEndReached.AddDynamic(this, &UHonorKitchenFrontEndMedia::HandleIntroEndReached);
	}
	else if (Player == LoadingPlayer)
	{
		Player->OnMediaEvent().AddUObject(this, &UHonorKitchenFrontEndMedia::HandleLoadingMediaEvent);
		Player->OnEndReached.AddDynamic(this, &UHonorKitchenFrontEndMedia::HandleLoadingEndReached);
	}
}

void UHonorKitchenFrontEndMedia::UnbindPlayer(UMediaPlayer* Player)
{
	if (Player)
	{
		Player->OnMediaEvent().RemoveAll(this);
	}
}

void UHonorKitchenFrontEndMedia::PlayIntroVideo(FHonorKitchenFrontEndMediaSimpleDelegate OnFinished)
{
	IntroFinishedDelegate = OnFinished;
	ActivePlayback = EPlaybackKind::Intro;

	if (BackgroundPlayer)
	{
		BackgroundPlayer->Close();
	}
	if (LoadingPlayer)
	{
		LoadingPlayer->Close();
	}
	if (!OpenOnPlayer(IntroPlayer, IntroSource, false))
	{
		NotifyIntroFinished();
	}
}

void UHonorKitchenFrontEndMedia::PlayLoadingVideo(FHonorKitchenFrontEndMediaSimpleDelegate OnPlaybackEnd)
{
	LoadingEndDelegate = OnPlaybackEnd;
	ActivePlayback = EPlaybackKind::Loading;

	if (IntroPlayer)
	{
		IntroPlayer->Close();
	}
	if (BackgroundPlayer)
	{
		BackgroundPlayer->Close();
	}
	if (!OpenOnPlayer(LoadingPlayer, LoadingSource, false))
	{
		NotifyLoadingPlaybackEnd();
	}
}

bool UHonorKitchenFrontEndMedia::IsMainMenuBackgroundActive() const
{
	return ActivePlayback == EPlaybackKind::MainMenuBackground
		&& BackgroundPlayer
		&& BackgroundPlayer->IsReady();
}

void UHonorKitchenFrontEndMedia::StartMainMenuBackgroundLoop()
{
	if (IsMainMenuBackgroundActive() && BackgroundPlayer->IsPlaying())
	{
		return;
	}

	ActivePlayback = EPlaybackKind::MainMenuBackground;
	if (IntroPlayer)
	{
		IntroPlayer->Close();
	}
	if (LoadingPlayer)
	{
		LoadingPlayer->Close();
	}
	OpenOnPlayer(BackgroundPlayer, IntroSource, true);
}

void UHonorKitchenFrontEndMedia::StopMainMenuBackground()
{
	if (BackgroundPlayer)
	{
		BackgroundPlayer->Close();
	}
	if (ActivePlayback == EPlaybackKind::MainMenuBackground)
	{
		ActivePlayback = EPlaybackKind::None;
	}
}

void UHonorKitchenFrontEndMedia::StopLoadingVideo()
{
	if (LoadingPlayer)
	{
		LoadingPlayer->Close();
	}
	if (ActivePlayback == EPlaybackKind::Loading)
	{
		ActivePlayback = EPlaybackKind::None;
	}
}

UMediaTexture* UHonorKitchenFrontEndMedia::GetFullscreenVideoTexture() const
{
	if (ActivePlayback == EPlaybackKind::Intro)
	{
		return IntroTexture;
	}
	if (ActivePlayback == EPlaybackKind::Loading)
	{
		return LoadingTexture;
	}
	return nullptr;
}

UMediaTexture* UHonorKitchenFrontEndMedia::GetMainMenuBackgroundTexture() const
{
	return BackgroundTexture;
}

void UHonorKitchenFrontEndMedia::HandleIntroMediaEvent(EMediaEvent Event)
{
	if (ActivePlayback != EPlaybackKind::Intro)
	{
		return;
	}

	if (Event == EMediaEvent::PlaybackEndReached)
	{
		NotifyIntroFinished();
	}
}

void UHonorKitchenFrontEndMedia::HandleLoadingMediaEvent(EMediaEvent Event)
{
	if (ActivePlayback != EPlaybackKind::Loading)
	{
		return;
	}

	if (Event == EMediaEvent::PlaybackEndReached)
	{
		NotifyLoadingPlaybackEnd();
	}
}

void UHonorKitchenFrontEndMedia::HandleIntroEndReached()
{
	if (ActivePlayback == EPlaybackKind::Intro)
	{
		NotifyIntroFinished();
	}
}

void UHonorKitchenFrontEndMedia::HandleLoadingEndReached()
{
	if (ActivePlayback == EPlaybackKind::Loading)
	{
		NotifyLoadingPlaybackEnd();
	}
}

bool UHonorKitchenFrontEndMedia::TryDetectNonLoopingPlaybackEnd(UMediaPlayer* Player) const
{
	if (!Player || Player->IsLooping() || !Player->IsReady())
	{
		return false;
	}

	const FTimespan Duration = Player->GetDuration();
	if (Duration <= FTimespan::Zero())
	{
		return false;
	}

	const FTimespan Time = Player->GetTime();
	const FTimespan EndSlack = FTimespan::FromMilliseconds(200);
	if (Time + EndSlack >= Duration)
	{
		return true;
	}

	// WMF can hold the last frame without ever firing PlaybackEndReached.
	if (!Player->IsPlaying() && Time > FTimespan::Zero() && Time + FTimespan::FromSeconds(1) >= Duration)
	{
		return true;
	}

	return false;
}

void UHonorKitchenFrontEndMedia::PollActivePlaybackEnd()
{
	switch (ActivePlayback)
	{
	case EPlaybackKind::Intro:
		if (TryDetectNonLoopingPlaybackEnd(IntroPlayer))
		{
			NotifyIntroFinished();
		}
		break;
	case EPlaybackKind::Loading:
		if (TryDetectNonLoopingPlaybackEnd(LoadingPlayer))
		{
			NotifyLoadingPlaybackEnd();
		}
		break;
	default:
		break;
	}
}

void UHonorKitchenFrontEndMedia::Tick(float DeltaTime)
{
	PollActivePlaybackEnd();
}

TStatId UHonorKitchenFrontEndMedia::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHonorKitchenFrontEndMedia, STATGROUP_Tickables);
}

bool UHonorKitchenFrontEndMedia::IsTickable() const
{
	return ActivePlayback == EPlaybackKind::Intro || ActivePlayback == EPlaybackKind::Loading;
}

void UHonorKitchenFrontEndMedia::NotifyIntroFinished()
{
	if (ActivePlayback != EPlaybackKind::Intro)
	{
		return;
	}

	ActivePlayback = EPlaybackKind::None;
	FHonorKitchenFrontEndMediaSimpleDelegate Delegate = IntroFinishedDelegate;
	IntroFinishedDelegate.Unbind();
	Delegate.ExecuteIfBound();
}

void UHonorKitchenFrontEndMedia::NotifyLoadingPlaybackEnd()
{
	if (ActivePlayback != EPlaybackKind::Loading)
	{
		return;
	}

	// Stop per-frame polling until PlayLoadingVideo restarts or StopLoadingVideo runs.
	ActivePlayback = EPlaybackKind::None;

	FHonorKitchenFrontEndMediaSimpleDelegate Delegate = LoadingEndDelegate;
	Delegate.ExecuteIfBound();
}
