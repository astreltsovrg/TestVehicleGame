// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_ShockwaveCooldown.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTagsManager.h"

UGE_ShockwaveCooldown::UGE_ShockwaveCooldown()
{
	// Set duration to 10 seconds
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.0f));

	// Note: Component setup moved to PostInitProperties() as NewObject cannot be called in constructor
}

void UGE_ShockwaveCooldown::PostInitProperties()
{
	Super::PostInitProperties();

	// Skip for CDO and archetypes
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	// Add TargetTags component to grant Cooldown.Shockwave tag
	UTargetTagsGameplayEffectComponent& TargetTagsComponent = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Cooldown.Shockwave")));
	TargetTagsComponent.SetAndApplyTargetTagChanges(TagContainer);
}
