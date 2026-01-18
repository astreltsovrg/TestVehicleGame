// Copyright Epic Games, Inc. All Rights Reserved.


#include "TestVehicleGamePlayerController.h"
#include "TestVehicleGamePawn.h"
#include "TestVehicleGameUI.h"
#include "UI/EnergyBarWidget.h"
#include "UI/MainMenuWidget.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "TestVehicleGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Widgets/Input/SVirtualJoystick.h"

void ATestVehicleGamePlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// ensure we're attached to the vehicle pawn so that World Partition streaming works correctly
	bAttachToPawn = true;

	// only spawn UI on local player controllers
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				// add the controls to the player screen
				MobileControlsWidget->AddToPlayerScreen(0);

			} else {

				UE_LOG(LogTestVehicleGame, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}
		

		// spawn the UI widget and add it to the viewport
		VehicleUI = CreateWidget<UTestVehicleGameUI>(this, VehicleUIClass);

		if (VehicleUI)
		{
			VehicleUI->AddToViewport();

		} else {

			UE_LOG(LogTestVehicleGame, Error, TEXT("Could not spawn vehicle UI widget."));

		}

		// spawn the main menu widget (hidden by default)
		if (MainMenuWidgetClass)
		{
			MainMenuWidget = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);

			if (MainMenuWidget)
			{
				MainMenuWidget->AddToViewport(100); // High ZOrder to be on top
				MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
				MainMenuWidget->OnGameModeSelected.AddDynamic(this, &ATestVehicleGamePlayerController::OnGameModeSelected);
			}
			else
			{
				UE_LOG(LogTestVehicleGame, Error, TEXT("Could not spawn main menu widget."));
			}
		}

		// spawn the energy bar widget
		if (EnergyUIClass)
		{
			EnergyUI = CreateWidget<UEnergyBarWidget>(this, EnergyUIClass);
			if (EnergyUI)
			{
				EnergyUI->AddToViewport();
			}
			else
			{
				UE_LOG(LogTestVehicleGame, Error, TEXT("Could not spawn energy bar widget."));
			}
		}
	}
}

void ATestVehicleGamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		// Bind menu toggle action
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
		{
			if (ToggleMenuAction)
			{
				EnhancedInput->BindAction(ToggleMenuAction, ETriggerEvent::Started, this, &ATestVehicleGamePlayerController::ToggleMainMenu);
			}
		}
	}
}

void ATestVehicleGamePlayerController::Tick(float Delta)
{
	Super::Tick(Delta);

	if (IsValid(VehiclePawn) && IsValid(VehicleUI))
	{
		VehicleUI->UpdateSpeed(VehiclePawn->GetChaosVehicleMovement()->GetForwardSpeed());
		VehicleUI->UpdateGear(VehiclePawn->GetChaosVehicleMovement()->GetCurrentGear());
	}

	// Update energy bar
	if (IsValid(VehiclePawn) && IsValid(EnergyUI))
	{
		EnergyUI->UpdateEnergy(VehiclePawn->GetNitroFuel(), VehiclePawn->GetMaxNitroFuel());
	}
}

void ATestVehicleGamePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// get a pointer to the controlled pawn
	VehiclePawn = CastChecked<ATestVehicleGamePawn>(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	VehiclePawn->OnDestroyed.AddDynamic(this, &ATestVehicleGamePlayerController::OnPawnDestroyed);
}

void ATestVehicleGamePlayerController::OnPawnDestroyed(AActor* DestroyedPawn)
{
	// find the player start
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		// spawn a vehicle at the player start
		const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

		if (ATestVehicleGamePawn* RespawnedVehicle = GetWorld()->SpawnActor<ATestVehicleGamePawn>(VehiclePawnClass, SpawnTransform))
		{
			// possess the vehicle
			Possess(RespawnedVehicle);
		}
	}
}

bool ATestVehicleGamePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ATestVehicleGamePlayerController::ToggleMainMenu()
{
	if (MainMenuWidget)
	{
		if (MainMenuWidget->IsMenuVisible())
		{
			MainMenuWidget->HideMenu();
		}
		else
		{
			MainMenuWidget->ShowMenu();
		}
	}
}

void ATestVehicleGamePlayerController::OnGameModeSelected(FName ModeName)
{
	// Open level based on selected mode
	FString LevelPath;

	if (ModeName == FName("FreeDrive"))
	{
		LevelPath = TEXT("/Game/VehicleTemplate/Maps/VehicleBasic");
	}
	else if (ModeName == FName("TimeTrial"))
	{
		LevelPath = TEXT("/Game/Variant_TimeTrial/Maps/Lvl_Timetrial");
	}
	else if (ModeName == FName("Offroad"))
	{
		LevelPath = TEXT("/Game/Variant_OffRoad/Maps/Lvl_Offroad");
	}

	if (!LevelPath.IsEmpty())
	{
		UGameplayStatics::OpenLevel(GetWorld(), *LevelPath);
	}
	else
	{
		UE_LOG(LogTestVehicleGame, Warning, TEXT("Unknown game mode selected: %s"), *ModeName.ToString());
	}
}
