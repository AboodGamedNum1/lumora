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

class VulkanDevice final : public Device {
 public:
  ~VulkanDevice() override {
    for (auto& s : slots_) release(s);
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
    }
    bi.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    Slot s;
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
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               (d.format == Format::Depth32Float ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                 : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Slot s;
    if (vkCreateImage(device_, &ii, nullptr, &s.image) != VK_SUCCESS)
      return fail("vkCreateImage failed");
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device_, s.image, &mr);
    if (!allocate(s, mr, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ||
        vkBindImageMemory(device_, s.image, s.memory, 0) != VK_SUCCESS) {
      release(s);
      return fail("image memory allocation failed");
    }
    name_object(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(s.image), d.name);
    return store(s);
  }

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

 private:
  struct Slot {
    bool live = false;
    std::uint32_t generation = 1;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
  };
  Handle fail(const char* m) {
    error_ = m;
    return {};
  }
  void release(Slot& s) {
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

}  // namespace

std::unique_ptr<Device> create_vulkan_device(const DeviceDesc& desc, std::string* error) {
  auto d = std::make_unique<VulkanDevice>();
  if (!d->init(desc, error)) return nullptr;
  return d;
}

}  // namespace lumora::rhi
