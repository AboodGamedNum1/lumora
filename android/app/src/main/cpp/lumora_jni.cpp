#include <jni.h>

#include <cstdio>
#include <string>

#include "lumora/rhi/rhi.h"
#include "lumora/version.h"

namespace {

// Renders a clear plus a red triangle through the RHI on the given backend and checks a few pixels.
// Returns "OK <device>" or an error text.
std::string rhi_triangle(lumora::rhi::Backend backend) {
  using namespace lumora::rhi;
  std::string err;
  auto dev = create_device({backend, false}, &err);
  if (!dev) return "no device: " + err;
  PipelineDesc pd;
  pd.vertex_glsl =
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
  return env->NewStringUTF(rhi_triangle(lumora::rhi::Backend::Gles).c_str());
}
