# plasma.materialx

Imports MaterialX (`.mtlx`) documents as Plasma material assets.

Editor only. A `.mtlx` becomes a `.plMaterialAsset` at import time and nothing about MaterialX survives
into the runtime, which loads the baked material like any other.

## What it does

A MaterialX document is a graph of typed nodes ending in a surface shader. The plugin reads that graph
into the engine's format-neutral `plModelImporter2::MaterialGraphDesc`, then lowers the terminal shader
onto `OpenPBRMaterialDesc` — the same parameter set every model importer already targets — and writes a
material asset from it.

Four terminal shaders are recognised:

| Terminal | Mapping quality |
| --- | --- |
| `open_pbr_surface` | Name for name. The engine's own surface *is* OpenPBR. |
| `standard_surface` | Complete, modulo spelling (`sheen` → fuzz, `metalness` → base metalness). |
| `gltf_pbr` | Complete for what glTF carries. |
| `UsdPreviewSurface` | Complete for what it carries, which is the least of the four by design. |

An input driven by a constant or by a single `image` / `tiledimage` node is read directly. An input
driven by a larger sub-graph keeps its OpenPBR default and is **named in the editor log** — a material
that silently lost an input looks like a shading bug in the viewport and nothing else records why.
`tiledimage` UV tiling is reported the same way: the texture imports, the tiling does not.

## What it does not do

- **No shader generation.** MaterialX ships GLSL, MSL, OSL and MDL generators and no HLSL one, and
  `plShaderCompilerDXC` takes HLSL. Only `MaterialXCore` and `MaterialXFormat` are vendored, because the
  generator libraries would build a large amount of code nothing here could consume.
- **No procedural node translation.** `noise2d`, `fractal3d`, `ifgreater` and friends stop the walk and
  the input keeps its default. Translating them means emitting visual shader nodes, which is a separate
  pass over the same graph.
- **No export.** Writing a Plasma material back out as `.mtlx` is not implemented. The IR round-trips by
  design, so this is mostly serialisation when it is wanted.

## Building

```bash
PLASMA_SDK_ROOT=E:/PlasmaTech/PlasmaEngine2 plasmabuild build EditorPluginMaterialX
```

Output lands in `Bin/Windows_x64_Development/`.

### Requires engine support

This package builds against engine-side pieces added for it, all on the importer/material path:

- `plModelImporter2::MaterialGraphDesc` and `ResolveMaterialGraph` — the graph IR and its resolver.
- `plMeshImportUtils::ApplyOpenPBRMaterial` — exported so an importer that is not a *model* importer can
  produce material assets without keeping a second copy of the OpenPBR mapping in step.
- `plMaterialAssetDocument` — exported for the RTTI needed to cast a created document.
- `%ImporterFileTypes` on `plPluginBundle` — lets the editor name this package when a project opens with
  it disabled, instead of reporting `.mtlx` as an unsupported file type.

## Vendored MaterialX

MaterialX 1.39.3 (Apache-2.0), under `Source/ThirdParty/MaterialX`. `MaterialXCore` and
`MaterialXFormat` only, compiled straight into the plugin as a static library — `MATERIALX_BUILD_SHARED_LIBS`
is left undefined, which collapses its symbol visibility macros to nothing, so there is no second binary
to ship and no export/import mismatch to get wrong.

Its CMake is replaced by `Source/ThirdParty/MaterialX/materialx.module.cs`. The only build step CMake
performed that PlasmaBuild does not is `configure_file` on `MaterialXCore/Generated.h.in`; that header is
checked in instead, and carries a comment saying which three version numbers to update alongside a
version bump.

`MaterialXLibraries/` is the MaterialX standard node definition library, shipped as the package's
`assets` component. It is not optional: without it a document using a standard nodedef — which is nearly
all of them — resolves to a node with no inputs and would import as a blank material rather than as an
error. The plugin looks for it beside its own binary, then beside the editor, and `PLASMA_MATERIALX_LIBRARIES`
overrides both.

## Status

Builds and links, in tree and out. **Not yet exercised in a running editor** — no `.mtlx` has been
imported through it. The reader's assumptions were checked against MaterialX's own sample documents
(`resources/Materials/Examples`), including that `Input::getConnectedNode()` resolves transitively
through `nodegraph` outputs, which is how every texture-driven `UsdPreviewSurface` in that corpus is
wired.
