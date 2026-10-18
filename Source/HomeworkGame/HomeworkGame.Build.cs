using UnrealBuildTool;
public class HomeworkGame : ModuleRules
{
    public HomeworkGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "MechanicsHomework" });
    }
}
