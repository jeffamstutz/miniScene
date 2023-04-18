// Copyright 2022 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "match3D/match3D.h"
// anari
#include <anari/anari_cpp.hpp>
#include <anari/type_utility.h>
// miniScene
#include "miniScene/Scene.h"
// std
#include <cstring>

#include "Orbit.h"

namespace anari {

ANARI_TYPEFOR_SPECIALIZATION(float2, ANARI_FLOAT32_VEC2);
ANARI_TYPEFOR_SPECIALIZATION(float3, ANARI_FLOAT32_VEC3);
ANARI_TYPEFOR_SPECIALIZATION(float4, ANARI_FLOAT32_VEC4);
ANARI_TYPEFOR_SPECIALIZATION(int2, ANARI_INT32_VEC2);
ANARI_TYPEFOR_SPECIALIZATION(int3, ANARI_INT32_VEC3);
ANARI_TYPEFOR_SPECIALIZATION(int4, ANARI_INT32_VEC4);
ANARI_TYPEFOR_SPECIALIZATION(uint2, ANARI_UINT32_VEC2);
ANARI_TYPEFOR_SPECIALIZATION(uint3, ANARI_UINT32_VEC3);
ANARI_TYPEFOR_SPECIALIZATION(uint4, ANARI_UINT32_VEC4);

ANARI_TYPEFOR_DEFINITION(float2);
ANARI_TYPEFOR_DEFINITION(float3);
ANARI_TYPEFOR_DEFINITION(float4);
ANARI_TYPEFOR_DEFINITION(int2);
ANARI_TYPEFOR_DEFINITION(int3);
ANARI_TYPEFOR_DEFINITION(int4);
ANARI_TYPEFOR_DEFINITION(uint2);
ANARI_TYPEFOR_DEFINITION(uint3);
ANARI_TYPEFOR_DEFINITION(uint4);

ANARI_TYPEFOR_SPECIALIZATION(mini::common::vec2f, ANARI_FLOAT32_VEC2);
ANARI_TYPEFOR_SPECIALIZATION(mini::common::vec3f, ANARI_FLOAT32_VEC3);
ANARI_TYPEFOR_SPECIALIZATION(mini::common::vec4f, ANARI_FLOAT32_VEC4);

ANARI_TYPEFOR_DEFINITION(mini::common::vec2f);
ANARI_TYPEFOR_DEFINITION(mini::common::vec3f);
ANARI_TYPEFOR_DEFINITION(mini::common::vec4f);

} // namespace anari

static void statusFunc(const void *userData, ANARIDevice device,
                       ANARIObject source, ANARIDataType sourceType,
                       ANARIStatusSeverity severity, ANARIStatusCode code,
                       const char *message) {
  bool verbose = true; // userData ? *(const bool *)userData : false;
  if (severity == ANARI_SEVERITY_FATAL_ERROR)
    fprintf(stderr, "[ANARI][FATAL] %s\n", message);
  else if (severity == ANARI_SEVERITY_ERROR)
    fprintf(stderr, "[ANARI][ERROR] %s\n", message);
  else if (severity == ANARI_SEVERITY_WARNING)
    fprintf(stderr, "[ANARI][WARN ] %s\n", message);

  if (!verbose)
    return;

  if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING)
    fprintf(stderr, "[ANARI][PERF ] %s\n", message);
  else if (severity == ANARI_SEVERITY_INFO)
    fprintf(stderr, "[ANARI][INFO ] %s\n", message);
  else if (severity == ANARI_SEVERITY_DEBUG)
    fprintf(stderr, "[ANARI][DEBUG] %s\n", message);
}

static void noDelete(const void * /*user_data*/, const void * /*ptr*/) {
  // no-op
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

  auto array = anariNewArray2D(device, mTexture->data.data(), noDelete, nullptr,
                               texelType, mTexture->size.x, mTexture->size.y);

  anari::setAndReleaseParameter(device, aSampler, "image", array);
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

  if (mMaterial->colorTexture) {
    auto aSampler = makeAnariImageSampler(device, mMaterial->colorTexture);
    if (aSampler)
      anari::setAndReleaseParameter(device, aMaterial, "baseColor", aSampler);
  }
  anari::commitParameters(device, aMaterial);
  return aMaterial;
}

static anari::Geometry makeAnariGeometry(anari::Device device,
                                         mini::Mesh::SP mMesh) {
  auto aGeometry = anari::newObject<anari::Geometry>(device, "triangle");
  anari::setAndReleaseParameter(
      device, aGeometry, "primitive.index",
      anariNewArray1D(device, mMesh->indices.data(), noDelete, nullptr,
                      ANARI_UINT32_VEC3, mMesh->indices.size()));
  anari::setAndReleaseParameter(
      device, aGeometry, "vertex.position",
      anari::newArray1D(device, mMesh->vertices.data(), noDelete, nullptr,
                        mMesh->vertices.size()));
  if (!mMesh->normals.empty()) {
    anari::setAndReleaseParameter(
        device, aGeometry, "vertex.normal",
        anari::newArray1D(device, mMesh->normals.data(), noDelete, nullptr,
                          mMesh->normals.size()));
  }
  if (!mMesh->texcoords.empty()) {
    anari::setAndReleaseParameter(
        device, aGeometry, "vertex.attribute0",
        anari::newArray1D(device, mMesh->texcoords.data(), noDelete, nullptr,
                          mMesh->texcoords.size()));
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
    anari::setAndReleaseParameter(
        device, aGroup, "surface",
        anari::newArray1D(device, aSurfaces.data(), aSurfaces.size()));
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
                                              mini::Scene::SP scene,
                                              mini::common::box3f &bounds) {
  auto aWorld = anari::newObject<anari::World>(device);

  if (!scene)
    return aWorld;

  bounds = scene->getBounds();

  std::vector<anari::Instance> aInstances;

  for (auto &mInstance : scene->instances) {
    if (!mInstance)
      continue;
    aInstances.push_back(makeAnariInstance(device, mInstance));
  }

  if (!aInstances.empty()) {
    anari::setAndReleaseParameter(
        device, aWorld, "instance",
        anari::newArray1D(device, aInstances.data(), aInstances.size()));
  }

  for (auto i : aInstances)
    anari::release(device, i);

  anari::commitParameters(device, aWorld);
  return aWorld;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ExampleApp : public match3D::SimpleApplication {
public:
  ExampleApp(std::string filename);
  ~ExampleApp() override = default;

  void setup() override;
  void buildUI() override;
  void drawBackground() override;
  void teardown() override;

private:
  void handleInput();
  void updateCamera();
  void reshape(int width, int height);
  void loadScene();

  // Data //

  anari::Device m_device{nullptr};
  anari::Frame m_frame{nullptr};
  anari::World m_world{nullptr};
  anari::Renderer m_renderer{nullptr};
  anari::Camera m_camera{nullptr};

  float m_duration{0.f};

  std::string m_filename;

  mini::Scene::SP m_scene;
  mini::common::box3f m_bounds;

  float2 m_previousMouse;
  bool m_mouseRotating{false};
  bool m_manipulating{false};
  Orbit m_arcball;

  GLuint m_framebufferTexture{0};
  GLuint m_framebufferObject{0};
  int m_width{1200};
  int m_height{800};
  int m_renderWidth{0};
  int m_renderHeight{0};
};

// ExampleApp definitions /////////////////////////////////////////////////////

ExampleApp::ExampleApp(std::string filename) : m_filename(filename) {
  m_previousMouse = float2(-1, -1);
}

void ExampleApp::setup() {
  // ANARI //

  anari::Library library = anari::loadLibrary("environment", statusFunc);
  if (!library)
    throw std::runtime_error("failed to load ANARI library");

  m_device = anari::newDevice(library, "default");

  anari::unloadLibrary(library);

  printf("loading file '%s'\n", m_filename.c_str());
  m_scene = mini::Scene::load(m_filename);

  m_camera = anari::newObject<anari::Camera>(m_device, "perspective");
  m_renderer = anari::newObject<anari::Renderer>(m_device, "default");
  m_world = createAnariWorldFromScene(m_device, m_scene, m_bounds);
  m_frame = anari::newObject<anari::Frame>(m_device);

  anari::setParameter(m_device, m_frame, "camera", m_camera);
  anari::setParameter(m_device, m_frame, "renderer", m_renderer);
  anari::setParameter(m_device, m_frame, "world", m_world);

  anari::setParameter(m_device, m_renderer, "background",
                      float4(0.2f, 0.2f, 0.2f, 1.f));
  anari::commitParameters(m_device, m_renderer);

  // ImGui //

  ImGuiIO &io = ImGui::GetIO();
  io.FontGlobalScale = 1.5f;
  io.IniFilename = nullptr;

  // OpenGL //

  // Create a texture
  glGenTextures(1, &m_framebufferTexture);
  glBindTexture(GL_TEXTURE_2D, m_framebufferTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  // Map framebufferTexture (above) to a new OpenGL read/write framebuffer
  glGenFramebuffers(1, &m_framebufferObject);
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferObject);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         m_framebufferTexture, 0);
  glReadBuffer(GL_COLOR_ATTACHMENT0);

  // Load scene + final setup //

  float3 bounds[2];
  std::memcpy(bounds, &m_bounds, sizeof(bounds));
  printf("\nworld bounds: ({%f, %f, %f}, {%f, %f, %f}\n\n", bounds[0].x,
         bounds[0].y, bounds[0].z, bounds[1].x, bounds[1].y, bounds[1].z);

  auto center = 0.5f * (bounds[0] + bounds[1]);
  auto diag = bounds[1] - bounds[0];

  m_arcball =
      Orbit((bounds[0] + bounds[1]) / 2.f, 1.25f * linalg::length(diag));

  reshape(m_width, m_height);
  updateCamera();
  anari::render(m_device, m_frame);
}

void ExampleApp::buildUI() {
  if (getWindowSize(m_width, m_height))
    reshape(m_width, m_height);

  handleInput();

  {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);

    ImGui::Begin("Debug Info", nullptr, windowFlags);

    ImGui::Text("display rate: %.1f FPS", 1.f / getLastFrameLatency());
    ImGui::Text(" render rate: %.1f FPS", 1.f / m_duration);
    ImGui::NewLine();

    ImGui::Text("set 'up': ");
    ImGui::SameLine();
    if (ImGui::Button("+x")) {
      m_arcball.setAxis(OrbitAxis::POS_X);
      updateCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("+y")) {
      m_arcball.setAxis(OrbitAxis::POS_Y);
      updateCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("+z")) {
      m_arcball.setAxis(OrbitAxis::POS_Z);
      updateCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("-x")) {
      m_arcball.setAxis(OrbitAxis::NEG_X);
      updateCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("-y")) {
      m_arcball.setAxis(OrbitAxis::NEG_Y);
      updateCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("-z")) {
      m_arcball.setAxis(OrbitAxis::NEG_Z);
      updateCamera();
    }

    ImGui::End();
  }
}

void ExampleApp::drawBackground() {
  int &w = m_width;
  int &h = m_height;

  if (anari::isReady(m_device, m_frame)) {
    anari::getProperty(m_device, m_frame, "duration", m_duration);

    auto fb = anari::map<uint32_t>(m_device, m_frame, "channel.color");
    glBindTexture(GL_TEXTURE_2D, m_framebufferTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb.width, fb.height, GL_RGBA,
                    GL_UNSIGNED_BYTE, fb.data);
    anari::unmap(m_device, m_frame, "channel.color");
    anari::render(m_device, m_frame);
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, m_framebufferObject);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  glClear(GL_COLOR_BUFFER_BIT);
  glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void ExampleApp::teardown() {
  anari::wait(m_device, m_frame);

  anari::release(m_device, m_world);
  anari::release(m_device, m_camera);
  anari::release(m_device, m_renderer);

  anari::release(m_device, m_frame);

  anari::release(m_device, m_device);
}

void ExampleApp::handleInput() {
  ImGuiIO &io = ImGui::GetIO();

  if (io.WantCaptureMouse)
    return;

  const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  const bool middleDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

  const bool anyDown = leftDown || rightDown || middleDown;

  if (!anyDown) {
    m_manipulating = false;
    m_previousMouse = float2(-1, -1);
  } else if (!m_manipulating)
    m_manipulating = true;

  if (m_mouseRotating && !leftDown)
    m_mouseRotating = false;

  if (m_manipulating) {
    float2 position;
    std::memcpy(&position, &io.MousePos, sizeof(position));

    const float2 mouse = float2(position.x, position.y);

    if (anyDown && m_previousMouse != float2(-1, -1)) {
      const float2 prev = m_previousMouse;

      const float2 mouseFrom = prev * 2.f / float2(m_width, m_height);
      const float2 mouseTo = mouse * 2.f / float2(m_width, m_height);

      const float2 mouseDelta = mouseFrom - mouseTo;

      if (mouseDelta != float2(0.f, 0.f)) {
        if (leftDown) {
          if (!m_mouseRotating) {
            m_arcball.startNewRotation();
            m_mouseRotating = true;
          }

          m_arcball.rotate(mouseDelta);
        } else if (rightDown)
          m_arcball.zoom(mouseDelta.y);
        else if (middleDown)
          m_arcball.pan(mouseDelta);

        updateCamera();
      }
    }

    m_previousMouse = mouse;
  }
}

void ExampleApp::updateCamera() {
  auto eye = m_arcball.eye();
  auto dir = m_arcball.dir();
  auto up = m_arcball.up();

  anari::setParameter(m_device, m_camera, "position", m_arcball.eye());
  anari::setParameter(m_device, m_camera, "direction", m_arcball.dir());
  anari::setParameter(m_device, m_camera, "up", m_arcball.up());
  anari::commitParameters(m_device, m_camera);
}

void ExampleApp::reshape(int width, int height) {
  glViewport(0, 0, width, height);

  glBindTexture(GL_TEXTURE_2D, m_framebufferTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
               GL_FLOAT, 0);

  uint32_t size[2] = {uint32_t(width), uint32_t(height)};
  anari::setParameter(m_device, m_frame, "size", size);
  anari::setParameter(m_device, m_frame, "channel.color",
                      ANARI_UFIXED8_RGBA_SRGB);
  anari::setParameter(m_device, m_frame, "renderer", m_renderer);
  anari::setParameter(m_device, m_frame, "camera", m_camera);
  anari::commitParameters(m_device, m_frame);

  anari::setParameter(m_device, m_camera, "aspect", width / float(height));
  anari::commitParameters(m_device, m_camera);

  m_width = width;
  m_height = height;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    printf("usage: ./miniAnari [.mini file]\n");
    return 0;
  }

  ExampleApp app(argv[1]);
  app.run(1200, 800, "miniScene ANARI Viewer");
}
