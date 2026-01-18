// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnergyBarWidget.generated.h"

/**
 * Energy Bar Widget - Displays current energy/nitro fuel level
 * Widget setup (ProgressBar) is handled in a Blueprint subclass.
 */
UCLASS(abstract)
class TESTVEHICLEGAME_API UEnergyBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called to update the energy display */
	void UpdateEnergy(float CurrentEnergy, float MaxEnergy);

protected:
	/** Implemented in Blueprint to update the energy bar visual */
	UFUNCTION(BlueprintImplementableEvent, Category = "Energy")
	void OnEnergyUpdate(float CurrentEnergy, float MaxEnergy, float NormalizedEnergy);
};
