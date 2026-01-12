// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TestVehicleGame : ModuleRules
{
	public TestVehicleGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"Chaos",      // For FHeightField and Chaos physics types
			"ChaosCore",  // For TAABB, TVector and other core Chaos types
			"RHI",        // For FRHIResource
			"RenderCore", // For BeginInitResource, FRenderResource
			"UMG",
			"Slate",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule"
		});

		PublicIncludePaths.AddRange(new string[] {
			"TestVehicleGame",
			"TestVehicleGame/SportsCar",
			"TestVehicleGame/OffroadCar",
			"TestVehicleGame/Variant_Offroad",
			"TestVehicleGame/Variant_TimeTrial",
			"TestVehicleGame/Variant_TimeTrial/UI",
			"TestVehicleGame/GAS",
			"TestVehicleGame/AI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
