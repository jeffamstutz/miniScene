// anari_viewer
#include "anari_viewer/windows/LightsEditor.h"
#include "anari_viewer/windows/Viewport.h"
// miniScene
#include "miniScene/Scene.h"
// std
#include <iostream>

static const bool g_true = true;
static bool g_verbose = false;
static bool g_useDefaultLayout = true;
static bool g_enableDebug = false;
static std::string g_libraryName = "environment";
static anari::Library g_debug = nullptr;
static anari::Device g_device = nullptr;
static const char *g_traceDir = nullptr;
static std::string g_filename;

static const char *g_defaultLayout =
    R"layout(
[Window][MainDockSpace]
Pos=0,25
Size=1920,1174
Collapsed=0

[Window][Viewport]
Pos=551,25
Size=1369,1174
Collapsed=0
DockId=0x00000002,0

[Window][Lights Editor]
Pos=0,25
Size=549,1174
Collapsed=0
DockId=0x00000001,0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Docking][Data]
DockSpace   ID=0x782A6D6B Window=0xDEDC5B90 Pos=0,25 Size=1920,1174 Split=X Selected=0x13926F0B
  DockNode  ID=0x00000001 Parent=0x782A6D6B SizeRef=549,1174 Selected=0x5098EBE6
  DockNode  ID=0x00000002 Parent=0x782A6D6B SizeRef=1369,1174 CentralNode=1 Selected=0x13926F0B
)layout";

namespace anari {

ANARI_TYPEFOR_SPECIALIZATION(mini::common::vec2f, ANARI_FLOAT32_VEC2);
ANARI_TYPEFOR_SPECIALIZATION(mini::common::vec3f, ANARI_FLOAT32_VEC3);
ANARI_TYPEFOR_SPECIALIZATION(mini::common::vec4f, ANARI_FLOAT32_VEC4);

ANARI_TYPEFOR_DEFINITION(mini::common::vec2f);
ANARI_TYPEFOR_DEFINITION(mini::common::vec3f);
ANARI_TYPEFOR_DEFINITION(mini::common::vec4f);

} // namespace anari

namespace viewer {

static void statusFunc(const void *userData, ANARIDevice device,
                       ANARIObject source, ANARIDataType sourceType,
                       ANARIStatusSeverity severity, ANARIStatusCode code,
                       const char *message) {
  const bool verbose = userData ? *(const bool *)userData : false;
  if (severity == ANARI_SEVERITY_FATAL_ERROR) {
    fprintf(stderr, "[FATAL][%p] %s\n", source, message);
    std::exit(1);
  } else if (severity == ANARI_SEVERITY_ERROR)
    fprintf(stderr, "[ERROR][%p] %s\n", source, message);
  else if (severity == ANARI_SEVERITY_WARNING)
    fprintf(stderr, "[WARN ][%p] %s\n", source, message);
  else if (verbose && severity == ANARI_SEVERITY_PERFORMANCE_WARNING)
    fprintf(stderr, "[PERF ][%p] %s\n", source, message);
  else if (verbose && severity == ANARI_SEVERITY_INFO)
    fprintf(stderr, "[INFO ][%p] %s\n", source, message);
  else if (verbose && severity == ANARI_SEVERITY_DEBUG)
    fprintf(stderr, "[DEBUG][%p] %s\n", source, message);
}

static void initializeANARI() {
  auto library =
      anariLoadLibrary(g_libraryName.c_str(), statusFunc, &g_verbose);
  if (!library)
    throw std::runtime_error("Failed to load ANARI library");

  if (g_enableDebug)
    g_debug = anariLoadLibrary("debug", statusFunc, &g_true);

  anari::Device dev = anariNewDevice(library, "default");

  anari::unloadLibrary(library);

  if (g_enableDebug)
    anari::setParameter(dev, dev, "glDebug", true);

#ifdef USE_GLES2
  anari::setParameter(dev, dev, "glAPI", "OpenGL_ES");
#else
  anari::setParameter(dev, dev, "glAPI", "OpenGL");
#endif

  if (g_enableDebug) {
    anari::Device dbg = anariNewDevice(g_debug, "debug");
    anari::setParameter(dbg, dbg, "wrappedDevice", dev);
    if (g_traceDir) {
      anari::setParameter(dbg, dbg, "traceDir", g_traceDir);
      anari::setParameter(dbg, dbg, "traceMode", "code");
    }
    anari::commitParameters(dbg, dbg);
    anari::release(dev, dev);
    dev = dbg;
  }

  anari::commitParameters(dev, dev);

  g_device = dev;
}

static anari::Sampler makeAnariImageSampler(anari::Device device,
                                            mini::Texture::SP mTexture) {
  if (mTexture->format == mini::Texture::Format::UNDEFINED ||
      mTexture->format == mini::Texture::Format::EMBEDDED_PTEX ||
      mTexture->data.empty())
    return {};

  auto aSampler = anari::newObject<anari::Sampler>(device, "image2D");

  ANARIDataType texelType = ANARI_UFIXED8_VEC4;

  if (mTexture->format == mini::Texture::Format::FLOAT4)
    texelType = ANARI_FLOAT32_VEC4;
  else if (mTexture->format == mini::Texture::Format::FLOAT1)
    texelType = ANARI_FLOAT32;

  auto numTexels = mTexture->size.x * mTexture->size.y;

  anari::setParameterArray2D(device, aSampler, "image", texelType,
                             mTexture->data.data(), mTexture->size.x,
                             mTexture->size.y);
  anari::setParameter(device, aSampler, "filter",
                      mTexture->filterMode ==
                              mini::Texture::FilterMode::FILTER_BILINEAR
                          ? "linear"
                          : "nearest");
  anari::setParameter(device, aSampler, "wrapMode1", "repeat");
  anari::setParameter(device, aSampler, "wrapMode2", "repeat");
  anari::setParameter(device, aSampler, "attribute", "attribute0");
  anari::commitParameters(device, aSampler);
  return aSampler;
}

static anari::Material makeAnariMaterial(anari::Device device,
                                         mini::Material::SP mMaterial) {
  auto aMaterial = anari::newObject<anari::Material>(device, "physicallyBased");
  anari::setParameter(device, aMaterial, "baseColor", mMaterial->baseColor);
  anari::setParameter(device, aMaterial, "metallic", mMaterial->metallic);
  anari::setParameter(device, aMaterial, "roughness", mMaterial->roughness);
  anari::setParameter(device, aMaterial, "transmission",
                      mMaterial->transmission);
  anari::setParameter(device, aMaterial, "ior", mMaterial->ior);
  anari::setParameter(device, aMaterial, "alphaMode", "blend");

  if (mMaterial->colorTexture) {
    auto aSampler = makeAnariImageSampler(device, mMaterial->colorTexture);
    if (aSampler)
      anari::setAndReleaseParameter(device, aMaterial, "baseColor", aSampler);
  }
  if (mMaterial->alphaTexture) {
    auto aSampler = makeAnariImageSampler(device, mMaterial->alphaTexture);
    if (aSampler)
      anari::setAndReleaseParameter(device, aMaterial, "opacity", aSampler);
  }
  anari::commitParameters(device, aMaterial);
  return aMaterial;
}

static anari::Geometry makeAnariGeometry(anari::Device device,
                                         mini::Mesh::SP mMesh) {
  auto aGeometry = anari::newObject<anari::Geometry>(device, "triangle");
  anari::setParameterArray1D(device, aGeometry, "primitive.index",
                             ANARI_UINT32_VEC3, mMesh->indices.data(),
                             mMesh->indices.size());
  anari::setParameterArray1D(device, aGeometry, "vertex.position",
                             mMesh->vertices.data(), mMesh->vertices.size());
  if (!mMesh->normals.empty()) {
    anari::setParameterArray1D(device, aGeometry, "vertex.normal",
                               mMesh->normals.data(), mMesh->normals.size());
  }
  if (!mMesh->texcoords.empty()) {
    anari::setParameterArray1D(device, aGeometry, "vertex.attribute0",
                               mMesh->texcoords.data(),
                               mMesh->texcoords.size());
  }
  anari::commitParameters(device, aGeometry);
  return aGeometry;
}

static anari::Surface makeAnariSurface(anari::Device device,
                                       mini::Mesh::SP mMesh) {
  auto aSurface = anari::newObject<anari::Surface>(device);
  auto aMaterial = makeAnariMaterial(device, mMesh->material);
  auto aGeometry = makeAnariGeometry(device, mMesh);
  anari::setAndReleaseParameter(device, aSurface, "material", aMaterial);
  anari::setAndReleaseParameter(device, aSurface, "geometry", aGeometry);
  anari::commitParameters(device, aSurface);
  return aSurface;
}

static anari::Instance makeAnariInstance(anari::Device device,
                                         mini::Instance::SP mInstance) {
  auto aInstance = anari::newObject<anari::Instance>(device);
  auto aGroup = anari::newObject<anari::Group>(device);

  std::vector<anari::Surface> aSurfaces;

  for (auto &mMesh : mInstance->object->meshes) {
    if (!mMesh)
      continue;
    aSurfaces.push_back(makeAnariSurface(device, mMesh));
  }

  if (!aSurfaces.empty()) {
    anari::setParameterArray1D(device, aGroup, "surface", aSurfaces.data(),
                               aSurfaces.size());
  }

  anari::commitParameters(device, aGroup);

  for (auto s : aSurfaces)
    anari::release(device, s);

  anari::setAndReleaseParameter(device, aInstance, "group", aGroup);
  // TODO create matrix transform
  anari::commitParameters(device, aInstance);
  return aInstance;
}

static anari::World createAnariWorldFromScene(anari::Device device,
                                              mini::Scene::SP scene) {
  auto aWorld = anari::newObject<anari::World>(device);

  if (!scene)
    return aWorld;

  std::vector<anari::Instance> aInstances;

  for (auto &mInstance : scene->instances) {
    if (!mInstance)
      continue;
    aInstances.push_back(makeAnariInstance(device, mInstance));
  }

  if (!aInstances.empty()) {
    anari::setParameterArray1D(device, aWorld, "instance", aInstances.data(),
                               aInstances.size());
  }

  for (auto i : aInstances)
    anari::release(device, i);

  anari::commitParameters(device, aWorld);
  return aWorld;
}

// Application definition /////////////////////////////////////////////////////

struct AppState {
  manipulators::Orbit manipulator;
  anari::Device device{nullptr};
  anari::World world{nullptr};
};

class Application : public match3D::DockingApplication {
public:
  Application() = default;
  ~Application() override = default;

  match3D::WindowArray setup() override {
    ui::init();

    // ANARI //

    initializeANARI();

    auto device = g_device;

    if (!device)
      std::exit(1);

    m_state.device = device;

    // Setup scene //

    printf("loading file '%s'\n", g_filename.c_str());
    auto scene = mini::Scene::load(g_filename);
    m_state.world = createAnariWorldFromScene(m_state.device, scene);

    // ImGui //

    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = 1.5f;
    io.IniFilename = nullptr;

    if (g_useDefaultLayout)
      ImGui::LoadIniSettingsFromMemory(g_defaultLayout);

    auto *viewport = new windows::Viewport(device, "Viewport");
    viewport->setManipulator(&m_state.manipulator);
    viewport->setWorld(m_state.world);
    viewport->resetView();

    auto *leditor = new windows::LightsEditor({device});
    leditor->setWorlds({m_state.world});

    match3D::WindowArray windows;
    windows.emplace_back(viewport);
    windows.emplace_back(leditor);

    return windows;
  }

  void buildMainMenuUI() override {
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("print ImGui ini")) {
          const char *info = ImGui::SaveIniSettingsToMemory();
          printf("%s\n", info);
        }

        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }
  }

  void teardown() override {
    anari::release(m_state.device, m_state.world);
    anari::release(m_state.device, m_state.device);
    ui::shutdown();
  }

private:
  AppState m_state;
};

} // namespace viewer

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static void printUsage() {
  std::cout << "./anariViewer [{--help|-h}]\n"
            << "   [{--verbose|-v}] [{--debug|-g}]\n"
            << "   [{--library|-l} <ANARI library>]\n"
            << "   [{--trace|-t} <directory>]\n";
}

static void parseCommandLine(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose")
      g_verbose = true;
    if (arg == "--help" || arg == "-h") {
      printUsage();
      std::exit(0);
    } else if (arg == "--noDefaultLayout")
      g_useDefaultLayout = false;
    else if (arg == "-l" || arg == "--library")
      g_libraryName = argv[++i];
    else if (arg == "--debug" || arg == "-g")
      g_enableDebug = true;
    else if (arg == "--trace" || arg == "-t")
      g_traceDir = argv[++i];
    else
      g_filename = std::move(arg);
  }
}

int main(int argc, char *argv[]) {
  parseCommandLine(argc, argv);
  if (g_filename.empty()) {
    printf("ERROR: no .mini file provided\n");
    std::exit(1);
  }
  viewer::Application app;
  app.run(1920, 1200, "ANARI miniScene Viewer");
  return 0;
}
