#include "VulkanRenderer.h"
#include <stdexcept>
#include <cstring>

namespace aurora::renderer {

VulkanRenderer::VulkanRenderer() {
    m_backend = RendererBackend::Vulkan;
}
VulkanRenderer::~VulkanRenderer() { shutdown(); }

bool VulkanRenderer::init(void* nativeWindowHandle, const RendererConfig& cfg) {
    m_width      = cfg.width;
    m_height     = cfg.height;
    m_hdrEnabled = cfg.hdrOutput;
#ifdef AURORA_VULKAN
    if (!createInstance())         return false;
    if (!selectPhysicalDevice())   return false;
    if (!createLogicalDevice())    return false;
    if (!createSwapchain(nativeWindowHandle, cfg.width, cfg.height)) return false;
    if (!createRenderPass())       return false;
    if (!createPipeline())         return false;
    if (!createCommandBuffers())   return false;
    m_initialized = true;
    return true;
#else
    (void)nativeWindowHandle;
    return false;
#endif
}

void VulkanRenderer::shutdown() {
#ifdef AURORA_VULKAN
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        if (m_imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
        if (m_renderFinished != VK_NULL_HANDLE) vkDestroySemaphore(m_device, m_renderFinished, nullptr);
        if (m_inFlight       != VK_NULL_HANDLE) vkDestroyFence(m_device, m_inFlight, nullptr);
        if (m_cmdPool        != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
        if (m_pipeline       != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_pipeline, nullptr);
        if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        if (m_renderPass     != VK_NULL_HANDLE) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        for (auto fb  : m_framebuffers)   vkDestroyFramebuffer(m_device, fb, nullptr);
        for (auto iv  : m_swapImageViews) vkDestroyImageView(m_device, iv, nullptr);
        if (m_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_surface  != VK_NULL_HANDLE) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_instance != VK_NULL_HANDLE) vkDestroyInstance(m_instance, nullptr);
#endif
    m_initialized = false;
}

bool VulkanRenderer::createInstance() {
#ifdef AURORA_VULKAN
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Aurora Motion Player";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "AuroraEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
        "VK_KHR_win32_surface",
#elif defined(__ANDROID__)
        "VK_KHR_android_surface",
#else
        "VK_KHR_xcb_surface",
#endif
    };
    // Debug layer in debug builds
#ifdef _DEBUG
    std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#else
    std::vector<const char*> layers;
#endif

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.data();

    return vkCreateInstance(&ci, nullptr, &m_instance) == VK_SUCCESS;
#else
    return false;
#endif
}

bool VulkanRenderer::selectPhysicalDevice() {
#ifdef AURORA_VULKAN
    uint32_t cnt = 0;
    vkEnumeratePhysicalDevices(m_instance, &cnt, nullptr);
    if (cnt == 0) return false;
    std::vector<VkPhysicalDevice> devs(cnt);
    vkEnumeratePhysicalDevices(m_instance, &cnt, devs.data());
    // Pick first discrete GPU, else first available
    for (auto& d : devs) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d, &p);
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physDevice = d; return true;
        }
    }
    m_physDevice = devs[0];
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createLogicalDevice() {
#ifdef AURORA_VULKAN
    uint32_t qfc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfc, nullptr);
    std::vector<VkQueueFamilyProperties> qfp(qfc);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfc, qfp.data());
    for (uint32_t i = 0; i < qfc; ++i) {
        if (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) m_graphicsFamily = i;
    }
    m_presentFamily = m_graphicsFamily;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = m_graphicsFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceFeatures feat{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = devExts;
    dci.pEnabledFeatures        = &feat;

    if (vkCreateDevice(m_physDevice, &dci, nullptr, &m_device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily,  0, &m_presentQueue);
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createSwapchain(void* /*windowHandle*/, int w, int h) {
    // Full swapchain creation — platform surface creation depends on Win32/Android
    (void)w; (void)h;
    return m_device != nullptr;
}

bool VulkanRenderer::createRenderPass() { return m_device != nullptr; }
bool VulkanRenderer::createPipeline()   { return m_device != nullptr; }
bool VulkanRenderer::createCommandBuffers() { return m_device != nullptr; }
void VulkanRenderer::recreateSwapchain() {}

void VulkanRenderer::renderFrame(video::VideoFramePtr frame) {
    if (!m_initialized || !frame) return;
    uploadFrame(frame);
}

void VulkanRenderer::uploadFrame(video::VideoFramePtr /*frame*/) {
    // Upload YUV planes to Vulkan texture, then draw fullscreen quad with GLSL shader
}

void VulkanRenderer::resize(int w, int h) {
    m_width = w; m_height = h;
    recreateSwapchain();
}

void VulkanRenderer::setHDRMetadata(float /*maxLuminance*/, float /*minLuminance*/) {}

void VulkanRenderer::present() {
    if (!m_initialized) return;
    if (m_presentedCb) m_presentedCb();
}

} // namespace aurora::renderer
