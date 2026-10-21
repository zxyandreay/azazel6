#include "presenter.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void sdl(bool result) {
    if (!result) throw std::runtime_error(SDL_GetError());
}
void vk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult=" + std::to_string(result));
}
constexpr std::uint64_t wait_ns = 100000000; // Bounded acquire/fence waits, not guest time.
}

Presenter::~Presenter() {
    if (device_) vkDeviceWaitIdle(device_);
    destroy_chain();
    if (device_) {
        if (fence_) vkDestroyFence(device_, fence_, nullptr);
        if (pool_) vkDestroyCommandPool(device_, pool_, nullptr);
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_) SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

void Presenter::surface() {
    if (!surface_)
        sdl(SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_));
}

void Presenter::initialize(SDL_Window* window) {
    require(!instance_ && window, "Presenter initialization must be single-use");
    window_ = window;
    std::uint32_t version = 0;
    vk(vkEnumerateInstanceVersion(&version), "instance version");
    require(version >= VK_API_VERSION_1_1, "Vulkan 1.1 required");
    Uint32 count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    require(extensions && count, SDL_GetError());
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Azazel6 synthetic host probe";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &app;
    create.enabledExtensionCount = count;
    create.ppEnabledExtensionNames = extensions;
    vk(vkCreateInstance(&create, nullptr, &instance_), "create instance");
    SDL_Log("Vulkan instance created");
    surface();
    SDL_Log("Vulkan Android surface created");
    vk(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "physical device count");
    SDL_Log("Vulkan physical device count: %u", count);
    require(count && count <= 32, "Invalid physical device count");
    std::vector<VkPhysicalDevice> devices(count);
    vk(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "physical devices");
    devices.resize(count);
    for (auto candidate : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        SDL_Log("Vulkan candidate: %s API=%u", properties.deviceName, properties.apiVersion);
        if (properties.apiVersion < VK_API_VERSION_1_1) continue;
        std::uint32_t n = 0;
        vk(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &n, nullptr), "extension count");
        require(n <= 1024, "Too many device extensions");
        std::vector<VkExtensionProperties> supported(n);
        vk(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &n, supported.data()), "extensions");
        supported.resize(n);
        if (std::none_of(supported.begin(), supported.end(), [](const auto& e) {
                return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            })) continue;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, nullptr);
        SDL_Log("Vulkan queue family count: %u", n);
        require(n && n <= 64, "Invalid queue family count");
        std::vector<VkQueueFamilyProperties> families(n);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &n, families.data());
        for (std::uint32_t i = 0; i < n; ++i) {
            VkBool32 present = VK_FALSE;
            vk(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &present), "surface support");
            if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                physical_ = candidate;
                family_ = i;
                SDL_Log("Vulkan 1.1 device: %s", properties.deviceName);
                break;
            }
        }
        if (physical_) break;
    }
    require(physical_, "No Vulkan 1.1 graphics/present queue");
    const float priority = 1;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = family_; queue.queueCount = 1; queue.pQueuePriorities = &priority;
    const char* swap = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device.queueCreateInfoCount = 1; device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = 1; device.ppEnabledExtensionNames = &swap;
    vk(vkCreateDevice(physical_, &device, nullptr, &device_), "create device");
    vkGetDeviceQueue(device_, family_, 0, &queue_);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.queueFamilyIndex = family_;
    vk(vkCreateCommandPool(device_, &pool, nullptr, &pool_), "command pool");
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = pool_; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    vk(vkAllocateCommandBuffers(device_, &allocate, &command_), "command buffer");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vk(vkCreateFence(device_, &fence, nullptr, &fence_), "fence");
}

void Presenter::destroy_chain() {
    for (auto semaphore : presented_) if (semaphore) vkDestroySemaphore(device_, semaphore, nullptr);
    presented_.clear(); images_.clear();
    if (acquired_) vkDestroySemaphore(device_, acquired_, nullptr);
    acquired_ = VK_NULL_HANDLE;
    if (chain_) vkDestroySwapchainKHR(device_, chain_, nullptr);
    chain_ = VK_NULL_HANDLE;
    dirty_ = true;
}

void Presenter::suspend() {
    if (device_) vk(vkDeviceWaitIdle(device_), "suspend device idle");
    destroy_chain();
    if (surface_) SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
}

bool Presenter::swapchain() {
    vk(vkDeviceWaitIdle(device_), "recreate device idle");
    destroy_chain();
    surface();
    VkBool32 supported = VK_FALSE;
    vk(vkGetPhysicalDeviceSurfaceSupportKHR(physical_, family_, surface_, &supported), "renewed surface support");
    require(supported, "Queue cannot present to renewed surface");
    VkSurfaceCapabilitiesKHR caps{};
    vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &caps), "surface capabilities");
    require(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT, "Surface cannot receive diagnostic clear");
    int width = 0, height = 0;
    sdl(SDL_GetWindowSizeInPixels(window_, &width, &height));
    if (width <= 0 || height <= 0) return false;
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(static_cast<std::uint32_t>(width), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<std::uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (!extent.width || !extent.height) return false;
    std::uint32_t count = 0;
    vk(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &count, nullptr), "format count");
    require(count && count <= 256, "Invalid surface format count");
    std::vector<VkSurfaceFormatKHR> formats(count);
    vk(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &count, formats.data()), "surface formats");
    formats.resize(count);
    VkSurfaceFormatKHR format{};
    for (auto f : formats) {
        if (f.format == VK_FORMAT_UNDEFINED) f.format = VK_FORMAT_B8G8R8A8_UNORM;
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { format = f; break; }
    }
    require(format.format != VK_FORMAT_UNDEFINED, "No supported RGBA/BGRA surface format");
    require(caps.minImageCount < 8, "Surface requires too many images");
    count = caps.minImageCount + 1;
    if (caps.maxImageCount) count = std::min(count, caps.maxImageCount);
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (auto a : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                   VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR})
        if (caps.supportedCompositeAlpha & a) { alpha = a; break; }
    require(caps.supportedCompositeAlpha & alpha, "No composite alpha mode");
    VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create.surface = surface_; create.minImageCount = count;
    create.imageFormat = format.format; create.imageColorSpace = format.colorSpace;
    create.imageExtent = extent; create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = caps.currentTransform; create.compositeAlpha = alpha;
    create.presentMode = VK_PRESENT_MODE_FIFO_KHR; create.clipped = VK_TRUE;
    vk(vkCreateSwapchainKHR(device_, &create, nullptr, &chain_), "create swapchain");
    vk(vkGetSwapchainImagesKHR(device_, chain_, &count, nullptr), "image count");
    require(count && count <= 8, "Invalid swapchain image count");
    images_.resize(count);
    vk(vkGetSwapchainImagesKHR(device_, chain_, &count, images_.data()), "images");
    images_.resize(count);
    presented_.resize(count, VK_NULL_HANDLE);
    VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vk(vkCreateSemaphore(device_, &sem, nullptr, &acquired_), "acquire semaphore");
    // Image acquisition proves its previous presentation wait has consumed this
    // image's semaphore. A submission fence alone cannot prove that.
    for (auto& semaphore : presented_) vk(vkCreateSemaphore(device_, &sem, nullptr, &semaphore), "present semaphore");
    dirty_ = false;
    SDL_Log("Synthetic swapchain: %ux%u, %u images", extent.width, extent.height, count);
    return true;
}

bool Presenter::present(float red, float green) {
    if (dirty_ && !swapchain()) return false;
    auto result = vkWaitForFences(device_, 1, &fence_, VK_TRUE, wait_ns);
    if (result == VK_TIMEOUT) return false;
    vk(result, "wait frame");
    std::uint32_t index = 0;
    result = vkAcquireNextImageKHR(device_, chain_, wait_ns, acquired_, VK_NULL_HANDLE, &index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { dirty_ = true; return false; }
    if (result == VK_ERROR_SURFACE_LOST_KHR) { suspend(); return false; }
    if (result == VK_TIMEOUT || result == VK_NOT_READY) return false;
    if (result == VK_SUBOPTIMAL_KHR) dirty_ = true;
    else vk(result, "acquire image");
    require(index < images_.size(), "Invalid acquired image index");
    vk(vkResetCommandPool(device_, pool_, 0), "reset commands");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk(vkBeginCommandBuffer(command_, &begin), "begin commands");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = images_[index];
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    const VkClearColorValue color{{red, green, 0.18f, 1.0f}};
    vkCmdClearColorImage(command_, images_[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        &color, 1, &barrier.subresourceRange);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(command_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    vk(vkEndCommandBuffer(command_), "end commands");
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &acquired_; submit.pWaitDstStageMask = &stage;
    submit.commandBufferCount = 1; submit.pCommandBuffers = &command_;
    submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &presented_[index];
    vk(vkResetFences(device_, 1, &fence_), "reset fence");
    vk(vkQueueSubmit(queue_, 1, &submit, fence_), "submit clear");
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1; present.pWaitSemaphores = &presented_[index];
    present.swapchainCount = 1; present.pSwapchains = &chain_; present.pImageIndices = &index;
    result = vkQueuePresentKHR(queue_, &present);
    if (result == VK_ERROR_SURFACE_LOST_KHR) { suspend(); return false; }
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) dirty_ = true;
    else vk(result, "present clear");
    return true;
}
