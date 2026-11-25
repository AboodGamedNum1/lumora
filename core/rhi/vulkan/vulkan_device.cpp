// Vulkan 1.1 backend, first slice: instance, device, validation, buffers and textures.
// No swapchain, pipelines or command recording yet. Evidence in CI is software (lavapipe).
#include <vulkan/vulkan.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

#include "lumora/rhi/rhi.h"

namespace lumora::rhi {
namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
                                        VkDebugUtilsMessageTypeFlagsEXT, 
                                        const VkDebugUtilsMessengerCallbackDataEXT* data,
                                        void* user) {
  if (sev & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
    ++*static_cast<std::atomic<std::uint32_t>*>(user);
    std::fprintf(stderr, "[vulkan validation] %s\n", data->pMessage);
  }
  return VK_FALSE;
}

VkFormat to_vk(Format f) {
  switch (f) {
    case Format::Rgba8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::Bgra8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
    case Format::R8Unorm: return VK_FORMAT_R8_UNORM;
    case Format::Depth32Float: return VK_FORMAT_D32_SFLOAT;
  }
  return VK_FORMAT_UNDEFINED;
}

struct Slot {
  bool live = false;
  std::uint32_t generation = 1;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  std::uint64_t size = 0;
  std::uint32_t width = 0, height = 0;
  Format format = Format::Rgba8Unorm;
};

class VulkanDevice final : public Device {
 public:
  ~VulkanDevice() override {
    if (device_) vkDeviceWaitIdle(device_);
    for (auto& s : slots_) release(s);
    if (render_pass_) vkDestroyRenderPass(device_, render_pass_, nullptr);
    if (pool_) vkDestroyCommandPool(device_, pool_, nullptr);
    if (device_) vkDestroyDevice(device_, nullptr);
    if (messenger_) {
      auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
          vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
      if (fn) fn(instance_, messenger_, nullptr);
    }
    if (instance_) vkDestroyInstance(instance_, nullptr);
  }

  bool init(const DeviceDesc& desc, std::string* error) {
    auto fail = [&](const char* m) {
      if (error) *error = m;
      return false;
    };
    std::vector<const char*> layers, exts;
    if (desc.enable_validation) {
      std::uint32_t n = 0;
      vkEnumerateInstanceLayerProperties(&n, nullptr);
      std::vector<VkLayerProperties> lp(n);
      vkEnumerateInstanceLayerProperties(&n, lp.data());
      for (auto& l : lp)
        if (!std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation")) {
          layers.push_back("VK_LAYER_KHRONOS_validation");
          exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
          validation_ = true;
        }
    }
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "Lumora";
    ai.apiVersion = VK_API_VERSION_1_1;
    VkDebugUtilsMessengerCreateInfoEXT mci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    mci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    mci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    mci.pfnUserCallback = debug_cb;
    mci.pUserData = &messages_;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    ici.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    ici.ppEnabledLayerNames = layers.data();
    ici.enabledExtensionCount = static_cast<std::uint32_t>(exts.size());
    ici.ppEnabledExtensionNames = exts.data();
    if (validation_) ici.pNext = &mci;  // also covers instance create and destroy
    if (vkCreateInstance(&ici, nullptr, &instance_) != VK_SUCCESS)
      return fail("vkCreateInstance failed (no Vulkan loader or driver?)");
    if (validation_) {
      auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
          vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
      if (fn) fn(instance_, &mci, nullptr, &messenger_);
      set_name_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
          vkGetInstanceProcAddr(instance_, "vkSetDebugUtilsObjectNameEXT"));
    }

    std::uint32_t pc = 0;
    vkEnumeratePhysicalDevices(instance_, &pc, nullptr);
    if (pc == 0) return fail("no Vulkan physical device");
    std::vector<VkPhysicalDevice> pds(pc);
    vkEnumeratePhysicalDevices(instance_, &pc, pds.data());
    phys_ = pds[0];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys_, &props);
    std::uint32_t qc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qc, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qc);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qc, qf.data());
    std::uint32_t family = qc;
    for (std::uint32_t i = 0; i < qc; ++i)
      if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        family = i;
        break;
      }
    if (family == qc) return fail("no graphics queue family");
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    if (vkCreateDevice(phys_, &dci, nullptr, &device_) != VK_SUCCESS)
      return fail("vkCreateDevice failed");
    vkGetPhysicalDeviceMemoryProperties(phys_, &mem_);
    family_ = family;
    vkGetDeviceQueue(device_, family, 0, &queue_);
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = family;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS)
      return fail("vkCreateCommandPool failed");
    VkAttachmentDescription att{};
    att.format = VK_FORMAT_R8G8B8A8_UNORM;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp{};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ref;
    VkSubpassDependency dep{};
    dep.srcSubpass = 0;
    dep.dstSubpass = VK_SUBPASS_EXTERNAL;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo rpi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 1;
    rpi.pAttachments = &att;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    rpi.dependencyCount = 1;
    rpi.pDependencies = &dep;
    if (vkCreateRenderPass(device_, &rpi, nullptr, &render_pass_) != VK_SUCCESS)
      return fail("vkCreateRenderPass failed");

    caps_.backend = Backend::Vulkan;
    caps_.device_name = props.deviceName;
    caps_.max_texture_size = props.limits.maxImageDimension2D;
    VkPhysicalDeviceFeatures f;
    vkGetPhysicalDeviceFeatures(phys_, &f);
    caps_.astc = f.textureCompressionASTC_LDR;
    caps_.etc2 = f.textureCompressionETC2;
    caps_.supports_compute_shaders = (qf[family].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
    slots_.emplace_back();  // slot 0 is the invalid handle
    return true;
  }

  const Capabilities& capabilities() const override { return caps_; }
  std::uint32_t validation_message_count() const override { return messages_.load(); }
  bool validation_active() const override { return validation_ && messenger_ != VK_NULL_HANDLE; }
  std::string_view last_error() const override { return error_; }
  std::uint32_t live_resource_count() const override { return live_; }

  Handle create_buffer(const BufferDesc& d) override {
    if (d.name.empty()) return fail("buffer needs a name");
    if (d.size == 0) return fail("buffer size is zero");
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = d.size;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    switch (d.usage) {
      case BufferUsage::Vertex: bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
      case BufferUsage::Index: bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; break;
      case BufferUsage::Uniform: bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
      case BufferUsage::Storage: bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;
      case BufferUsage::Readback: bi.usage = 0; break;
    }
    bi.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    Slot s;
    s.size = d.size;
    if (vkCreateBuffer(device_, &bi, nullptr, &s.buffer) != VK_SUCCESS)
      return fail("vkCreateBuffer failed");
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device_, s.buffer, &mr);
    if (!allocate(s, mr, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
        vkBindBufferMemory(device_, s.buffer, s.memory, 0) != VK_SUCCESS) {
      release(s);
      return fail("buffer memory allocation failed");
    }
    name_object(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(s.buffer), d.name);
    return store(s);
  }

  Handle create_texture(const TextureDesc& d) override {
    if (d.name.empty()) return fail("texture needs a name");
    if (d.width == 0 || d.height == 0) return fail("texture extent is zero");
    if (d.width > caps_.max_texture_size || d.height > caps_.max_texture_size)
      return fail("texture exceeds max_texture_size");
    if (d.mip_levels == 0) return fail("texture needs at least one mip level");
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = to_vk(d.format);
    ii.extent = {d.width, d.height, 1};
    ii.mipLevels = d.mip_levels;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT |
               (d.format == Format::Depth32Float ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                 : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Slot s;
    s.width = d.width;
    s.height = d.height;
    s.format = d.format;
    if (vkCreateImage(device_, &ii, nullptr, &s.image) != VK_SUCCESS)
      return fail("vkCreateImage failed");
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device_, s.image, &mr);
    if (!allocate(s, mr, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ||
        vkBindImageMemory(device_, s.image, s.memory, 0) != VK_SUCCESS) {
      release(s);
      return fail("image memory allocation failed");
    }
    if (d.format != Format::Depth32Float && d.mip_levels == 1) {
      VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      vi.image = s.image;
      vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
      vi.format = ii.format;
      vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      if (vkCreateImageView(device_, &vi, nullptr, &s.view) != VK_SUCCESS) {
        release(s);
        return fail("vkCreateImageView failed");
      }
    }
    name_object(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(s.image), d.name);
    return store(s);
  }

  Handle create_graphics_pipeline(const PipelineDesc& d) override;
  std::unique_ptr<CommandBuffer> begin_commands() override;
  bool submit(CommandBuffer& cb) override;
  void* map(Handle h) override {
    Slot* s = get(h);
    if (!s || !s->buffer) return nullptr;
    void* p = nullptr;
    if (vkMapMemory(device_, s->memory, 0, VK_WHOLE_SIZE, 0, &p) != VK_SUCCESS) return nullptr;
    return p;
  }
  void unmap(Handle h) override {
    Slot* s = get(h);
    if (s && s->memory) vkUnmapMemory(device_, s->memory);
  }
  Slot* get(Handle h);
  Handle fail_msg(const char* m) { return fail(m); }
  void set_error(const char* m) { error_ = m; }

  void destroy(Handle h) override {
    if (h.index == 0 || h.index >= slots_.size()) return;
    Slot& s = slots_[h.index];
    if (!s.live || s.generation != h.generation) return;
    release(s);
    s.live = false;
    ++s.generation;
    free_.push_back(h.index);
    --live_;
  }

  Handle fail(const char* m) {
    error_ = m;
    return {};
  }
  void release(Slot& s) {
    if (s.pipeline) vkDestroyPipeline(device_, s.pipeline, nullptr);
    if (s.pipeline_layout) vkDestroyPipelineLayout(device_, s.pipeline_layout, nullptr);
    if (s.view) vkDestroyImageView(device_, s.view, nullptr);
    s.pipeline = VK_NULL_HANDLE;
    s.pipeline_layout = VK_NULL_HANDLE;
    s.view = VK_NULL_HANDLE;
    if (s.buffer) vkDestroyBuffer(device_, s.buffer, nullptr);
    if (s.image) vkDestroyImage(device_, s.image, nullptr);
    if (s.memory) vkFreeMemory(device_, s.memory, nullptr);
    s.buffer = VK_NULL_HANDLE;
    s.image = VK_NULL_HANDLE;
    s.memory = VK_NULL_HANDLE;
  }
  bool allocate(Slot& s, const VkMemoryRequirements& mr, VkMemoryPropertyFlags want) {
    for (std::uint32_t i = 0; i < mem_.memoryTypeCount; ++i) {
      if ((mr.memoryTypeBits & (1u << i)) &&
          (mem_.memoryTypes[i].propertyFlags & want) == want) {
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = i;
        return vkAllocateMemory(device_, &ai, nullptr, &s.memory) == VK_SUCCESS;
      }
    }
    // Software and unified-memory devices may lack a pure device-local type for images.
    if (want & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) return allocate(s, mr, 0);
    return false;
  }
  void name_object(VkObjectType type, std::uint64_t handle, const std::string& name) {
    if (!set_name_) return;
    VkDebugUtilsObjectNameInfoEXT ni{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    ni.objectType = type;
    ni.objectHandle = handle;
    ni.pObjectName = name.c_str();
    set_name_(device_, &ni);
  }
  Handle store(Slot& s) {
    std::uint32_t i;
    if (!free_.empty()) {
      i = free_.back();
      free_.pop_back();
      s.generation = slots_[i].generation;
    } else {
      i = static_cast<std::uint32_t>(slots_.size());
      slots_.emplace_back();
    }
    s.live = true;
    slots_[i] = s;
    ++live_;
    return {i, s.generation};
  }

  VkInstance instance_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  std::uint32_t family_ = 0;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;  // RGBA8 color, clear, store, ends in TRANSFER_SRC
  VkPhysicalDevice phys_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
  PFN_vkSetDebugUtilsObjectNameEXT set_name_ = nullptr;
  VkPhysicalDeviceMemoryProperties mem_{};
  bool validation_ = false;
  Capabilities caps_;
  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
  std::uint32_t live_ = 0;
  std::string error_;
  std::atomic<std::uint32_t> messages_{0};
};

class VulkanCommandBuffer final : public CommandBuffer {
 public:
  VulkanCommandBuffer(VulkanDevice* dev, VkCommandBuffer cb) : cb_(cb), dev_(dev) {}
  ~VulkanCommandBuffer() override {
    for (VkFramebuffer f : framebuffers_) vkDestroyFramebuffer(dev_->device_, f, nullptr);
    if (cb_) vkFreeCommandBuffers(dev_->device_, dev_->pool_, 1, &cb_);
  }
  void begin_render_pass(Handle color, const float clear[4]) override {
    Slot* t = dev_->get(color);
    if (!t || !t->view || t->format != Format::Rgba8Unorm) {
      bad_ = true;
      return;
    }
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fi.renderPass = dev_->render_pass_;
    fi.attachmentCount = 1;
    fi.pAttachments = &t->view;
    fi.width = t->width;
    fi.height = t->height;
    fi.layers = 1;
    VkFramebuffer fb = VK_NULL_HANDLE;
    if (vkCreateFramebuffer(dev_->device_, &fi, nullptr, &fb) != VK_SUCCESS) {
      bad_ = true;
      return;
    }
    framebuffers_.push_back(fb);
    VkClearValue cv{};
    for (int i = 0; i < 4; ++i) cv.color.float32[i] = clear[i];
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = dev_->render_pass_;
    bi.framebuffer = fb;
    bi.renderArea.extent = {t->width, t->height};
    bi.clearValueCount = 1;
    bi.pClearValues = &cv;
    vkCmdBeginRenderPass(cb_, &bi, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp{0, 0, static_cast<float>(t->width), static_cast<float>(t->height), 0.0f, 1.0f};
    VkRect2D sc{{0, 0}, {t->width, t->height}};
    vkCmdSetViewport(cb_, 0, 1, &vp);
    vkCmdSetScissor(cb_, 0, 1, &sc);
  }
  void bind_pipeline(Handle p) override {
    Slot* s = dev_->get(p);
    if (!s || !s->pipeline) {
      bad_ = true;
      return;
    }
    vkCmdBindPipeline(cb_, VK_PIPELINE_BIND_POINT_GRAPHICS, s->pipeline);
  }
  void draw(std::uint32_t n) override { vkCmdDraw(cb_, n, 1, 0, 0); }
  void end_render_pass() override { vkCmdEndRenderPass(cb_); }
  void copy_texture_to_buffer(Handle tex, Handle buf) override {
    Slot* t = dev_->get(tex);
    Slot* b = dev_->get(buf);
    if (!t || !b || !t->image || !b->buffer ||
        b->size < std::uint64_t{t->width} * t->height * 4) {
      bad_ = true;
      return;
    }
    VkBufferImageCopy r{};
    r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    r.imageExtent = {t->width, t->height, 1};
    vkCmdCopyImageToBuffer(cb_, t->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, b->buffer, 1, &r);
  }
  VkCommandBuffer cb_;
  bool bad_ = false;
  VulkanDevice* dev_;
  std::vector<VkFramebuffer> framebuffers_;
};

VulkanDevice::Slot* VulkanDevice::get(Handle h) {
  if (h.index == 0 || h.index >= slots_.size()) return nullptr;
  Slot& s = slots_[h.index];
  return (s.live && s.generation == h.generation) ? &s : nullptr;
}

std::unique_ptr<CommandBuffer> VulkanDevice::begin_commands() {
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = pool_;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cb = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_, &ai, &cb) != VK_SUCCESS) {
    error_ = "vkAllocateCommandBuffers failed";
    return nullptr;
  }
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);
  return std::make_unique<VulkanCommandBuffer>(this, cb);
}

bool VulkanDevice::submit(CommandBuffer& base) {
  auto& c = static_cast<VulkanCommandBuffer&>(base);
  if (c.bad_) {
    error_ = "command buffer recorded an invalid command";
    return false;
  }
  if (vkEndCommandBuffer(c.cb_) != VK_SUCCESS) {
    error_ = "vkEndCommandBuffer failed";
    return false;
  }
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence fence = VK_NULL_HANDLE;
  vkCreateFence(device_, &fci, nullptr, &fence);
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &c.cb_;
  bool ok = vkQueueSubmit(queue_, 1, &si, fence) == VK_SUCCESS &&
            vkWaitForFences(device_, 1, &fence, VK_TRUE, 30ull * 1000 * 1000 * 1000) == VK_SUCCESS;
  vkDestroyFence(device_, fence, nullptr);
  if (!ok) error_ = "queue submit or wait failed";
  return ok;
}

Handle VulkanDevice::create_graphics_pipeline(const PipelineDesc& d) {
  if (d.name.empty()) return fail("pipeline needs a name");
  if (d.vertex_spirv.empty() || d.fragment_spirv.empty()) return fail("pipeline needs both shaders");
  if (d.color_format != Format::Rgba8Unorm) return fail("only Rgba8Unorm color is supported");
  Slot s;
  VkShaderModule mods[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  const std::span<const std::uint32_t> code[2] = {d.vertex_spirv, d.fragment_spirv};
  for (int i = 0; i < 2; ++i) {
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = code[i].size() * 4;
    mi.pCode = code[i].data();
    if (vkCreateShaderModule(device_, &mi, nullptr, &mods[i]) != VK_SUCCESS) {
      for (auto m : mods) if (m) vkDestroyShaderModule(device_, m, nullptr);
      return fail("vkCreateShaderModule failed");
    }
  }
  auto drop_modules = [&] { for (auto m : mods) if (m) vkDestroyShaderModule(device_, m, nullptr); };
  VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  if (vkCreatePipelineLayout(device_, &li, nullptr, &s.pipeline_layout) != VK_SUCCESS) {
    drop_modules();
    return fail("vkCreatePipelineLayout failed");
  }
  VkPipelineShaderStageCreateInfo st[2]{};
  st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  st[0].module = mods[0];
  st[1].module = mods[1];
  st[0].pName = st[1].pName = "main";
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo vs{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vs.viewportCount = 1;
  vs.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineColorBlendAttachmentState ba{};
  ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &ba;
  const VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  ds.dynamicStateCount = 2;
  ds.pDynamicStates = dyn;
  VkGraphicsPipelineCreateInfo gi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gi.stageCount = 2;
  gi.pStages = st;
  gi.pVertexInputState = &vi;
  gi.pInputAssemblyState = &ia;
  gi.pViewportState = &vs;
  gi.pRasterizationState = &rs;
  gi.pMultisampleState = &ms;
  gi.pColorBlendState = &cb;
  gi.pDynamicState = &ds;
  gi.layout = s.pipeline_layout;
  gi.renderPass = render_pass_;
  VkResult r = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gi, nullptr, &s.pipeline);
  drop_modules();
  if (r != VK_SUCCESS) {
    release(s);
    return fail("vkCreateGraphicsPipelines failed");
  }
  name_object(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(s.pipeline), d.name);
  return store(s);
}

}  // namespace

std::unique_ptr<Device> create_vulkan_device(const DeviceDesc& desc, std::string* error) {
  auto d = std::make_unique<VulkanDevice>();
  if (!d->init(desc, error)) return nullptr;
  return d;
}

}  // namespace lumora::rhi
