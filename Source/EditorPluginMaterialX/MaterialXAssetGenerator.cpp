#include <EditorPluginMaterialX/EditorPluginMaterialXPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/EditorApp/EditorApp.h>
#include <EditorPluginAssets/MaterialAsset/MaterialAsset.h>
#include <EditorPluginAssets/Util/MeshImportUtils.h>
#include <EditorPluginMaterialX/MaterialXAssetGenerator.h>
#include <EditorPluginMaterialX/MaterialXGraphReader.h>
#include <Foundation/IO/OSFile.h>
#include <ModelImporter2/Importer/MaterialGraph.h>

// clang-format off
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plMaterialXAssetDocumentGenerator, 1, plRTTIDefaultAllocator<plMaterialXAssetDocumentGenerator>)
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

plMaterialXAssetDocumentGenerator::plMaterialXAssetDocumentGenerator()
{
  AddSupportedFileType("mtlx");
}

plMaterialXAssetDocumentGenerator::~plMaterialXAssetDocumentGenerator() = default;

void plMaterialXAssetDocumentGenerator::GetImportModes(plStringView sAbsInputFile, plDynamicArray<plAssetDocumentGenerator::ImportMode>& out_modes) const
{
  plAssetDocumentGenerator::ImportMode& mode = out_modes.ExpandAndGetRef();
  mode.m_Priority = plAssetDocGeneratorPriority::DefaultPriority;
  mode.m_sName = "MaterialXImport.Material";
  mode.m_sIcon = ":/AssetIcons/Material.svg";
}

plStatus plMaterialXAssetDocumentGenerator::Generate(plStringView sInputFileAbs, plStringView sMode, plDynamicArray<plDocument*>& out_generatedDocuments)
{
  plDynamicArray<plString> searchedPaths;
  const plString sLibraryPath = plMaterialXGraphReader::GetStandardLibraryPath(&searchedPaths);

  if (sLibraryPath.IsEmpty())
  {
    // Without the standard library a document using standard nodedefs reads as a node with no inputs,
    // which would import as a blank material rather than as a failure. Listing the directories that
    // were checked is the difference between this being a one-line fix and a reinstall.
    plStringBuilder sTried;

    for (const plString& sPath : searchedPaths)
    {
      sTried.AppendFormat("\n  {}", sPath);
    }

    plLog::Error("MaterialX standard library not found. Looked in:{}", sTried);

    return plStatus(plFmt("The MaterialX standard library was not found. Looked in:{}\n\nSet PLASMA_MATERIALX_LIBRARIES to the package's MaterialXLibraries folder, or copy that folder next to the plugin binary.", sTried));
  }

  plDynamicArray<plModelImporter2::MaterialGraphDesc> graphs;

  if (plStatus res = plMaterialXGraphReader::ReadDocument(sInputFileAbs, sLibraryPath, graphs); res.Failed())
    return res;

  plStringBuilder sTargetDirectory = sInputFileAbs;
  sTargetDirectory.PathParentDirectory();

  const plStringBuilder sSourceDirectory = sTargetDirectory;

  plStringBuilder sBaseName = plPathUtils::GetFileName(sInputFileAbs);
  plStringBuilder sDocumentPath, sSafeName;

  for (const plModelImporter2::MaterialGraphDesc& graph : graphs)
  {
    plModelImporter2::OpenPBRMaterialDesc pbr;
    plModelImporter2::MaterialGraphResolveReport report;

    if (plModelImporter2::ResolveMaterialGraph(graph, pbr, &report).Failed())
    {
      plLog::Warning("Skipping material '{}': no surface shader this importer understands.", graph.m_sName);
      continue;
    }

    // Named after the material, not the file: a document can hold several, and the name the author
    // gave the <surfacematerial> is the one they will look for. Falls back to the file name only when
    // the material is anonymous.
    plPathUtils::MakeValidFilename(graph.m_sName.IsEmpty() ? sBaseName.GetView() : graph.m_sName.GetView(), '_', sSafeName);

    sDocumentPath = sTargetDirectory;
    sDocumentPath.AppendPath(sSafeName);
    sDocumentPath.ChangeFileExtension("plMaterialAsset");
    plOSFile::FindFreeFilename(sDocumentPath);

    plDocument* pDocument = plEditorApp::GetSingleton()->CreateDocument(sDocumentPath, plDocumentFlags::None);

    if (pDocument == nullptr)
    {
      plLog::Error("Could not create material asset '{}'.", sDocumentPath);
      continue;
    }

    out_generatedDocuments.PushBack(pDocument);

    plMaterialAssetDocument* pMaterialDoc = plDynamicCast<plMaterialAssetDocument*>(pDocument);

    if (pMaterialDoc == nullptr)
      continue;

    // No importer instance: the textures a .mtlx references are files on disk, never embedded.
    plMeshImportUtils::ApplyOpenPBRMaterial(pMaterialDoc, pbr, sSourceDirectory, sTargetDirectory, nullptr);

    // The report is what makes a lossy import diagnosable. A material that quietly dropped an input
    // looks like a shading bug in the viewport and nothing else records why.
    plLog::Success("Imported MaterialX material '{}' ({}).", graph.m_sName, report.m_sMatchedSurface);

    for (const plString& sInput : report.m_UnsupportedInputs)
    {
      plLog::Warning("  '{}' kept its default: the graph driving it is more than a texture or a constant.", sInput);
    }

    for (const plString& sInput : report.m_UnmappedInputs)
    {
      plLog::Info("  '{}' has no equivalent in the engine's material.", sInput);
    }
  }

  if (out_generatedDocuments.IsEmpty())
    return plStatus(plFmt("'{}' produced no materials.", sInputFileAbs));

  return plStatus(PL_SUCCESS);
}
