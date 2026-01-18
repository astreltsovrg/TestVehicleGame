// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_ShockwaveCooldown.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

// Define native gameplay tag - registered at static initialization, before CDO creation
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Shockwave, "Cooldown.Shockwave");

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

	// Add TargetTags component to grant Cooldown.Shockwave tag
	// Uses statically defined native tag (registered before CDO creation)
	UTargetTagsGameplayEffectComponent& TargetTagsComponent = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(TAG_Cooldown_Shockwave);
	TargetTagsComponent.SetAndApplyTargetTagChanges(TagContainer);
}
