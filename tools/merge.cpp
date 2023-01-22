// ======================================================================== //
// Copyright 2022++ Ingo Wald                                               //
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
#include "miniScene/Serialized.h"

namespace mini {

  void usage(const std::string &error = "")
  {
    if (!error.empty())
      std::cerr << MINI_TERMINAL_RED << "Error: " << error
                << MINI_TERMINAL_DEFAULT << std::endl << std::endl;
    std::cout << "miniMerge a.mini b.mini ... -o merged.mini" << std::endl;
    exit(error.empty()?0:1);
  }
  
  void miniMerge(int ac, char **av)
  {
    std::vector<std::string> inFileNames;
    std::string outFileName = "";
    bool mergeStatic = true;
            
    if (ac == 1) usage();
    for (int i=1;i<ac;i++) {
      std::string arg = av[i];
      if (arg[0] != '-')
        inFileNames.push_back(arg);
      else if (arg == "-o")
        outFileName = av[++i];
      else if (arg == "--no-merge-static")
        mergeStatic = false;
      else if (arg == "-h" || arg == "--help")
        usage();
      else
        throw std::runtime_error("unknown cmdline argument '"+arg+"'");
    }

    if (inFileNames.empty())
      usage("no input file names specified");
    if (outFileName.empty())
      usage("no output file name specified");

    Scene::SP out = Scene::create();

    for (auto inFileName : inFileNames) {
      std::cout << MINI_TERMINAL_LIGHT_BLUE
                << "loading mini file from " << inFileName 
                << MINI_TERMINAL_DEFAULT << std::endl;
      Scene::SP scene = Scene::load(inFileName);
      for (auto inst : scene->instances) {
        if (mergeStatic && inst->xfm == affine3f()) {
          if (out->instances.empty()) {
            out->instances.push_back(inst);
            continue;
          }
          
          if (out->instances.size() == 1 && out->instances[0]->xfm == affine3f()) {
            for (auto mesh : inst->object->meshes)
              out->instances[0]->object->meshes.push_back(mesh);
            continue;
          }
        }
        // fall-through: either merging is disabled, or could not merge:
        out->instances.push_back(inst);
      }
    }
    
    out->save(outFileName);
    std::cout << MINI_TERMINAL_LIGHT_GREEN
              << "#miniInfo: merged scene saved."
              << MINI_TERMINAL_DEFAULT << std::endl;
  }
  
} // ::mini

int main(int ac, char **av)
{ mini::miniMerge(ac,av); return 0; }

