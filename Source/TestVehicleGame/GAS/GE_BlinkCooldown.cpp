// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_BlinkCooldown.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTagsManager.h"

UGE_BlinkCooldown::UGE_BlinkCooldown()
{
	// Set duration to 15 seconds
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(15.0f));

	// Note: Component setup moved to PostInitProperties() as NewObject cannot be called in constructor
}

void UGE_BlinkCooldown::PostInitProperties()
{
	Super::PostInitProperties();

	// Skip for CDO and archetypes
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	// Add TargetTags component to grant Cooldown.Blink tag
	UTargetTagsGameplayEffectComponent& TargetTagsComponent = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Cooldown.Blink")));
	TargetTagsComponent.SetAndApplyTargetTagChanges(TagContainer);
}
