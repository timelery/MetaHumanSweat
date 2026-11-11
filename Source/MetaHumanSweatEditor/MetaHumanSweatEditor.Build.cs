using UnrealBuildTool;

public class MetaHumanSweatEditor : ModuleRules
{
	public MetaHumanSweatEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "MetaHumanSweat" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd", "MaterialEditor", "AssetTools", "AssetRegistry", "ContentBrowser", "ToolMenus", "Slate", "SlateCore",
			"Projects", "DeveloperSettings", "DeveloperToolSettings"
		});
	}
}
