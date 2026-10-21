#pragma once
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// One submission in flight. All calls (including destruction) are serialized by
// the application's lifecycle mutex. The SDL window outlives this owner.
class Presenter {
public:
    Presenter() = default;
    ~Presenter();
    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;
    void initialize(SDL_Window* window);
    void suspend();
    void resize() { dirty_ = true; }
    bool present(float red, float green);
private:
    void surface();
    bool swapchain();
    void destroy_chain();
    SDL_Window* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    std::uint32_t family_ = 0;
    VkSwapchainKHR chain_ = VK_NULL_HANDLE;
    VkCommandPool pool_ = VK_NULL_HANDLE;
    VkCommandBuffer command_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
    VkSemaphore acquired_ = VK_NULL_HANDLE;
    std::vector<VkSemaphore> presented_;
    std::vector<VkImage> images_;
    bool dirty_ = true;
};
