//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

// Upstream generates this file from Generated.h.in with CMake's configure_file. PlasmaBuild has no
// equivalent step, so it is written out here with the values the CMake would have substituted for
// MaterialX 1.39.3. If the vendored MaterialX is updated, update the three version numbers to match
// source/CMakeLists.txt - nothing else in this file depends on the version.
//
// MATERIALX_BUILD_SHARED_LIBS is deliberately left undefined: MaterialX is compiled straight into the
// plugin, so the symbol visibility macros collapse to nothing and there is no second DLL to ship.

#ifndef MATERIALX_GENERATED_H
#define MATERIALX_GENERATED_H

#define MATERIALX_MAJOR_VERSION 1
#define MATERIALX_MINOR_VERSION 39
#define MATERIALX_BUILD_VERSION 3

// Establish namespace:
namespace MaterialX_v1_39_3
{
}

// Establish alias to allow downstream clients to still use the MaterialX namespace:
namespace MaterialX = MaterialX_v1_39_3;

// All code in this project must use these macros for opening and closing the
// global MaterialX namespace:
#define MATERIALX_NAMESPACE_BEGIN namespace MaterialX_v1_39_3 {
#define MATERIALX_NAMESPACE_END }

#endif
