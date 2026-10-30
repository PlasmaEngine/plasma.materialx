#include <EditorPluginMaterialX/EditorPluginMaterialXPCH.h>

#include <EditorPluginMaterialX/EditorPluginMaterialXDLL.h>
#include <EditorPluginMaterialX/MaterialXGraphReader.h>
#include <Foundation/Configuration/Plugin.h>

// The asset generator needs no registration - plAssetDocumentGenerator::CreateGenerators finds every
// type derived from it by reflection, so loading this DLL is enough for .mtlx to become importable.
//
// This hook only reports whether the standard library shipped alongside the plugin was found, because
// the alternative is discovering it as materials that import blank.
PL_PLUGIN_ON_LOADED()
{
  plDynamicArray<plString> searchedPaths;
  const plString sLibraryPath = plMaterialXGraphReader::GetStandardLibraryPath(&searchedPaths);

  if (sLibraryPath.IsEmpty())
  {
    plStringBuilder sTried;

    for (const plString& sPath : searchedPaths)
    {
      sTried.AppendFormat("\n  {}", sPath);
    }

    plLog::Warning("MaterialX: the standard library folder was not found, so documents using standard node "
                   "definitions will not import. Looked in:{}",
      sTried);
  }
  else
  {
    plLog::Info("MaterialX standard library: '{}'", sLibraryPath);
  }
}

PL_PLUGIN_ON_UNLOADED()
{
}
