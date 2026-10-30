#include <EditorPluginMaterialX/EditorPluginMaterialXPCH.h>

#include <EditorPluginMaterialX/MaterialXGraphReader.h>
#include <Foundation/Containers/Set.h>
#include <Foundation/IO/OSFile.h>
#include <Foundation/Strings/StringConversion.h>
#include <Foundation/System/EnvironmentVariableUtils.h>

#if PL_ENABLED(PL_PLATFORM_WINDOWS)
#  include <Foundation/Basics/Platform/Win/IncludeWindows.h>
#endif

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Material.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXFormat/XmlIo.h>

namespace mx = MaterialX;

namespace plMaterialXGraphReader
{
  namespace
  {
    /// The directory this DLL was actually loaded from.
    ///
    /// Taken from the module that contains this function's own address, so it is where this code is
    /// rather than where a plugin of this name might be found - the two differ for anything loaded from
    /// outside the application directory, which is every package.
    plString GetOwnModuleDirectory()
    {
#if PL_ENABLED(PL_PLATFORM_WINDOWS)
      HMODULE hModule = nullptr;

      if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetOwnModuleDirectory), &hModule) == 0)
      {
        return plString();
      }

      wchar_t szPath[1024] = {};
      const DWORD uiLength = ::GetModuleFileNameW(hModule, szPath, PL_ARRAY_SIZE(szPath));

      if (uiLength == 0 || uiLength >= PL_ARRAY_SIZE(szPath))
        return plString();

      plStringBuilder sPath = plStringUtf8(szPath).GetView();
      sPath.PathParentDirectory();
      sPath.MakeCleanPath();
      return sPath;
#else
      return plString();
#endif
    }

    plVariant ToVariant(const mx::ValuePtr& pValue)
    {
      if (pValue == nullptr)
        return plVariant();

      if (pValue->isA<float>())
        return plVariant(pValue->asA<float>());

      if (pValue->isA<int>())
        return plVariant(pValue->asA<int>());

      if (pValue->isA<bool>())
        return plVariant(pValue->asA<bool>());

      if (pValue->isA<mx::Color3>())
      {
        const mx::Color3 c = pValue->asA<mx::Color3>();
        return plVariant(plColor(c[0], c[1], c[2]));
      }

      if (pValue->isA<mx::Color4>())
      {
        const mx::Color4 c = pValue->asA<mx::Color4>();
        return plVariant(plColor(c[0], c[1], c[2], c[3]));
      }

      if (pValue->isA<mx::Vector2>())
      {
        const mx::Vector2 v = pValue->asA<mx::Vector2>();
        return plVariant(plVec2(v[0], v[1]));
      }

      if (pValue->isA<mx::Vector3>())
      {
        const mx::Vector3 v = pValue->asA<mx::Vector3>();
        return plVariant(plVec3(v[0], v[1], v[2]));
      }

      if (pValue->isA<mx::Vector4>())
      {
        const mx::Vector4 v = pValue->asA<mx::Vector4>();
        return plVariant(plVec4(v[0], v[1], v[2], v[3]));
      }

      // Filenames, geometry names and enum tokens all arrive as strings, and so does anything this
      // does not model - keeping the text means the resolver can still recognise it.
      return plVariant(plString(pValue->getValueString().c_str()));
    }

    /// Copies one MaterialX node into the graph, then follows whatever drives its inputs.
    ///
    /// Recursion is bounded by \a visited rather than by depth: MaterialX documents are acyclic by
    /// specification, but a hand-edited one need not be, and a cycle here would be a stack overflow
    /// inside the editor.
    void AddNodeRecursive(const mx::NodePtr& pNode, plModelImporter2::MaterialGraphDesc& ref_graph, plSet<plString>& ref_visited)
    {
      if (pNode == nullptr)
        return;

      const plString sName = pNode->getName().c_str();

      if (ref_visited.Contains(sName))
        return;

      ref_visited.Insert(sName);

      // Built as a local and pushed complete, with the children walked only afterwards.
      //
      // Recursing while holding a reference into m_Nodes does not work: the recursion appends to the
      // same array, which reallocates it, and every input read after the first connected one then
      // writes through a dangling reference. The symptom is not a crash but a graph that quietly loses
      // connections - which reads as "the texture did not import".
      plModelImporter2::MaterialGraphNode node;
      node.m_sName = sName;
      node.m_sNodeType.Assign(pNode->getCategory().c_str());

      plHybridArray<mx::NodePtr, 8> children;

      for (const mx::InputPtr& pInput : pNode->getInputs())
      {
        if (pInput == nullptr)
          continue;

        plHashedString sInputName;
        sInputName.Assign(pInput->getName().c_str());

        if (const mx::NodePtr pConnected = pInput->getConnectedNode())
        {
          plModelImporter2::MaterialGraphConnection connection;
          connection.m_sSourceNode = pConnected->getName().c_str();
          connection.m_sSourceOutput = pInput->getOutputString().c_str();

          node.m_InputConnections.Insert(sInputName, connection);
          children.PushBack(pConnected);
          continue;
        }

        if (const mx::ValuePtr pValue = pInput->getValue())
        {
          const plVariant value = ToVariant(pValue);

          if (value.IsValid())
            node.m_InputValues.Insert(sInputName, value);
        }
      }

      ref_graph.m_Nodes.PushBack(std::move(node));

      for (const mx::NodePtr& pChild : children)
      {
        AddNodeRecursive(pChild, ref_graph, ref_visited);
      }
    }

    /// Builds one graph from a terminal surface shader node.
    void BuildGraph(const mx::NodePtr& pSurface, plStringView sMaterialName, plStringView sSourcePath,
      plDynamicArray<plModelImporter2::MaterialGraphDesc>& out_graphs)
    {
      plModelImporter2::MaterialGraphDesc& graph = out_graphs.ExpandAndGetRef();
      graph.m_sName = sMaterialName;
      graph.m_sSourcePath = sSourcePath;
      graph.m_sSurfaceNode = pSurface->getName().c_str();

      plSet<plString> visited;
      AddNodeRecursive(pSurface, graph, visited);
    }
  } // namespace

  plString GetStandardLibraryPath(plDynamicArray<plString>* out_pSearchedPaths)
  {
    // Where the libraries sit relative to a directory being probed.
    //
    // "Assets/MaterialXLibraries" is the shipped layout: the mount has to be called Assets for the
    // installer to copy it, and the second level keeps the name specific - the walk below climbs
    // several directories, and a bare "Assets" would happily match some unrelated project folder.
    // The plain form is kept for a manual copy next to the binary.
    const plStringView subPaths[] = {"Assets/MaterialXLibraries"_plsv, "MaterialXLibraries"_plsv};

    auto tryDirectory = [&subPaths, out_pSearchedPaths](plStringView sDirectory, plString& out_sResult) -> bool
    {
      if (sDirectory.IsEmpty())
        return false;

      for (plStringView sSubPath : subPaths)
      {
        plStringBuilder sCandidate = sDirectory;
        sCandidate.AppendPath(sSubPath);
        sCandidate.MakeCleanPath();

        if (out_pSearchedPaths != nullptr)
          out_pSearchedPaths->PushBack(sCandidate);

        if (plOSFile::ExistsDirectory(sCandidate))
        {
          out_sResult = sCandidate;
          return true;
        }
      }

      return false;
    };

    plString sResult;

    // An explicit override first, matching how the other packages locate data they did not install
    // themselves. This one names the folder outright rather than its parent.
    {
      plStringBuilder sOverride = plEnvironmentVariableUtils::GetValueString("PLASMA_MATERIALX_LIBRARIES");

      if (!sOverride.IsEmpty())
      {
        sOverride.MakeCleanPath();

        if (out_pSearchedPaths != nullptr)
          out_pSearchedPaths->PushBack(sOverride);

        if (plOSFile::ExistsDirectory(sOverride))
          return sOverride;
      }
    }

    // Then up from this DLL's own directory.
    //
    // Deliberately not plPlugin::GetPluginPaths: that re-runs a candidate search over the application
    // directory, the plugin search roots and the working directory, and when none of them holds the
    // file it returns a path under the application directory that need not exist. It answers "where
    // would a plugin of this name be found", not "where is this code". Only the second is any use for
    // locating data shipped beside it.
    const plString sModuleDirectory = GetOwnModuleDirectory();

    if (!sModuleDirectory.IsEmpty())
    {
      plStringBuilder sDirectory = sModuleDirectory;

      // Beside the binary, then upwards: an installed package puts the two together, while a local
      // build leaves the binary in Bin/<platform>_<config>/ and the libraries at the package root.
      //
      // Six levels rather than three, because a plugin loaded with LoadCopy runs from a
      // 'loaded/<n>/' subdirectory of wherever it was found - two levels deeper than the build put it.
      for (plUInt32 uiLevel = 0; uiLevel < 6; ++uiLevel)
      {
        if (tryDirectory(sDirectory, sResult))
          return sResult;

        if (sDirectory.IsEmpty() || sDirectory.IsEqual("/"))
          break;

        sDirectory.PathParentDirectory();
      }
    }

    // And finally beside the editor itself, for a build that stages it there.
    if (tryDirectory(plOSFile::GetApplicationDirectory(), sResult))
      return sResult;

    return plString();
  }

  plStatus ReadDocument(plStringView sAbsFilePath, plStringView sSearchPath, plDynamicArray<plModelImporter2::MaterialGraphDesc>& out_graphs)
  {
    const plStringBuilder sFile = sAbsFilePath;
    const plStringBuilder sSearch = sSearchPath;

    try
    {
      // Kept apart from the document's own search path. Passing no folder list makes loadLibraries scan
      // every subdirectory of every path it is given, which is what picks up stdlib, pbrlib and the
      // rest - but it would also drag in any .mtlx sitting next to the document being imported.
      mx::FileSearchPath librarySearchPath;

      if (!sSearch.IsEmpty())
        librarySearchPath = mx::FileSearchPath(sSearch.GetData());

      // The document's own folder, so sibling includes and relative texture paths resolve.
      plStringBuilder sDocumentDir = sFile;
      sDocumentDir.PathParentDirectory();

      mx::FileSearchPath searchPath(sDocumentDir.GetData());

      mx::DocumentPtr pDocument = mx::createDocument();
      mx::readFromXmlFile(pDocument, sFile.GetData(), searchPath);

      // Without the standard library the document's nodedefs do not resolve, and a standard_surface
      // reads as a node with no inputs rather than as an error - so this is worth failing loudly on.
      if (!sSearch.IsEmpty())
      {
        mx::DocumentPtr pStandardLibrary = mx::createDocument();
        mx::loadLibraries({}, librarySearchPath, pStandardLibrary);
        pDocument->importLibrary(pStandardLibrary);
      }

      // Turns every filename input into an absolute path, so the texture importer downstream does not
      // have to know where the document lived.
      mx::flattenFilenames(pDocument, searchPath);

      const plUInt32 uiFirstGraph = out_graphs.GetCount();

      for (const mx::NodePtr& pMaterial : pDocument->getMaterialNodes())
      {
        for (const mx::NodePtr& pSurface : mx::getShaderNodes(pMaterial))
        {
          BuildGraph(pSurface, pMaterial->getName().c_str(), sFile, out_graphs);
        }
      }

      // Plenty of documents in the wild are a bare surface shader with no material node wrapping it.
      if (out_graphs.GetCount() == uiFirstGraph)
      {
        for (const mx::NodePtr& pNode : pDocument->getNodes())
        {
          if (pNode->getType() == mx::SURFACE_SHADER_TYPE_STRING)
          {
            BuildGraph(pNode, pNode->getName().c_str(), sFile, out_graphs);
          }
        }
      }

      if (out_graphs.GetCount() == uiFirstGraph)
        return plStatus(plFmt("'{}' contains no surface shader.", sFile));

      return plStatus(PL_SUCCESS);
    }
    catch (const std::exception& e)
    {
      // MaterialX reports every parse and lookup failure by throwing, so this is the normal error path
      // rather than an exceptional one.
      return plStatus(plFmt("Failed to read '{}': {}", sFile, e.what()));
    }
    catch (...)
    {
      return plStatus(plFmt("Failed to read '{}': unknown MaterialX error.", sFile));
    }
  }

} // namespace plMaterialXGraphReader
