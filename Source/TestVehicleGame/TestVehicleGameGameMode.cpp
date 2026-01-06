// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestVehicleGameGameMode.h"
#include "TestVehicleGamePlayerController.h"

ATestVehicleGameGameMode::ATestVehicleGameGameMode()
{
	PlayerControllerClass = ATestVehicleGamePlayerController::StaticClass();
}
