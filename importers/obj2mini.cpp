// ======================================================================== //
// Copyright 2018-2022 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "miniScene/Scene.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION 1
#include "stb/stb_image.h"

//std
#include <set>

namespace std {
  inline bool operator<(const tinyobj::index_t &a,
                        const tinyobj::index_t &b)
  {
    if (a.vertex_index < b.vertex_index) return true;
    if (a.vertex_index > b.vertex_index) return false;

    if (a.normal_index < b.normal_index) return true;
    if (a.normal_index > b.normal_index) return false;

    if (a.texcoord_index < b.texcoord_index) return true;
    if (a.texcoord_index > b.texcoord_index) return false;

    return false;
  }
}

namespace mini {

    /*! find vertex with given position, normal, texcoord, and return
      its vertex ID, or, if it doesn't exit, add it to the mesh, and
      its just-created index */
    int addVertex(Mesh::SP mesh,
                  tinyobj::attrib_t &attributes,
                  const tinyobj::index_t &idx,
                  std::map<tinyobj::index_t,int> &knownVertices)
    {
      if (knownVertices.find(idx) != knownVertices.end())
        return knownVertices[idx];

      const vec3f *vertex_array   = (const vec3f*)attributes.vertices.data();
      const vec3f *normal_array   = (const vec3f*)attributes.normals.data();
      const vec2f *texcoord_array = (const vec2f*)attributes.texcoords.data();

      int newID = (int)mesh->vertices.size();
      knownVertices[idx] = newID;

      mesh->vertices.push_back(vertex_array[idx.vertex_index]);
      if (idx.normal_index >= 0) {
        while (mesh->normals.size() < mesh->vertices.size())
          mesh->normals.push_back(normal_array[idx.normal_index]);
      }
      if (idx.texcoord_index >= 0) {
        while (mesh->texcoords.size() < mesh->vertices.size())
          mesh->texcoords.push_back(texcoord_array[idx.texcoord_index]);
      }

      return newID;
    }

    /*! load a texture (if not already loaded), and return its ID in the
      model's textures[] vector. Textures that could not get loaded
      return -1 */
    Texture::SP loadTexture(std::map<std::string,Texture::SP> &knownTextures,
                            const std::string &inFileName,
                            const std::string &modelPath)
    {
      if (inFileName == "")
        return nullptr;

      if (knownTextures.find(inFileName) != knownTextures.end())
        return knownTextures[inFileName];

      std::string fileName = inFileName;
      // first, fix backspaces:
      for (auto &c : fileName)
        if (c == '\\') c = '/';
      fileName = modelPath+"/"+fileName;

      Texture::SP texture;
      vec2i res;
      int   comp;
      unsigned char* image = stbi_load(fileName.c_str(),
                                       &res.x, &res.y, &comp, STBI_rgb_alpha);
      int textureID = -1;
      if (image) {
        for (int y=0;y<res.y/2;y++) {
          uint32_t *line_y = ((uint32_t*)image) + y * res.x;
          uint32_t *mirrored_y = ((uint32_t*)image) + (res.y-1-y) * res.x;
          for (int x=0;x<res.x;x++) {
            std::swap(line_y[x],mirrored_y[x]);
          }
        }

        texture = std::make_shared<Texture>();
        texture->size = res;
        texture->data.resize(res.x*res.y*sizeof(int));
        memcpy(texture->data.data(),image,texture->data.size());

        /* iw - actually, it seems that stbi loads the pictures
           mirrored along the y axis - mirror them here */

        STBI_FREE(image);
      } else {
        std::cout << MINI_COLOR_RED
                  << "Could not load texture from " << fileName << "!"
                  << MINI_COLOR_DEFAULT << std::endl;
      }

      knownTextures[inFileName] = texture;
      return texture;
    }

    Scene::SP loadOBJ(const std::string &objFile)
    {
      Scene::SP scene = std::make_shared<Scene>();
      Object::SP model = std::make_shared<Object>();
      scene->instances.push_back(std::make_shared<Instance>(model));
      const std::string modelDir
        = objFile.substr(0,objFile.rfind('/')+1);

      tinyobj::attrib_t attributes;
      std::vector<tinyobj::shape_t> shapes;
      std::vector<tinyobj::material_t> materials;
      std::string err = "";

      std::cout << "reading OBJ file '" << objFile << " from directory '" << modelDir << "'" << std::endl;
      bool readOK
        = tinyobj::LoadObj(&attributes,
                           &shapes,
                           &materials,
                           &err,
                           &err,
                           objFile.c_str(),
                           modelDir.c_str(),
                           /* triangulate */true);
      if (!readOK) {
        throw std::runtime_error("Could not read OBJ model from "+objFile+" : "+err);
      }

      if (materials.empty())
        // throw std::runtime_error("could not parse materials ...");
        std::cout << MINI_COLOR_RED << "WARNING: NO MATERIALS (could not find/parse mtl file!?)" << MINI_COLOR_DEFAULT << std::endl;

      std::vector<Material::SP> baseMaterials;
      for (auto &objMat : materials) {
        Material::SP baseMaterial = std::make_shared<Material>();
        baseMaterial->baseColor =
          { float(objMat.diffuse[0]),
            float(objMat.diffuse[1]),
            float(objMat.diffuse[2]) };
        baseMaterial->emission =
          { float(objMat.emission[0]),
            float(objMat.emission[1]),
            float(objMat.emission[2]) };
        baseMaterial->ior = objMat.ior;
        baseMaterials.push_back(baseMaterial);
      }

      Material::SP dummyMaterial = std::make_shared<Material>();
      dummyMaterial->baseColor = randomColor(size_t(dummyMaterial.get()));

      std::map<std::pair<Material::SP,Texture::SP>,Material::SP>
        texturedMaterials;

      std::cout << "Done loading obj file - found "
                << shapes.size() << " shapes with "
                << materials.size() << " materials" << std::endl;

      std::map<std::string,Texture::SP> knownTextures;
      for (int shapeID=0;shapeID<(int)shapes.size();shapeID++) {
        tinyobj::shape_t &shape = shapes[shapeID];

        Mesh::SP mesh = std::make_shared<Mesh>();

        std::set<int> materialIDs;
        for (auto faceMatID : shape.mesh.material_ids)
          materialIDs.insert(faceMatID);


        for (int materialID : materialIDs) {
          std::map<tinyobj::index_t,int> knownVertices;
          Mesh::SP mesh = std::make_shared<Mesh>();
          // mesh->material
          //   = (materialID < ourMaterials.size())
          //   ? ourMaterials[materialID]
          //   : dummyMaterial;

          for (size_t faceID=0;faceID<shape.mesh.material_ids.size();faceID++) {
            if (shape.mesh.material_ids[faceID] != materialID) continue;
            if (shape.mesh.num_face_vertices[faceID] != 3)
              throw std::runtime_error("not properly tessellated");
            tinyobj::index_t idx0 = shape.mesh.indices[3*faceID+0];
            tinyobj::index_t idx1 = shape.mesh.indices[3*faceID+1];
            tinyobj::index_t idx2 = shape.mesh.indices[3*faceID+2];

            vec3i idx(addVertex(mesh, attributes, idx0, knownVertices),
                      addVertex(mesh, attributes, idx1, knownVertices),
                      addVertex(mesh, attributes, idx2, knownVertices));
            mesh->indices.push_back(idx);
          }
          Texture::SP diffuseTexture
            = (materialID < materials.size())
            ? loadTexture(knownTextures,
                          materials[materialID].diffuse_texname,
                          modelDir)
            : 0;

          Material::SP baseMaterial
            = (materialID < materials.size())
            ? baseMaterials[materialID]
            : dummyMaterial;

          std::pair<Material::SP,Texture::SP> tuple = { baseMaterial,diffuseTexture };
          if (texturedMaterials.find(tuple) == texturedMaterials.end()) {
            mesh->material = std::make_shared<Material>();
            *mesh->material = *baseMaterial;
            mesh->material->colorTexture = diffuseTexture;
            texturedMaterials[tuple] = mesh->material;
          } else
            mesh->material = texturedMaterials[tuple];

          if (mesh->vertices.empty())
            /* ignore this mesh */;
          else {
            model->meshes.push_back(mesh);
          }
        }
      }

      return scene;
    }

} // ::mini


void usage(const std::string &msg)
{
  if (!msg.empty()) std::cerr << std::endl << "***Error***: " << msg << std::endl << std::endl;
  std::cout << "Usage: ./obj2brix inFile.pbf -o outfile.brx" << std::endl;
  std::cout << "Imports a OBJ+MTL file into brix's scene format.\n";
  std::cout << "(from where it can then be partitioned and/or rendered)\n";
  exit(msg != "");
}

int main(int ac, char **av)
{
  std::string inFileName = "";
  std::string outFileName = "";

  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg == "-o") {
      outFileName = av[++i];
    } else if (arg[0] != '-')
      inFileName = arg;
    else
      usage("unknown cmd line arg '"+arg+"'");
  }

  if (inFileName.empty()) usage("no input file name specified");
  if (outFileName.empty()) usage("no output file name base specified");

  std::cout << MINI_COLOR_BLUE
            << "loading OBJ model from " << inFileName
            << MINI_COLOR_DEFAULT << std::endl;

  mini::Scene::SP scene = mini::loadOBJ(inFileName);

  std::cout << MINI_COLOR_DEFAULT
            << "done importing; saving to " << outFileName
            << MINI_COLOR_DEFAULT << std::endl;
  scene->save(outFileName);
  std::cout << MINI_COLOR_LIGHT_GREEN
            << "scene saved"
            << MINI_COLOR_DEFAULT << std::endl;
  return 0;
}
