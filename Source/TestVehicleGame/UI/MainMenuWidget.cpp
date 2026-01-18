// Copyright Epic Games, Inc. All Rights Reserved.

#include "MainMenuWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click delegates
	if (Btn_Play)
	{
		Btn_Play->OnClicked.AddDynamic(this, &UMainMenuWidget::OnPlayClicked);
	}
	if (Btn_Settings)
	{
		Btn_Settings->OnClicked.AddDynamic(this, &UMainMenuWidget::OnSettingsClicked);
	}
	if (Btn_Quit)
	{
		Btn_Quit->OnClicked.AddDynamic(this, &UMainMenuWidget::OnQuitClicked);
	}
	if (Btn_FreeDrive)
	{
		Btn_FreeDrive->OnClicked.AddDynamic(this, &UMainMenuWidget::OnFreeDriveClicked);
	}
	if (Btn_TimeTrial)
	{
		Btn_TimeTrial->OnClicked.AddDynamic(this, &UMainMenuWidget::OnTimeTrialClicked);
	}
	if (Btn_Offroad)
	{
		Btn_Offroad->OnClicked.AddDynamic(this, &UMainMenuWidget::OnOffroadClicked);
	}
	if (Btn_ModeBack)
	{
		Btn_ModeBack->OnClicked.AddDynamic(this, &UMainMenuWidget::OnBackClicked);
	}
	if (Btn_Apply)
	{
		Btn_Apply->OnClicked.AddDynamic(this, &UMainMenuWidget::ApplySettings);
	}
	if (Btn_SettingsBack)
	{
		Btn_SettingsBack->OnClicked.AddDynamic(this, &UMainMenuWidget::OnBackClicked);
	}

	// Load settings on construct
	LoadSettings();
}

void UMainMenuWidget::ShowMenu()
{
	SetVisibility(ESlateVisibility::Visible);

	// Set input mode to UI only
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}

	// Pause game
	UGameplayStatics::SetGamePaused(GetWorld(), true);

	// Navigate to main screen
	NavigateToScreen(EMenuScreen::Main);
}

void UMainMenuWidget::HideMenu()
{
	SetVisibility(ESlateVisibility::Collapsed);

	// Set input mode back to game
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}

	// Unpause game
	UGameplayStatics::SetGamePaused(GetWorld(), false);
}

bool UMainMenuWidget::IsMenuVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UMainMenuWidget::NavigateToScreen(EMenuScreen Screen)
{
	CurrentScreen = Screen;

	// Switch WidgetSwitcher to appropriate index
	if (ScreenSwitcher)
	{
		ScreenSwitcher->SetActiveWidgetIndex(static_cast<int32>(Screen));
	}

	// Notify Blueprint for animations
	OnScreenChanged(Screen);
}

void UMainMenuWidget::NavigateBack()
{
	if (CurrentScreen != EMenuScreen::Main)
	{
		NavigateToScreen(EMenuScreen::Main);
	}
	else
	{
		HideMenu();
	}
}

void UMainMenuWidget::OnPlayClicked()
{
	NavigateToScreen(EMenuScreen::ModeSelect);
}

void UMainMenuWidget::OnSettingsClicked()
{
	LoadSettings();
	NavigateToScreen(EMenuScreen::Settings);
}

void UMainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(
		GetWorld(),
		GetOwningPlayer(),
		EQuitPreference::Quit,
		false
	);
}

void UMainMenuWidget::OnModeSelected(FName ModeName)
{
	// Broadcast delegate for PlayerController to handle level loading
	OnGameModeSelected.Broadcast(ModeName);

	// Hide menu (will also unpause)
	HideMenu();
}

void UMainMenuWidget::OnBackClicked()
{
	NavigateBack();
}

void UMainMenuWidget::OnFreeDriveClicked()
{
	OnModeSelected(FName("FreeDrive"));
}

void UMainMenuWidget::OnTimeTrialClicked()
{
	OnModeSelected(FName("TimeTrial"));
}

void UMainMenuWidget::OnOffroadClicked()
{
	OnModeSelected(FName("Offroad"));
}

void UMainMenuWidget::LoadSettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (Settings)
	{
		// Find current resolution index
		FIntPoint CurrentRes = Settings->GetScreenResolution();
		TArray<FIntPoint> Resolutions = GetAvailableResolutions();

		ResolutionIndex = Resolutions.IndexOfByKey(CurrentRes);
		if (ResolutionIndex == INDEX_NONE)
		{
			// Default to 1080p if current resolution not in list
			ResolutionIndex = 1;
		}
	}

	// Default volume values (could load from SaveGame in future)
	MasterVolume = 1.0f;
	MusicVolume = 1.0f;
}

void UMainMenuWidget::ApplySettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (Settings)
	{
		TArray<FIntPoint> Resolutions = GetAvailableResolutions();
		if (Resolutions.IsValidIndex(ResolutionIndex))
		{
			FIntPoint NewRes = Resolutions[ResolutionIndex];
			Settings->SetScreenResolution(NewRes);
			Settings->SetFullscreenMode(EWindowMode::Fullscreen);
			Settings->ApplySettings(false);
			Settings->SaveSettings();
		}
	}

	// Volume would be applied via SoundMix or saved to SaveGame
	// For now, just log
	UE_LOG(LogTemp, Log, TEXT("MainMenu: Applied settings - MasterVolume=%.2f, MusicVolume=%.2f, Resolution=%s"),
		   MasterVolume, MusicVolume, *GetResolutionDisplayString(ResolutionIndex));
}

void UMainMenuWidget::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UMainMenuWidget::SetMusicVolume(float Volume)
{
	MusicVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UMainMenuWidget::SetResolution(int32 Index)
{
	TArray<FIntPoint> Resolutions = GetAvailableResolutions();
	if (Resolutions.IsValidIndex(Index))
	{
		ResolutionIndex = Index;
	}
}

TArray<FIntPoint> UMainMenuWidget::GetAvailableResolutions() const
{
	return {
		FIntPoint(1280, 720),   // 720p
		FIntPoint(1920, 1080),  // 1080p
		FIntPoint(2560, 1440),  // 1440p
		FIntPoint(3840, 2160)   // 4K
	};
}

FString UMainMenuWidget::GetResolutionDisplayString(int32 Index) const
{
	TArray<FIntPoint> Resolutions = GetAvailableResolutions();
	if (Resolutions.IsValidIndex(Index))
	{
		FIntPoint Res = Resolutions[Index];
		return FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);
	}
	return TEXT("Unknown");
}
