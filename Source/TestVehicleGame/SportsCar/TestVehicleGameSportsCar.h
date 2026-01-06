// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TestVehicleGamePawn.h"
#include "TestVehicleGameSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class ATestVehicleGameSportsCar : public ATestVehicleGamePawn
{
	GENERATED_BODY()
	
public:

	ATestVehicleGameSportsCar();
};
