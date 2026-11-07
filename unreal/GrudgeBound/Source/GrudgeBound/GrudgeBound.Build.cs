// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GrudgeBound : ModuleRules
{
	public GrudgeBound(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"GrudgeBound",
			"GrudgeBound/Variant_Strategy",
			"GrudgeBound/Variant_Strategy/UI",
			"GrudgeBound/Variant_TwinStick",
			"GrudgeBound/Variant_TwinStick/AI",
			"GrudgeBound/Variant_TwinStick/Gameplay",
			"GrudgeBound/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
