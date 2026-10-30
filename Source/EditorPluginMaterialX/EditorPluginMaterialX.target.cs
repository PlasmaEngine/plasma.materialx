using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class EditorPluginMaterialXTarget : TargetRules
{
    public EditorPluginMaterialXTarget(BuildContext context) : base(context)
    {
        Type = TargetType.SharedLibrary;
        OutputName = "plEditorPluginMaterialX";
        OutputDirectory = PlasmaPackageSdk.PackageBinaryDirectory(context);

        UsePCHFiles = true;
        UseIncrementalLinking = true;

        // Unity builds are off because MaterialX is compiled into this target. Its sources open a
        // versioned namespace via macro and rely on their own include order; merging them into one
        // translation unit with the plugin's code produces name collisions that read as MaterialX bugs.
        UseUnityBuild = false;

        ExtraModules.Add("EditorPluginMaterialXModule");
        ExtraModules.Add("MaterialXModule");
    }
}
