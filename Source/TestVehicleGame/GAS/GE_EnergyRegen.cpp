// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_EnergyRegen.h"
#include "NitroAttributeSet.h"

UGE_EnergyRegen::UGE_EnergyRegen()
{
	// Infinite duration - runs forever
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Execute every 0.5 seconds
	Period = 0.5f;

	// Don't execute on application, wait for first period
	bExecutePeriodicEffectOnApplication = false;

	// Set up the modifier to add energy
	FGameplayModifierInfo EnergyModifier;
	EnergyModifier.Attribute = UNitroAttributeSet::GetEnergyAttribute();
	EnergyModifier.ModifierOp = EGameplayModOp::Additive;
	EnergyModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f)); // +5 energy per tick

	Modifiers.Add(EnergyModifier);
}
