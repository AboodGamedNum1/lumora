#include <android/native_window_jni.h>
#include <jni.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "lumora/rhi/rhi.h"
#include "cpu_renderer.h"
#include "lumora/version.h"
#include "zip_store.h"

namespace {

// Renders a clear plus a red triangle through the RHI on the given backend and checks a few pixels.
// Returns "OK <device>" or an error text.
std::string rhi_triangle(lumora::rhi::Backend backend, bool if_chain) {
  using namespace lumora::rhi;
  std::string err;
  auto dev = create_device({backend, false}, &err);
  if (!dev) return "no device: " + err;
  PipelineDesc pd;
  pd.vertex_glsl = if_chain ?
      "#version 300 es\n"
      "void main() {\n"
      "  vec2 p = vec2(-0.8, -0.8);\n"
      "  if (gl_VertexID == 0) p = vec2(0.0, 0.8);\n"
      "  if (gl_VertexID == 1) p = vec2(0.8, -0.8);\n"
      "  gl_Position = vec4(p, 0.0, 1.0);\n"
      "}\n" :
      "#version 300 es\n"
      "void main() {\n"
      "  vec2 p[3] = vec2[](vec2(0.0, 0.8), vec2(0.8, -0.8), vec2(-0.8, -0.8));\n"
      "  gl_Position = vec4(p[gl_VertexID], 0.0, 1.0);\n"
      "}\n";
  pd.fragment_glsl =
      "#version 300 es\n"
      "precision mediump float;\n"
      "out vec4 out_color;\n"
      "void main() { out_color = vec4(1.0, 0.0, 0.0, 1.0); }\n";
  constexpr std::uint32_t kSize = 64;
  Handle target = dev->create_texture({"target", kSize, kSize, 1, Format::Rgba8Unorm});
  Handle readback = dev->create_buffer({"readback", kSize * kSize * 4, BufferUsage::Readback});
  Handle pipe = dev->create_graphics_pipeline(pd);
  if (!target.valid() || !readback.valid() || !pipe.valid()) return "resource failed: " + std::string(dev->last_error());
  const float clear[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  auto cb = dev->begin_commands();
  cb->begin_render_pass(target, clear);
  cb->bind_pipeline(pipe);
  cb->draw(3);
  cb->end_render_pass();
  cb->copy_texture_to_buffer(target, readback);
  if (!dev->submit(*cb)) return "submit failed: " + std::string(dev->last_error());
  const auto* px = static_cast<const unsigned char*>(dev->map(readback));
  if (!px) return "map failed";
  auto at = [&](unsigned x, unsigned y) { return px + (y * kSize + x) * 4; };
  if (at(32, 32)[0] != 255 || at(32, 32)[2] != 0) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "center pixel is not red: center=%u,%u,%u,%u corner=%u,%u,%u,%u on %s", at(32, 32)[0],
                  at(32, 32)[1], at(32, 32)[2], at(32, 32)[3], at(1, 1)[0], at(1, 1)[1], at(1, 1)[2], at(1, 1)[3],
                  dev->capabilities().device_name.c_str());
    return buf;
  }
  if (at(1, 1)[2] != 255 || at(1, 1)[0] != 0) return "corner pixel is not blue";
  return "OK " + dev->capabilities().device_name;
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_dev_lumora_app_NativeBridge_versionString(JNIEnv* env, jobject /*thiz*/) {
  return env->NewStringUTF(lumora::version_string().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_lumora_app_NativeBridge_glesTriangle(JNIEnv* env, jobject /*thiz*/) {
  // The emulator's GLES 3.0 SwiftShader draws nothing with a constant array indexed by gl_VertexID, so
  // the test uses the if-chain vertex shader (observed in CI, see PLAN.md).
  return env->NewStringUTF(rhi_triangle(lumora::rhi::Backend::Gles, true).c_str());
}

// Renders a clear (blue) plus a red triangle straight into the window surface of an Android Surface
// through the RHI, then presents. The Kotlin side reads the image back and checks the pixels.
extern "C" JNIEXPORT jstring JNICALL
Java_dev_lumora_app_NativeBridge_glesWindowTriangle(JNIEnv* env, jobject /*thiz*/, jobject surface) {
  using namespace lumora::rhi;
  ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
  if (!window) return env->NewStringUTF("no native window");
  std::string result;
  {
    std::string err;
    auto dev = create_device({Backend::Gles, false}, &err);
    if (!dev) {
      result = "no device: " + err;
    } else {
      Handle win = dev->create_window_surface(window);
      PipelineDesc pd;
      pd.vertex_glsl =
          "#version 300 es\n"
          "void main() {\n"
          "  vec2 p = vec2(-0.8, -0.8);\n"
          "  if (gl_VertexID == 0) p = vec2(0.0, 0.8);\n"
          "  if (gl_VertexID == 1) p = vec2(0.8, -0.8);\n"
          "  gl_Position = vec4(p, 0.0, 1.0);\n"
          "}\n";
      pd.fragment_glsl =
          "#version 300 es\n"
          "precision mediump float;\n"
          "out vec4 out_color;\n"
          "void main() { out_color = vec4(1.0, 0.0, 0.0, 1.0); }\n";
      Handle pipe = dev->create_graphics_pipeline(pd);
      if (!win.valid() || !pipe.valid()) {
        result = "setup failed: " + std::string(dev->last_error());
      } else {
        const float clear[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        auto cb = dev->begin_commands();
        cb->begin_render_pass(win, clear);
        cb->bind_pipeline(pipe);
        cb->draw(3);
        cb->end_render_pass();
        if (!dev->submit(*cb)) result = "submit failed: " + std::string(dev->last_error());
        else if (!dev->present(win)) result = "present failed: " + std::string(dev->last_error());
        else result = "OK " + dev->capabilities().device_name;
      }
    }
  }
  ANativeWindow_release(window);
  return env->NewStringUTF(result.c_str());
}

namespace {
// One renderer per wallpaper engine surface: device, window surface and the triangle pipeline.
struct Renderer {
  std::unique_ptr<lumora::rhi::Device> dev;
  ANativeWindow* window = nullptr;
  lumora::rhi::Handle surface, pipeline;
  ~Renderer() {
    if (dev && surface.valid()) dev->destroy(surface);
    if (dev && pipeline.valid()) dev->destroy(pipeline);
    dev.reset();
    if (window) ANativeWindow_release(window);
  }
};
}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_dev_lumora_app_NativeBridge_engineCreate(JNIEnv* env, jobject /*thiz*/, jobject surface) {
  using namespace lumora::rhi;
  auto r = std::make_unique<Renderer>();
  r->window = ANativeWindow_fromSurface(env, surface);
  if (!r->window) return 0;
  r->dev = create_device({Backend::Gles, false}, nullptr);
  if (!r->dev) return 0;
  r->surface = r->dev->create_window_surface(r->window);
  PipelineDesc pd;
  pd.vertex_glsl =
      "#version 300 es\n"
      "void main() {\n"
      "  vec2 p = vec2(-0.5, -0.5);\n"
      "  if (gl_VertexID == 0) p = vec2(0.0, 0.5);\n"
      "  if (gl_VertexID == 1) p = vec2(0.5, -0.5);\n"
      "  gl_Position = vec4(p, 0.0, 1.0);\n"
      "}\n";
  pd.fragment_glsl =
      "#version 300 es\n"
      "precision mediump float;\n"
      "out vec4 out_color;\n"
      "void main() { out_color = vec4(1.0, 1.0, 1.0, 1.0); }\n";
  r->pipeline = r->dev->create_graphics_pipeline(pd);
  if (!r->surface.valid() || !r->pipeline.valid()) return 0;
  return reinterpret_cast<jlong>(r.release());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_lumora_app_NativeBridge_engineFrame(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle, jfloat red, jfloat green,
                                             jfloat blue) {
  auto* r = reinterpret_cast<Renderer*>(handle);
  if (!r) return JNI_FALSE;
  const float clear[4] = {red, green, blue, 1.0f};
  auto cb = r->dev->begin_commands();
  cb->begin_render_pass(r->surface, clear);
  cb->bind_pipeline(r->pipeline);
  cb->draw(3);
  cb->end_render_pass();
  return r->dev->submit(*cb) && r->dev->present(r->surface) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_lumora_app_NativeBridge_engineDestroy(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
  delete reinterpret_cast<Renderer*>(handle);
}

// Loads a .lumora package from bytes (ZIP, checksums, manifest, scene, textures) and renders it with the CPU
// reference renderer at 128x128. Returns "OK <name> <files>" followed by four sample pixels, or "ERR <why>".
// Software evidence only: it proves the loader and renderer run on the emulator CPU, nothing about a GPU.
extern "C" JNIEXPORT jstring JNICALL
Java_dev_lumora_app_NativeBridge_renderPackage(JNIEnv* env, jobject /*thiz*/, jbyteArray bytes) {
  using namespace lumora;
  const jsize n = env->GetArrayLength(bytes);
  std::vector<std::uint8_t> z(static_cast<std::size_t>(n));
  env->GetByteArrayRegion(bytes, 0, n, reinterpret_cast<jbyte*>(z.data()));
  auto zr = scene::read_zip_store(z.data(), z.size());
  if (!zr.ok) return env->NewStringUTF(("ERR archive: " + zr.error).c_str());
  auto lr = scene::load_package(zr.files);
  if (!lr.ok()) return env->NewStringUTF(("ERR package: " + lr.message).c_str());
  render2d::TextureMap tex;
  std::string err;
  if (!render2d::load_textures(lr.scene, zr.files, tex, err)) return env->NewStringUTF(("ERR textures: " + err).c_str());
  render2d::View v;
  v.width = v.height = 128;
  const auto img = render2d::render_scene(lr.scene, tex, v);
  if (!img.valid()) return env->NewStringUTF("ERR render");
  std::string out = "OK " + lr.manifest.name + " files=" + std::to_string(zr.files.size());
  const int pts[4][2] = {{64, 2}, {64, 125}, {10, 64}, {64, 64}};
  for (const auto& p : pts) {
    const std::uint8_t* q = &img.rgba[(static_cast<std::size_t>(p[1]) * img.width + static_cast<std::size_t>(p[0])) * 4];
    out += " " + std::to_string(p[0]) + "," + std::to_string(p[1]) + "=" + std::to_string(q[0]) + "," +
           std::to_string(q[1]) + "," + std::to_string(q[2]) + "," + std::to_string(q[3]);
  }
  return env->NewStringUTF(out.c_str());
}
