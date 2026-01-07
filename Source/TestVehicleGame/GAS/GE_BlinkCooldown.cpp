// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_BlinkCooldown.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

// Define native gameplay tag - registered at static initialization, before CDO creation
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Blink, "Cooldown.Blink");

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

	// Add TargetTags component to grant Cooldown.Blink tag
	// Uses statically defined native tag (registered before CDO creation)
	UTargetTagsGameplayEffectComponent& TargetTagsComponent = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(TAG_Cooldown_Blink);
	TargetTagsComponent.SetAndApplyTargetTagChanges(TagContainer);
}
