// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_EnergyRegen.generated.h"

/**
 * Energy Regeneration GameplayEffect.
 * Periodically restores energy to the vehicle.
 * Applied on spawn and runs indefinitely.
 */
UCLASS()
class TESTVEHICLEGAME_API UGE_EnergyRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_EnergyRegen();
};
