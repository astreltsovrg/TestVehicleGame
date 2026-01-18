// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UWidgetSwitcher;
class UButton;

/** Menu screen enumeration for WidgetSwitcher navigation */
UENUM(BlueprintType)
enum class EMenuScreen : uint8
{
	Main = 0,
	ModeSelect = 1,
	Settings = 2
};

/** Delegate for game mode selection */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameModeSelected, FName, ModeName);

/**
 * Main Menu Widget - Overlay pause menu with mode selection and settings
 *
 * Uses WidgetSwitcher for navigation between screens:
 * - Main: Play, Settings, Quit
 * - ModeSelect: FreeDrive, TimeTrial, Offroad
 * - Settings: Volume sliders, Resolution
 */
UCLASS(abstract)
class TESTVEHICLEGAME_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Delegate broadcast when a game mode is selected */
	UPROPERTY(BlueprintAssignable, Category = "Menu|Events")
	FOnGameModeSelected OnGameModeSelected;

	/** Show menu and pause game */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowMenu();

	/** Hide menu and resume game */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void HideMenu();

	/** Check if menu is currently visible */
	UFUNCTION(BlueprintPure, Category = "Menu")
	bool IsMenuVisible() const;

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	//~ End UUserWidget interface

	/** Current screen for WidgetSwitcher */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	EMenuScreen CurrentScreen = EMenuScreen::Main;

	/** Reference to WidgetSwitcher (bind in Blueprint - optional to avoid compile errors) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu")
	TObjectPtr<UWidgetSwitcher> ScreenSwitcher;

	//~ Button references (bind automatically from Blueprint)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_Play;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_Quit;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_FreeDrive;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_TimeTrial;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_Offroad;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_ModeBack;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_Apply;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu|Buttons")
	TObjectPtr<UButton> Btn_SettingsBack;

	//~ Settings state
	UPROPERTY(BlueprintReadWrite, Category = "Menu|Settings")
	float MasterVolume = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Menu|Settings")
	float MusicVolume = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Menu|Settings")
	int32 ResolutionIndex = 0;

public:
	//~ Screen Navigation

	/** Navigate to specific screen */
	UFUNCTION(BlueprintCallable, Category = "Menu|Navigation")
	void NavigateToScreen(EMenuScreen Screen);

	/** Navigate back (to Main or close menu) */
	UFUNCTION(BlueprintCallable, Category = "Menu|Navigation")
	void NavigateBack();

protected:
	/** Called when screen changes - implement in Blueprint for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Navigation")
	void OnScreenChanged(EMenuScreen NewScreen);

public:
	//~ Button Handlers (call from Blueprint event bindings)

	/** Handle Play button click - navigates to mode select */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnPlayClicked();

	/** Handle Settings button click - navigates to settings */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnSettingsClicked();

	/** Handle Quit button click - exits game */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnQuitClicked();

	/** Handle mode selection - broadcasts delegate and hides menu */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnModeSelected(FName ModeName);

	/** Handle Back button click */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnBackClicked();

	/** Handle Free Drive mode selection */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnFreeDriveClicked();

	/** Handle Time Trial mode selection */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnTimeTrialClicked();

	/** Handle Offroad mode selection */
	UFUNCTION(BlueprintCallable, Category = "Menu|Actions")
	void OnOffroadClicked();

	//~ Settings

	/** Apply current settings (resolution, volume) */
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings")
	void ApplySettings();

	/** Load settings from GameUserSettings */
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings")
	void LoadSettings();

	/** Set master volume (0.0 - 1.0) */
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings")
	void SetMasterVolume(float Volume);

	/** Set music volume (0.0 - 1.0) */
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings")
	void SetMusicVolume(float Volume);

	/** Set resolution by index */
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings")
	void SetResolution(int32 Index);

	/** Get available resolutions */
	UFUNCTION(BlueprintPure, Category = "Menu|Settings")
	TArray<FIntPoint> GetAvailableResolutions() const;

	/** Get resolution as display string */
	UFUNCTION(BlueprintPure, Category = "Menu|Settings")
	FString GetResolutionDisplayString(int32 Index) const;
};
