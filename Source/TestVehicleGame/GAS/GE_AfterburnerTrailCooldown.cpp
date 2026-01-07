// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_AfterburnerTrailCooldown.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

// Define native gameplay tag - registered at static initialization, before CDO creation
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_AfterburnerTrail, "Cooldown.AfterburnerTrail");

UGE_AfterburnerTrailCooldown::UGE_AfterburnerTrailCooldown()
{
	// Set duration to 5 seconds (shorter cooldown for hold ability)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));

	// Note: Component setup moved to PostInitProperties() as NewObject cannot be called in constructor
}

void UGE_AfterburnerTrailCooldown::PostInitProperties()
{
	Super::PostInitProperties();

	// Add TargetTags component to grant Cooldown.AfterburnerTrail tag
	// Uses statically defined native tag (registered before CDO creation)
	UTargetTagsGameplayEffectComponent& TargetTagsComponent = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(TAG_Cooldown_AfterburnerTrail);
	TargetTagsComponent.SetAndApplyTargetTagChanges(TagContainer);
}
