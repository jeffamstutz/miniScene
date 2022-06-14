# ======================================================================== #
# Copyright 2018-2021 Ingo Wald                                            #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
# ======================================================================== #


# TODO: make this importer/converter - and all the pbrt and cup stuff
# that is only required for this tool - optional...
option(MINI_IMPORTER_PBRT "Build PBRT importer" OFF)
if (MINI_IMPORT_PBRT)
  include(${CMAKE_SOURCE_DIR}/cmake/FindOpenEXR.cmake)
  add_subdirectory(submodules/pbrt-parser EXCLUDE_FROM_ALL)
  add_executable(pbf2mini
    pbf2mini.cpp
    cup_tools/tessellateCurves.cpp
    cup_tools/removeAllNonMeshShapes.cpp
    )
  target_link_libraries(pbf2mini
    PUBLIC
    miniScene
    pbrtParser
    )
  if (OPENEXR_FOUND)
    target_link_libraries(pbf2mini
      PUBLIC #OpenEXR::OpenEXR
      ${OPENEXR_LIBRARIES}
      )
    target_include_directories(pbf2mini
      PUBLIC
      ${OPENEXR_INCLUDES}
      )
    target_compile_definitions(pbf2mini
      PUBLIC HAVE_OPENEXR=1
      )
  endif()
endif()


# ==================================================================
# imports wavefront OBJ (w/ most material stuff) and textures; every
# file becomes one single unit-instance with single object that
# ocntains all meshes
# ==================================================================
option(MINI_IMPORTER_OBJ "Build OBJ importer" ON)
if (MINI_IMPORTER_OBJ)
  add_executable(obj2mini
    obj2mini.cpp
    )
  target_link_libraries(obj2mini
    miniScene
    )
endif()

# ==================================================================
# imports PLY models via the happly parser (in submodules/)
# ==================================================================
option(MINI_IMPORTER_PLY "Build PLY importer" OFF)
if (MINI_IMPORTER_PLY)
  add_executable(ply2mini
    ply2mini.cpp
    )
  target_link_libraries(ply2mini
    miniScene
    )
  target_include_directories(ply2mini PUBLIC ${PROJECT_SOURCE_DIR}/submodules/)
endif()

# ==================================================================
# imports (my own) "binmesh" models of the form "size_t numVtx; vec3f
# vtx[numVtx]; size_t numIdx; vec3i idx[numIdx]".
# ==================================================================
option(MINI_IMPORTER_BINMESH "Build BINMESH importer" OFF)
if (MINI_IMPORTER_BINMESH)
  add_executable(binmesh2mini
    binmesh2mini.cpp
    )
  target_link_libraries(binmesh2mini
    miniScene
    )
  target_include_directories(binmesh2mini PUBLIC ${PROJECT_SOURCE_DIR}/submodules/)
endif()

# ==================================================================
# scene tool to bake out moana pbrt+ptex into "baked ptex textures",
# so the importers/pbrt importer can then replace ptex textures with
# those bakes ones...
# ==================================================================
option(MINI_IMPORTER_BAKE_PTEX "Build BakePtex tool" OFF)
if (MINI_IMPORTER_BAKE_PTEX)
  add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/ptex EXCLUDE_FROM_ALL)
  add_executable(miniBakePtex
    bakeOutPtex.cpp
    )
  target_include_directories(miniBakePtex
    PRIVATE
    ${PROJECT_SOURCE_DIR}/submodules/ptex/src/ptex/
    )
  target_link_libraries(miniBakePtex
    miniScene
    pbrtParser
    Ptex_static
    )
endif()

