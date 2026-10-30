using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class EditorPluginMaterialXModule : ModuleRules
{
    public EditorPluginMaterialXModule(BuildContext context) : base(context)
    {
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Source/EditorPluginMaterialX",
            "EditorPluginMaterialXPCH.h", "BUILDSYSTEM_BUILDING_EDITORPLUGINMATERIALX_LIB");

        // Brings in EditorFramework, ToolsFoundation and - the part this plugin actually needs -
        // ModelImporter2, which owns MaterialGraphDesc and the OpenPBR parameter set.
        PlasmaPackageSdk.ConfigureEditorSdkConsumer(this, context);

        // plMaterialAssetDocument and plMeshImportUtils::ApplyOpenPBRMaterial live in the Assets editor
        // plugin. In-tree the plugin group's parent directory is on the include path automatically;
        // out here it has to be asked for, along with the import library.
        PlasmaPackageSdk.ConfigureAssetsBuiltIn(this, context);

        PublicDependencies.Add("MaterialXModule");

        // MaterialX throws on every parse failure, so the boundary that catches those has to be able
        // to unwind. Everything is caught in MaterialXGraphReader and turned into a plStatus.
        EnableExceptions = true;
    }
}
