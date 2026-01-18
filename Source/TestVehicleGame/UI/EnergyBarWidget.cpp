// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/EnergyBarWidget.h"

void UEnergyBarWidget::UpdateEnergy(float CurrentEnergy, float MaxEnergy)
{
	float NormalizedEnergy = (MaxEnergy > 0.0f)
		? FMath::Clamp(CurrentEnergy / MaxEnergy, 0.0f, 1.0f)
		: 0.0f;

	OnEnergyUpdate(CurrentEnergy, MaxEnergy, NormalizedEnergy);
}
