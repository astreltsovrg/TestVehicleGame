// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_AfterburnerDOT.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

// Tag to identify entities being burned by afterburner fire
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Burning, "State.Burning");

UGE_AfterburnerDOT::UGE_AfterburnerDOT()
{
	// Infinite duration - removed when leaving fire zone
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Execute every 0.5 seconds for DOT ticks
	Period = 0.5f;

	// Execute immediately on application (first damage tick)
	bExecutePeriodicEffectOnApplication = true;

	// Note: Component setup moved to PostInitProperties()
	// Actual damage modifiers can be added here if target has Health attribute
}

void UGE_AfterburnerDOT::PostInitProperties()
{
	Super::PostInitProperties();

	// Grant "State.Burning" tag while affected
	// This can be used for visual effects, sound, and gameplay checks
	UTargetTagsGameplayEffectComponent& TargetTagsComponent = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(TAG_State_Burning);
	TargetTagsComponent.SetAndApplyTargetTagChanges(TagContainer);
}
