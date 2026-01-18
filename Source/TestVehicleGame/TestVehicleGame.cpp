// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestVehicleGame.h"
#include "Modules/ModuleManager.h"

// Native gameplay tags are now registered via UE_DEFINE_GAMEPLAY_TAG_STATIC in respective .cpp files
// This ensures they are available before CDO creation

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, TestVehicleGame, "TestVehicleGame");

DEFINE_LOG_CATEGORY(LogTestVehicleGame)