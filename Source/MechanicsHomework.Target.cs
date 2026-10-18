using UnrealBuildTool;
public class MechanicsHomeworkTarget : TargetRules
{
    public MechanicsHomeworkTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("HomeworkGame");
    }
}
