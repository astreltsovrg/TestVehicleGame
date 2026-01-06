// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestVehicleGameWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UTestVehicleGameWheelFront::UTestVehicleGameWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}