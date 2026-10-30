using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

// MaterialX 1.39.3, replacing its CMake build.
//
// Only MaterialXCore and MaterialXFormat are vendored - reading and writing .mtlx documents and
// resolving nodedefs. The shader generation libraries (MaterialXGenGlsl, GenMsl, GenOsl, GenMdl) are
// deliberately absent: none of them emits HLSL, which is the only language this engine's shader
// compiler accepts, so they would build a large amount of code that nothing could consume.
//
// Compiled straight into the plugin rather than as its own DLL. MATERIALX_BUILD_SHARED_LIBS is left
// undefined, which collapses MaterialX's symbol visibility macros to nothing - see MaterialXCore/
// Library.h. That means no second binary to ship and no export/import mismatch to get wrong.
//
// Upstream generates MaterialXCore/Generated.h with configure_file; it is checked in here instead,
// with a comment saying which values must be updated alongside a version bump.
public class MaterialXModule : ModuleRules
{
    public MaterialXModule(BuildContext context) : base(context)
    {
        Type = ModuleType.ThirdParty;
        SourceDirectory = "Source/ThirdParty/MaterialX";

        // MaterialX includes its own headers as <MaterialXCore/Document.h>, so the directory holding
        // MaterialXCore and MaterialXFormat is what goes on the include path - for consumers too.
        PublicIncludePaths.Add(".");

        // MaterialX uses exceptions for every parse and lookup failure; there is no error-code path to
        // switch to. The plugin catches them at its own boundary and turns them into plStatus.
        EnableExceptions = true;

        // Its own PCH would have to be authored from scratch, and 21 files do not justify one.
        PCHUsage = PCHUsage.NoPCHs;

        // Third-party code compiled as-is. MaterialX is warning-clean at its own level but not at the
        // engine's, and patching upstream would make the next version bump a merge.
        TreatWarningsAsErrors = false;

        CppStandard = CppStandard.Cpp20;
    }
}
