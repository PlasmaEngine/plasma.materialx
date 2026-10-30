#pragma once

#include <EditorFramework/Assets/AssetDocumentGenerator.h>
#include <EditorPluginMaterialX/EditorPluginMaterialXDLL.h>

/// \brief Imports a .mtlx document as one or more material assets.
///
/// Found by reflection rather than by registration: plAssetDocumentGenerator::CreateGenerators
/// enumerates every type derived from it, so declaring this class is all it takes for .mtlx to appear
/// in the asset browser and the import dialog once the plugin is loaded.
class PL_EDITORPLUGINMATERIALX_DLL plMaterialXAssetDocumentGenerator : public plAssetDocumentGenerator
{
  PL_ADD_DYNAMIC_REFLECTION(plMaterialXAssetDocumentGenerator, plAssetDocumentGenerator);

public:
  plMaterialXAssetDocumentGenerator();
  ~plMaterialXAssetDocumentGenerator();

  virtual void GetImportModes(plStringView sAbsInputFile, plDynamicArray<plAssetDocumentGenerator::ImportMode>& out_modes) const override;
  virtual plStringView GetDocumentExtension() const override { return "plMaterialAsset"; }
  virtual plStringView GetGeneratorGroup() const override { return "MaterialXImport"; }
  virtual plStatus Generate(plStringView sInputFileAbs, plStringView sMode, plDynamicArray<plDocument*>& out_generatedDocuments) override;
};
