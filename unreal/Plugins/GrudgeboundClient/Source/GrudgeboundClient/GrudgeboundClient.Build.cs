using UnrealBuildTool;

public class GrudgeboundClient : ModuleRules
{
	public GrudgeboundClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Platform == UnrealTargetPlatform.Win64) PublicSystemLibraries.Add("Crypt32.lib");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings",
			"UMG",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",
			"Json",
			"Slate",
			"SlateCore",
		});
	}
}
