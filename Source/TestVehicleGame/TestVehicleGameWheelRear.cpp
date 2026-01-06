// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestVehicleGameWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UTestVehicleGameWheelRear::UTestVehicleGameWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}