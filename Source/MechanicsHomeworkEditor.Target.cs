using UnrealBuildTool;
public class MechanicsHomeworkEditorTarget : TargetRules
{
    public MechanicsHomeworkEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new[] { "HomeworkGame", "HomeworkEditor" });
    }
}
