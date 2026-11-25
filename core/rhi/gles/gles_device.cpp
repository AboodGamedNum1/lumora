// OpenGL ES 3.x backend, first slice: a headless EGL context (surfaceless), buffers, render-target
// textures, a program from GLSL ES source, one render pass (clear), draw, readback.
// Commands are recorded and replayed on submit. In CI the context comes from Mesa llvmpipe
// ("software evidence"), never from a phone GPU.
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "lumora/rhi/rhi.h"

namespace lumora::rhi {
namespace {

GLenum to_internal(Format f) {
  switch (f) {
    case Format::Rgba8Unorm: return GL_RGBA8;
    case Format::Bgra8Unorm: return GL_RGBA8;  // no BGRA render target in core GLES
    case Format::R8Unorm: return GL_R8;
    case Format::Depth32Float: return GL_DEPTH_COMPONENT32F;
  }
  return GL_RGBA8;
}

struct Slot {
  bool live = false;
  std::uint32_t generation = 1;
  GLuint texture = 0, fbo = 0, program = 0;
  std::vector<unsigned char> host;  // buffers
  std::uint32_t width = 0, height = 0;
  bool is_texture = false;
};

class GlesDevice;

class GlesCommandBuffer final : public CommandBuffer {
 public:
  explicit GlesCommandBuffer(GlesDevice* d) : dev_(d) {}
  void begin_render_pass(Handle color, const float clear[4]) override;
  void bind_pipeline(Handle p) override;
  void draw(std::uint32_t n) override;
  void end_render_pass() override;
  void copy_texture_to_buffer(Handle tex, Handle buf) override;
  std::vector<std::function<bool()>> ops_;
  GlesDevice* dev_;
};

class GlesDevice final : public Device {
 public:
  ~GlesDevice() override {
    if (display_ != EGL_NO_DISPLAY) {
      if (context_ != EGL_NO_CONTEXT) {
        eglMakeCurrent(display_, pbuffer_, pbuffer_, context_);
        for (auto& s : slots_) release(s);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (pbuffer_ != EGL_NO_SURFACE) eglDestroySurface(display_, pbuffer_);
        eglDestroyContext(display_, context_);
      }
      eglTerminate(display_);
    }
  }

  bool init(const DeviceDesc& desc, std::string* error) {
    caps_.backend = Backend::Gles;
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    auto get_platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (get_platform) {
      EGLDisplay d = get_platform(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
      if (d != EGL_NO_DISPLAY) display_ = d;
    }
    EGLint major = 0, minor = 0;
    if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, &major, &minor)) return fail(error, "eglInitialize failed");
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return fail(error, "eglBindAPI(GLES) failed");
    const EGLint cfg_attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                               EGL_NONE};
    EGLConfig cfg = nullptr;
    EGLint n = 0;
    if (!eglChooseConfig(display_, cfg_attr, &cfg, 1, &n) || n == 0) cfg = EGL_NO_CONFIG_KHR;
    // Preferred: an ES 3.2 context (EGL 1.5 attributes, with a debug context when validation is on).
    const EGLint ctx_attr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2,
                               EGL_CONTEXT_OPENGL_DEBUG, desc.enable_validation ? EGL_TRUE : EGL_FALSE, EGL_NONE};
    context_ = eglCreateContext(display_, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (context_ == EGL_NO_CONTEXT) {
      // EGL 1.4 drivers (older Android) only know the client version attribute. The context is ES 3.x
      // of whatever minor version the driver gives; capabilities() reports it, the caller decides.
      const EGLint legacy_attr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
      context_ = eglCreateContext(display_, cfg, EGL_NO_CONTEXT, legacy_attr);
    }
    if (context_ == EGL_NO_CONTEXT) return fail(error, "no GLES 3.x context (eglCreateContext failed)");
    if (!eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_)) {
      // No surfaceless-context extension (Android): use a 1x1 pbuffer. All rendering goes to our own FBOs.
      const EGLint pb_attr[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
      pbuffer_ = cfg ? eglCreatePbufferSurface(display_, cfg, pb_attr) : EGL_NO_SURFACE;
      if (pbuffer_ == EGL_NO_SURFACE || !eglMakeCurrent(display_, pbuffer_, pbuffer_, context_))
        return fail(error, "eglMakeCurrent failed (surfaceless and pbuffer)");
    }
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    caps_.device_name = std::string(renderer ? renderer : "?") + " / " + (version ? version : "?");
    GLint max_tex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
    caps_.max_texture_size = static_cast<std::uint32_t>(max_tex);
    caps_.supports_compute_shaders = true;
    if (desc.enable_validation) {
      glEnable(GL_DEBUG_OUTPUT);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glDebugMessageCallback(&GlesDevice::on_debug, this);
      validation_ = true;
    }
    glGenVertexArrays(1, &vao_);
    return true;
  }

  const Capabilities& capabilities() const override { return caps_; }
  bool validation_active() const override { return validation_; }
  std::uint32_t validation_message_count() const override { return messages_; }
  std::string_view last_error() const override { return error_; }
  std::uint32_t live_resource_count() const override { return live_; }

  Handle create_buffer(const BufferDesc& d) override {
    if (d.size == 0) return fail_h("buffer size is zero");
    Handle h = alloc();
    Slot& s = slots_[h.index];
    s.host.assign(d.size, 0);
    return h;
  }

  Handle create_texture(const TextureDesc& d) override {
    if (d.width == 0 || d.height == 0) return fail_h("texture size is zero");
    Handle h = alloc();
    Slot& s = slots_[h.index];
    s.is_texture = true;
    s.width = d.width;
    s.height = d.height;
    glGenTextures(1, &s.texture);
    glBindTexture(GL_TEXTURE_2D, s.texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, to_internal(d.format), static_cast<GLsizei>(d.width),
                   static_cast<GLsizei>(d.height));
    glGenFramebuffers(1, &s.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      destroy(h);
      return fail_h("framebuffer is incomplete");
    }
    return h;
  }

  Handle create_graphics_pipeline(const PipelineDesc& d) override {
    if (d.vertex_glsl.empty() || d.fragment_glsl.empty()) return fail_h("GLES needs GLSL ES source for both stages");
    GLuint vs = compile(GL_VERTEX_SHADER, d.vertex_glsl);
    GLuint fs = vs ? compile(GL_FRAGMENT_SHADER, d.fragment_glsl) : 0;
    if (!vs || !fs) {
      if (vs) glDeleteShader(vs);
      return {};
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
      char log[512] = {};
      glGetProgramInfoLog(prog, sizeof log, nullptr, log);
      glDeleteProgram(prog);
      return fail_h(std::string("program link failed: ") + log);
    }
    Handle h = alloc();
    slots_[h.index].program = prog;
    return h;
  }

  std::unique_ptr<CommandBuffer> begin_commands() override { return std::make_unique<GlesCommandBuffer>(this); }

  bool submit(CommandBuffer& cb) override {
    auto& g = static_cast<GlesCommandBuffer&>(cb);
    eglMakeCurrent(display_, pbuffer_, pbuffer_, context_);
    bool ok = true;
    for (auto& op : g.ops_) ok = op() && ok;
    glFinish();
    if (glGetError() != GL_NO_ERROR) {
      error_ = "GL error after submit";
      ok = false;
    }
    return ok;
  }

  void* map(Handle h) override {
    Slot* s = get(h);
    return s && !s->host.empty() ? s->host.data() : nullptr;
  }

  void destroy(Handle h) override {
    Slot* s = get(h);
    if (!s) return;
    release(*s);
    const std::uint32_t next_generation = s->generation + 1;
    *s = Slot{};
    s->generation = next_generation;
    free_.push_back(h.index);
    --live_;
  }

  Slot* get(Handle h) {
    if (!h.valid() || h.index >= slots_.size()) return nullptr;
    Slot& s = slots_[h.index];
    return s.live && s.generation == h.generation ? &s : nullptr;
  }
  GLuint vao() const { return vao_; }
  void set_error(std::string e) { error_ = std::move(e); }

 private:
  static void GL_APIENTRY on_debug(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar*,
                                   const void* user) {
    auto* self = const_cast<GlesDevice*>(static_cast<const GlesDevice*>(user));
    if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH) ++self->messages_;
  }
  bool fail(std::string* error, const char* msg) {
    error_ = msg;
    if (error) *error = msg;
    return false;
  }
  Handle fail_h(std::string msg) {
    error_ = std::move(msg);
    return {};
  }
  GLuint compile(GLenum type, std::string_view src) {
    GLuint sh = glCreateShader(type);
    const GLchar* p = src.data();
    GLint len = static_cast<GLint>(src.size());
    glShaderSource(sh, 1, &p, &len);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[512] = {};
      glGetShaderInfoLog(sh, sizeof log, nullptr, log);
      error_ = std::string("shader compile failed: ") + log;
      glDeleteShader(sh);
      return 0;
    }
    return sh;
  }
  void release(Slot& s) {
    if (s.fbo) glDeleteFramebuffers(1, &s.fbo);
    if (s.texture) glDeleteTextures(1, &s.texture);
    if (s.program) glDeleteProgram(s.program);
    s.fbo = s.texture = s.program = 0;
  }
  Handle alloc() {
    std::uint32_t i;
    if (!free_.empty()) {
      i = free_.back();
      free_.pop_back();
    } else {
      i = static_cast<std::uint32_t>(slots_.size());
      slots_.push_back({});
      if (i == 0) {  // index 0 is the invalid handle
        i = 1;
        slots_.push_back({});
      }
    }
    slots_[i].live = true;
    ++live_;
    return {i, slots_[i].generation};
  }

  Capabilities caps_;
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLContext context_ = EGL_NO_CONTEXT;
  EGLSurface pbuffer_ = EGL_NO_SURFACE;
  GLuint vao_ = 0;
  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
  std::uint32_t live_ = 0;
  std::uint32_t messages_ = 0;
  bool validation_ = false;
  std::string error_;
};

void GlesCommandBuffer::begin_render_pass(Handle color, const float clear[4]) {
  const float c[4] = {clear[0], clear[1], clear[2], clear[3]};
  ops_.push_back([this, color, c] {
    Slot* t = dev_->get(color);
    if (!t || !t->is_texture) {
      dev_->set_error("render pass target is not a texture");
      return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glViewport(0, 0, static_cast<GLsizei>(t->width), static_cast<GLsizei>(t->height));
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(c[0], c[1], c[2], c[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
  });
}

void GlesCommandBuffer::bind_pipeline(Handle p) {
  ops_.push_back([this, p] {
    Slot* s = dev_->get(p);
    if (!s || !s->program) {
      dev_->set_error("bind_pipeline: not a pipeline");
      return false;
    }
    glUseProgram(s->program);
    glBindVertexArray(dev_->vao());
    return true;
  });
}

void GlesCommandBuffer::draw(std::uint32_t n) {
  ops_.push_back([n] {
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(n));
    return true;
  });
}

void GlesCommandBuffer::end_render_pass() {}

void GlesCommandBuffer::copy_texture_to_buffer(Handle tex, Handle buf) {
  ops_.push_back([this, tex, buf] {
    Slot* t = dev_->get(tex);
    Slot* b = dev_->get(buf);
    if (!t || !b || !t->is_texture || b->host.size() < std::size_t{t->width} * t->height * 4) {
      dev_->set_error("copy_texture_to_buffer: bad handles or buffer too small");
      return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    std::vector<unsigned char> tmp(std::size_t{t->width} * t->height * 4);
    glReadPixels(0, 0, static_cast<GLsizei>(t->width), static_cast<GLsizei>(t->height), GL_RGBA,
                 GL_UNSIGNED_BYTE, tmp.data());
    // GL rows start at the bottom; the RHI buffer is top row first like the other backends.
    const std::size_t row = std::size_t{t->width} * 4;
    for (std::uint32_t y = 0; y < t->height; ++y)
      std::memcpy(b->host.data() + y * row, tmp.data() + (t->height - 1 - y) * row, row);
    return true;
  });
}

}  // namespace

std::unique_ptr<Device> create_gles_device(const DeviceDesc& desc, std::string* error) {
  auto d = std::make_unique<GlesDevice>();
  if (!d->init(desc, error)) return nullptr;
  return d;
}

}  // namespace lumora::rhi
