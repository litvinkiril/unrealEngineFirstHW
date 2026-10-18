using UnrealBuildTool;
public class HomeworkEditor : ModuleRules
{
    public HomeworkEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Kismet", "KismetCompiler",
            "BlueprintGraph", "AnimGraph", "AnimGraphRuntime", "EnhancedInput", "InputCore",
            "MechanicsHomework", "AssetRegistry"
        });
    }
}
