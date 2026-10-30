#pragma once

#include <EditorPluginMaterialX/EditorPluginMaterialXDLL.h>
#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Types/Status.h>
#include <ModelImporter2/Importer/MaterialGraph.h>

/// \brief Reads .mtlx documents into the engine's format-neutral material graph.
///
/// MaterialX is a description language, not a shader: a document is a graph of typed nodes ending in a
/// surface shader. That is the same shape UsdShade has, so both read into plModelImporter2's
/// MaterialGraphDesc and share one resolver from there on.
///
/// Everything MaterialX throws - and it throws for every parse and lookup failure, having no error-code
/// path - is caught here and returned as a plStatus. No MaterialX type appears in this header, so the
/// rest of the plugin never sees one.
namespace plMaterialXGraphReader
{
  /// \brief Reads every material in a .mtlx file.
  ///
  /// \param sAbsFilePath The document to read.
  /// \param sSearchPath Semicolon-separated directories for resolving XInclude references and the
  ///        standard library. Pass the shipped library folder, or documents using standard nodedefs
  ///        will not resolve.
  /// \param out_graphs One entry per material with a surface shader. A document can hold several.
  PL_EDITORPLUGINMATERIALX_DLL plStatus ReadDocument(plStringView sAbsFilePath, plStringView sSearchPath,
    plDynamicArray<plModelImporter2::MaterialGraphDesc>& out_graphs);

  /// \brief Absolute path of the standard MaterialX library folder that ships with this package.
  ///
  /// Found by walking up from this DLL's own directory, so it works from a local build - where the
  /// binary sits in Bin/<config>/ and the libraries at the package root - and from an installed package
  /// alike. Empty when the folder is missing, which is worth reporting rather than letting every
  /// document silently fail to resolve its nodedefs.
  ///
  /// \param out_pSearchedPaths Optionally receives every directory that was checked, so a failure can
  ///        say where it looked instead of only that it failed.
  PL_EDITORPLUGINMATERIALX_DLL plString GetStandardLibraryPath(plDynamicArray<plString>* out_pSearchedPaths = nullptr);

} // namespace plMaterialXGraphReader
