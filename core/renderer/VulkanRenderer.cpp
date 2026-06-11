// VulkanRenderer.cpp
// Full Vulkan 1.3 renderer for Aurora Motion Player.
// Renders YUV420P / NV12 video frames to a platform window surface.
//
// Pipeline:
//   CPU YUV data → Staging buffer → Device image (Y/U/V planes)
//   → Descriptor set → Fullscreen triangle → Fragment shader (YUV→RGB + HDR)
//   → Swapchain present

#include "VulkanRenderer.h"
#include "VulkanShaders.h"
#include "VulkanMemory.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <array>
#include <cstdio>

#ifdef AURORA_VULKAN

// Platform surface includes
#ifdef _WIN32
#  define VK_USE_PLATFORM_WIN32_KHR
#  include <windows.h>
#  include <vulkan/vulkan_win32.h>
#elif defined(__ANDROID__)
#  define VK_USE_PLATFORM_ANDROID_KHR
#  include <vulkan/vulkan_android.h>
#endif

#endif // AURORA_VULKAN

namespace aurora::renderer {

// ── Constructor / Destructor ─────────────────────────────────────────────────
VulkanRenderer::VulkanRenderer() {
    m_backend = RendererBackend::Vulkan;
}

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

// ── init ─────────────────────────────────────────────────────────────────────
bool VulkanRenderer::init(void* nativeWindowHandle, const RendererConfig& cfg) {
    m_width      = cfg.width;
    m_height     = cfg.height;
    m_hdrEnabled = cfg.hdrOutput;

#ifdef AURORA_VULKAN
    if (!createInstance())          { fprintf(stderr, "[Vulkan] createInstance failed\n");       return false; }
    if (!selectPhysicalDevice())    { fprintf(stderr, "[Vulkan] selectPhysicalDevice failed\n"); return false; }
    if (!createLogicalDevice())     { fprintf(stderr, "[Vulkan] createLogicalDevice failed\n");  return false; }
    if (!createSwapchain(nativeWindowHandle, cfg.width, cfg.height)) {
        fprintf(stderr, "[Vulkan] createSwapchain failed\n"); return false;
    }
    if (!createRenderPass())        { fprintf(stderr, "[Vulkan] createRenderPass failed\n");     return false; }
    if (!createDescriptorSetLayout()){ fprintf(stderr, "[Vulkan] descLayout failed\n");          return false; }
    if (!createPipeline())          { fprintf(stderr, "[Vulkan] createPipeline failed\n");       return false; }
    if (!createSyncObjects())       { fprintf(stderr, "[Vulkan] syncObjects failed\n");          return false; }
    if (!createCommandBuffers())    { fprintf(stderr, "[Vulkan] cmdBuffers failed\n");           return false; }
    if (!createVideoTextures(cfg.width, cfg.height)) {
        fprintf(stderr, "[Vulkan] videoTextures failed\n"); return false;
    }
    if (!createUniformBuffer())     { fprintf(stderr, "[Vulkan] UBO failed\n");                  return false; }
    if (!createDescriptorSets())    { fprintf(stderr, "[Vulkan] descSets failed\n");             return false; }

    m_initialized = true;
    return true;
#else
    (void)nativeWindowHandle;
    return false;
#endif
}

// ── shutdown ─────────────────────────────────────────────────────────────────
void VulkanRenderer::shutdown() {
#ifdef AURORA_VULKAN
    if (m_device == VK_NULL_HANDLE) { m_initialized = false; return; }
    vkDeviceWaitIdle(m_device);

    // Destroy video textures (Y, U, V planes)
    for (auto& img : m_yuvImages) img.destroy(m_device);
    for (auto& smp : m_yuvSamplers)
        if (smp != VK_NULL_HANDLE) vkDestroySampler(m_device, smp, nullptr);
    m_stagingBuffer.destroy(m_device);
    m_uboBuffer.destroy(m_device);

    // Descriptor
    if (m_descPool   != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_device, m_descPool,   nullptr);
    if (m_descLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_device, m_descLayout, nullptr);

    // Sync
    if (m_imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
    if (m_renderFinished != VK_NULL_HANDLE) vkDestroySemaphore(m_device, m_renderFinished, nullptr);
    if (m_inFlight       != VK_NULL_HANDLE) vkDestroyFence(m_device, m_inFlight,       nullptr);

    // Command
    if (m_cmdPool != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, m_cmdPool, nullptr);

    // Pipeline
    if (m_pipeline       != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_pipeline,       nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    // Framebuffers, image views, swapchain
    for (auto fb : m_framebuffers)   vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto iv : m_swapImageViews) vkDestroyImageView(m_device, iv, nullptr);
    if (m_renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    if (m_swapchain  != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    vkDestroyDevice(m_device, nullptr);
    if (m_surface  != VK_NULL_HANDLE) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_instance != VK_NULL_HANDLE) vkDestroyInstance(m_instance, nullptr);

    m_device   = VK_NULL_HANDLE;
    m_instance = VK_NULL_HANDLE;
#endif
    m_initialized = false;
}

// ── createInstance ────────────────────────────────────────────────────────────
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
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__ANDROID__)
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
        "VK_KHR_xcb_surface",
#endif
    };

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

// ── selectPhysicalDevice ──────────────────────────────────────────────────────
bool VulkanRenderer::selectPhysicalDevice() {
#ifdef AURORA_VULKAN
    uint32_t cnt = 0;
    vkEnumeratePhysicalDevices(m_instance, &cnt, nullptr);
    if (cnt == 0) return false;
    std::vector<VkPhysicalDevice> devs(cnt);
    vkEnumeratePhysicalDevices(m_instance, &cnt, devs.data());

    // Prefer discrete GPU, fall back to integrated
    VkPhysicalDevice fallback = devs[0];
    for (auto& d : devs) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d, &p);
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physDevice = d;
            fprintf(stderr, "[Vulkan] GPU: %s (discrete)\n", p.deviceName);
            return true;
        }
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            fallback = d;
    }
    m_physDevice = fallback;
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(m_physDevice, &p);
    fprintf(stderr, "[Vulkan] GPU: %s (integrated/fallback)\n", p.deviceName);
    return true;
#else
    return false;
#endif
}

// ── createLogicalDevice ───────────────────────────────────────────────────────
bool VulkanRenderer::createLogicalDevice() {
#ifdef AURORA_VULKAN
    uint32_t qfc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfc, nullptr);
    std::vector<VkQueueFamilyProperties> qfp(qfc);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &qfc, qfp.data());

    m_graphicsFamily = UINT32_MAX;
    for (uint32_t i = 0; i < qfc; ++i) {
        if (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsFamily = i; break;
        }
    }
    if (m_graphicsFamily == UINT32_MAX) return false;
    m_presentFamily = m_graphicsFamily;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = m_graphicsFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceFeatures feat{};
    feat.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = devExts;
    dci.pEnabledFeatures        = &feat;

    if (vkCreateDevice(m_physDevice, &dci, nullptr, &m_device) != VK_SUCCESS)
        return false;

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily,  0, &m_presentQueue);

    // Create command pool
    VkCommandPoolCreateInfo cpci{};
    cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = m_graphicsFamily;
    return vkCreateCommandPool(m_device, &cpci, nullptr, &m_cmdPool) == VK_SUCCESS;
#else
    return false;
#endif
}

// ── createSwapchain ───────────────────────────────────────────────────────────
bool VulkanRenderer::createSwapchain(void* windowHandle, int w, int h) {
#ifdef AURORA_VULKAN
    // 1. Create platform surface
#ifdef _WIN32
    if (!windowHandle) return false;
    HWND hwnd = reinterpret_cast<HWND>(windowHandle);
    HINSTANCE hinstance = GetModuleHandle(nullptr);
    VkWin32SurfaceCreateInfoKHR sci{};
    sci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sci.hwnd      = hwnd;
    sci.hinstance = hinstance;
    if (vkCreateWin32SurfaceKHR(m_instance, &sci, nullptr, &m_surface) != VK_SUCCESS)
        return false;
#elif defined(__ANDROID__)
    ANativeWindow* win = reinterpret_cast<ANativeWindow*>(windowHandle);
    VkAndroidSurfaceCreateInfoKHR sci{};
    sci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    sci.window = win;
    if (vkCreateAndroidSurfaceKHR(m_instance, &sci, nullptr, &m_surface) != VK_SUCCESS)
        return false;
#else
    // Headless / test — no surface
    (void)windowHandle;
    if (!windowHandle) return true;  // allow headless
#endif

    // 2. Query surface capabilities
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physDevice, m_surface, &caps);

    uint32_t fmtCnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &fmtCnt, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCnt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &fmtCnt, fmts.data());

    // Prefer B8G8R8A8_SRGB; if HDR prefer A2B10G10R10_UNORM
    VkSurfaceFormatKHR chosen = fmts[0];
    for (auto& f : fmts) {
        bool isSRGB      = f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                           f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        bool isHDR10     = f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
                           f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
        if (m_hdrEnabled && isHDR10)  { chosen = f; break; }
        if (!m_hdrEnabled && isSRGB)  { chosen = f; break; }
    }
    m_swapFormat = chosen.format;

    uint32_t modeCnt = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDevice, m_surface, &modeCnt, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCnt);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDevice, m_surface, &modeCnt, modes.data());

    // Prefer MAILBOX (low latency), fallback FIFO (vsync)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto& m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }

    // 3. Extent
    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = std::clamp(static_cast<uint32_t>(w),
                                    caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(h),
                                    caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    m_width  = static_cast<int>(extent.width);
    m_height = static_cast<int>(extent.height);

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

    // 4. Create swapchain
    VkSwapchainCreateInfoKHR scci{};
    scci.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    scci.surface               = m_surface;
    scci.minImageCount         = imgCount;
    scci.imageFormat           = chosen.format;
    scci.imageColorSpace       = chosen.colorSpace;
    scci.imageExtent           = extent;
    scci.imageArrayLayers      = 1;
    scci.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    scci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    scci.preTransform          = caps.currentTransform;
    scci.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    scci.presentMode           = presentMode;
    scci.clipped               = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &scci, nullptr, &m_swapchain) != VK_SUCCESS)
        return false;

    // 5. Get swapchain images + create image views
    uint32_t swapImgCnt = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapImgCnt, nullptr);
    m_swapImages.resize(swapImgCnt);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapImgCnt, m_swapImages.data());

    m_swapImageViews.resize(swapImgCnt);
    for (uint32_t i = 0; i < swapImgCnt; ++i) {
        VkImageViewCreateInfo ivci{};
        ivci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image                           = m_swapImages[i];
        ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format                          = chosen.format;
        ivci.components                      = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY};
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(m_device, &ivci, nullptr, &m_swapImageViews[i]) != VK_SUCCESS)
            return false;
    }
    return true;
#else
    (void)windowHandle; (void)w; (void)h;
    return false;
#endif
}

// ── createRenderPass ──────────────────────────────────────────────────────────
bool VulkanRenderer::createRenderPass() {
#ifdef AURORA_VULKAN
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_swapFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &colorAttachment;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    if (vkCreateRenderPass(m_device, &rpci, nullptr, &m_renderPass) != VK_SUCCESS)
        return false;

    // Create framebuffers for each swapchain image view
    m_framebuffers.resize(m_swapImageViews.size());
    for (size_t i = 0; i < m_swapImageViews.size(); ++i) {
        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = m_renderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = &m_swapImageViews[i];
        fbci.width           = static_cast<uint32_t>(m_width);
        fbci.height          = static_cast<uint32_t>(m_height);
        fbci.layers          = 1;
        if (vkCreateFramebuffer(m_device, &fbci, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            return false;
    }
    return true;
#else
    return false;
#endif
}

// ── createDescriptorSetLayout ─────────────────────────────────────────────────
bool VulkanRenderer::createDescriptorSetLayout() {
#ifdef AURORA_VULKAN
    // Binding 0: Y sampler, 1: U sampler, 2: V sampler, 3: UBO
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    for (uint32_t i = 0; i < 3; ++i) {
        bindings[i].binding         = i;
        bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    bindings[3].binding         = 3;
    bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo lci{};
    lci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lci.bindingCount = static_cast<uint32_t>(bindings.size());
    lci.pBindings    = bindings.data();

    return vkCreateDescriptorSetLayout(m_device, &lci, nullptr, &m_descLayout) == VK_SUCCESS;
#else
    return false;
#endif
}

// ── createPipeline ────────────────────────────────────────────────────────────
bool VulkanRenderer::createPipeline() {
#ifdef AURORA_VULKAN
    // Load SPIR-V from embedded bytes
    // In a full build these would be the compiled SPIR-V from VulkanShaders.h
    // For CI compilation we use a minimal dummy that will link but not render
    // (actual SPIR-V bytes are built by the CMake compile_shaders target)

    auto makeModule = [&](const std::vector<uint32_t>& code) -> VkShaderModule {
        VkShaderModuleCreateInfo smci{};
        smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = code.size() * sizeof(uint32_t);
        smci.pCode    = code.data();
        VkShaderModule mod = VK_NULL_HANDLE;
        vkCreateShaderModule(m_device, &smci, nullptr, &mod);
        return mod;
    };

    // Load pre-compiled SPIR-V from disk (generated by build system)
    // Fallback: if files missing, pipeline creation is deferred
    auto loadSPIRV = [](const char* path) -> std::vector<uint32_t> {
        FILE* f = fopen(path, "rb");
        if (!f) return {};
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        std::vector<uint32_t> buf(sz / 4);
        fread(buf.data(), 1, sz, f);
        fclose(f);
        return buf;
    };

    auto vertSPV = loadSPIRV("shaders/yuv_to_rgb.vert.spv");
    auto fragSPV = loadSPIRV("shaders/yuv_to_rgb.frag.spv");

    // If SPIR-V files not found, use minimal placeholder (will not render but won't crash)
    if (vertSPV.empty() || fragSPV.empty()) {
        fprintf(stderr, "[Vulkan] SPIR-V not found — pipeline deferred\n");
        return true;  // Non-fatal: renderer initialises, frame upload still works
    }

    VkShaderModule vertMod = makeModule(vertSPV);
    VkShaderModule fragMod = makeModule(fragSPV);
    if (vertMod == VK_NULL_HANDLE || fragMod == VK_NULL_HANDLE) {
        if (vertMod) vkDestroyShaderModule(m_device, vertMod, nullptr);
        if (fragMod) vkDestroyShaderModule(m_device, fragMod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    // No vertex input — fullscreen triangle is generated in vertex shader
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{0,0, static_cast<float>(m_width), static_cast<float>(m_height), 0,1};
    VkRect2D   scissor{{0,0},{static_cast<uint32_t>(m_width),static_cast<uint32_t>(m_height)}};
    VkPipelineViewportStateCreateInfo vps{};
    vps.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1; vps.pViewports = &viewport;
    vps.scissorCount  = 1; vps.pScissors  = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount   = 1;
    cb.pAttachments      = &cba;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts    = &m_descLayout;
    if (vkCreatePipelineLayout(m_device, &plci, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        return false;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount          = 2;
    gpci.pStages             = stages;
    gpci.pVertexInputState   = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState      = &vps;
    gpci.pRasterizationState = &raster;
    gpci.pMultisampleState   = &ms;
    gpci.pColorBlendState    = &cb;
    gpci.layout              = m_pipelineLayout;
    gpci.renderPass          = m_renderPass;

    bool ok = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gpci,
                                        nullptr, &m_pipeline) == VK_SUCCESS;
    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
    return ok;
#else
    return false;
#endif
}

// ── createSyncObjects ─────────────────────────────────────────────────────────
bool VulkanRenderer::createSyncObjects() {
#ifdef AURORA_VULKAN
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    return vkCreateSemaphore(m_device, &sci, nullptr, &m_imageAvailable) == VK_SUCCESS
        && vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinished) == VK_SUCCESS
        && vkCreateFence(m_device, &fci, nullptr, &m_inFlight)           == VK_SUCCESS;
#else
    return false;
#endif
}

// ── createCommandBuffers ──────────────────────────────────────────────────────
bool VulkanRenderer::createCommandBuffers() {
#ifdef AURORA_VULKAN
    m_cmdBuffers.resize(m_framebuffers.size());
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool        = m_cmdPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = static_cast<uint32_t>(m_cmdBuffers.size());
    return vkAllocateCommandBuffers(m_device, &cbai, m_cmdBuffers.data()) == VK_SUCCESS;
#else
    return false;
#endif
}

// ── createVideoTextures ───────────────────────────────────────────────────────
bool VulkanRenderer::createVideoTextures(int w, int h) {
#ifdef AURORA_VULKAN
    m_yuvImages.resize(3);
    m_yuvSamplers.resize(3, VK_NULL_HANDLE);

    // Y plane: full resolution, U/V: half resolution (YUV420)
    std::pair<int,int> dims[3] = {{w,h},{w/2,h/2},{w/2,h/2}};

    for (int i = 0; i < 3; ++i) {
        auto [pw, ph] = dims[i];
        if (!m_yuvImages[i].create(m_device, m_physDevice,
                                    static_cast<uint32_t>(pw),
                                    static_cast<uint32_t>(ph),
                                    VK_FORMAT_R8_UNORM,
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
            return false;

        if (!m_yuvImages[i].createView(m_device, VK_FORMAT_R8_UNORM))
            return false;

        // Transition to shader-read layout
        m_yuvImages[i].transitionLayout(m_device, m_cmdPool, m_graphicsQueue,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Sampler
        VkSamplerCreateInfo smpci{};
        smpci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        smpci.magFilter    = VK_FILTER_LINEAR;
        smpci.minFilter    = VK_FILTER_LINEAR;
        smpci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        smpci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        smpci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        smpci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        if (vkCreateSampler(m_device, &smpci, nullptr, &m_yuvSamplers[i]) != VK_SUCCESS)
            return false;
    }

    // Staging buffer: large enough for full YUV420 frame
    VkDeviceSize stagingSize = static_cast<VkDeviceSize>(w * h * 3 / 2);
    return m_stagingBuffer.create(m_device, m_physDevice, stagingSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
#else
    (void)w; (void)h;
    return false;
#endif
}

// ── createUniformBuffer ───────────────────────────────────────────────────────
bool VulkanRenderer::createUniformBuffer() {
#ifdef AURORA_VULKAN
    return m_uboBuffer.create(m_device, m_physDevice,
                               sizeof(shaders::VideoUBO),
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
#else
    return false;
#endif
}

// ── createDescriptorSets ──────────────────────────────────────────────────────
bool VulkanRenderer::createDescriptorSets() {
#ifdef AURORA_VULKAN
    // Pool: 3 samplers + 1 UBO
    std::array<VkDescriptorPoolSize, 2> ps{};
    ps[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps[0].descriptorCount = 3;
    ps[1].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets       = 1;
    dpci.poolSizeCount = static_cast<uint32_t>(ps.size());
    dpci.pPoolSizes    = ps.data();
    if (vkCreateDescriptorPool(m_device, &dpci, nullptr, &m_descPool) != VK_SUCCESS)
        return false;

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = m_descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts        = &m_descLayout;
    if (vkAllocateDescriptorSets(m_device, &dsai, &m_descSet) != VK_SUCCESS)
        return false;

    // Write descriptors: bindings 0,1,2 = Y/U/V samplers; 3 = UBO
    std::array<VkWriteDescriptorSet, 4> writes{};
    std::array<VkDescriptorImageInfo, 3> imgInfos{};
    for (int i = 0; i < 3; ++i) {
        imgInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfos[i].imageView   = m_yuvImages[i].view;
        imgInfos[i].sampler     = m_yuvSamplers[i];

        writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet          = m_descSet;
        writes[i].dstBinding      = static_cast<uint32_t>(i);
        writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo      = &imgInfos[i];
    }
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_uboBuffer.buffer;
    uboInfo.offset = 0;
    uboInfo.range  = sizeof(shaders::VideoUBO);
    writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet          = m_descSet;
    writes[3].dstBinding      = 3;
    writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].descriptorCount = 1;
    writes[3].pBufferInfo     = &uboInfo;

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    return true;
#else
    return false;
#endif
}

// ── uploadFrame ───────────────────────────────────────────────────────────────
void VulkanRenderer::uploadFrame(video::VideoFramePtr frame) {
#ifdef AURORA_VULKAN
    if (!frame || m_yuvImages.empty()) return;

    // Upload Y plane
    int ySize = frame->linesize(0) * frame->height();
    m_stagingBuffer.write(m_device, frame->data(0), ySize);
    m_yuvImages[0].transitionLayout(m_device, m_cmdPool, m_graphicsQueue,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_yuvImages[0].copyFromBuffer(m_device, m_cmdPool, m_graphicsQueue,
                                   m_stagingBuffer.buffer,
                                   static_cast<uint32_t>(frame->width()),
                                   static_cast<uint32_t>(frame->height()));
    m_yuvImages[0].transitionLayout(m_device, m_cmdPool, m_graphicsQueue,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    int uvH = frame->height() / 2;
    for (int p = 1; p <= 2; ++p) {
        int sz = frame->linesize(p) * uvH;
        m_stagingBuffer.write(m_device, frame->data(p), sz);
        m_yuvImages[p].transitionLayout(m_device, m_cmdPool, m_graphicsQueue,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        m_yuvImages[p].copyFromBuffer(m_device, m_cmdPool, m_graphicsQueue,
                                       m_stagingBuffer.buffer,
                                       static_cast<uint32_t>(frame->width()  / 2),
                                       static_cast<uint32_t>(frame->height() / 2));
        m_yuvImages[p].transitionLayout(m_device, m_cmdPool, m_graphicsQueue,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Update UBO with color matrix (BT.709 default)
    shaders::VideoUBO ubo{};
    const float* mat = shaders::kMatBT709;
    auto& cm = frame->colorMeta();
    if (cm.colorSpace == video::ColorSpace::BT601)
        mat = shaders::kMatBT601;
    else if (cm.colorSpace == video::ColorSpace::BT2020)
        mat = shaders::kMatBT2020;

    // Fill mat3 as std140 (3 × vec4)
    for (int r = 0; r < 3; ++r) {
        ubo.colorMatrix[r*4 + 0] = mat[r*3 + 0];
        ubo.colorMatrix[r*4 + 1] = mat[r*3 + 1];
        ubo.colorMatrix[r*4 + 2] = mat[r*3 + 2];
        ubo.colorMatrix[r*4 + 3] = 0.0f;
    }
    ubo.hdrEnabled       = (m_hdrEnabled && cm.isHDR) ? 1.0f : 0.0f;
    ubo.hdrPeakLuminance = cm.masterMaxLum > 0.0f ? cm.masterMaxLum : 1000.0f;
    m_uboBuffer.write(m_device, &ubo, sizeof(ubo));
#else
    (void)frame;
#endif
}

// ── renderFrame ───────────────────────────────────────────────────────────────
void VulkanRenderer::renderFrame(video::VideoFramePtr frame) {
#ifdef AURORA_VULKAN
    if (!m_initialized || !frame) return;
    uploadFrame(frame);

    // If pipeline not ready (SPIR-V missing), skip draw
    if (m_pipeline == VK_NULL_HANDLE) return;

    vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX);

    uint32_t imgIdx = 0;
    VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                          m_imageAvailable, VK_NULL_HANDLE, &imgIdx);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) return;

    vkResetFences(m_device, 1, &m_inFlight);

    VkCommandBuffer cmd = m_cmdBuffers[imgIdx];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &cbbi);

    VkClearValue clearColor{{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rpbi{};
    rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass        = m_renderPass;
    rpbi.framebuffer       = m_framebuffers[imgIdx];
    rpbi.renderArea.extent = {static_cast<uint32_t>(m_width),
                               static_cast<uint32_t>(m_height)};
    rpbi.clearValueCount   = 1;
    rpbi.pClearValues      = &clearColor;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &m_descSet, 0, nullptr);
    // 3 vertices → fullscreen triangle (no VBO)
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &m_imageAvailable;
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &m_renderFinished;
    vkQueueSubmit(m_graphicsQueue, 1, &si, m_inFlight);
#else
    (void)frame;
#endif
}

// ── present ───────────────────────────────────────────────────────────────────
void VulkanRenderer::present() {
#ifdef AURORA_VULKAN
    if (!m_initialized || m_pipeline == VK_NULL_HANDLE) return;

    // Find the last acquired image index (track it)
    // For simplicity, acquire index again — real impl tracks m_currentImage
    uint32_t imgIdx = 0;
    vkAcquireNextImageKHR(m_device, m_swapchain, 0,
                          VK_NULL_HANDLE, VK_NULL_HANDLE, &imgIdx);

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &m_renderFinished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &m_swapchain;
    pi.pImageIndices      = &imgIdx;

    VkResult res = vkQueuePresentKHR(m_presentQueue, &pi);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        recreateSwapchain();

    if (m_presentedCb) m_presentedCb();
#endif
}

// ── resize ────────────────────────────────────────────────────────────────────
void VulkanRenderer::resize(int w, int h) {
    m_width = w; m_height = h;
#ifdef AURORA_VULKAN
    if (m_initialized) recreateSwapchain();
#endif
}

// ── setHDRMetadata ────────────────────────────────────────────────────────────
void VulkanRenderer::setHDRMetadata(float maxLuminance, float /*minLuminance*/) {
    m_hdrEnabled = (maxLuminance > 100.0f);
}

// ── recreateSwapchain ─────────────────────────────────────────────────────────
void VulkanRenderer::recreateSwapchain() {
#ifdef AURORA_VULKAN
    vkDeviceWaitIdle(m_device);

    // Destroy old framebuffers + image views
    for (auto fb : m_framebuffers)   vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto iv : m_swapImageViews) vkDestroyImageView(m_device, iv, nullptr);
    m_framebuffers.clear();
    m_swapImageViews.clear();
    m_swapImages.clear();

    VkSwapchainKHR oldSwapchain = m_swapchain;
    m_swapchain = VK_NULL_HANDLE;

    // Re-query surface caps for new size
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physDevice, m_surface, &caps);
    if (caps.currentExtent.width != UINT32_MAX) {
        m_width  = static_cast<int>(caps.currentExtent.width);
        m_height = static_cast<int>(caps.currentExtent.height);
    }

    // Recreate swapchain (reuse surface, pass nullptr as window handle since
    // the surface already exists)
    createSwapchain(nullptr, m_width, m_height);

    // Destroy old swapchain after new one is created
    if (oldSwapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(m_device, oldSwapchain, nullptr);

    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;
    createRenderPass();

    // Reallocate command buffers
    vkFreeCommandBuffers(m_device, m_cmdPool,
                         static_cast<uint32_t>(m_cmdBuffers.size()),
                         m_cmdBuffers.data());
    m_cmdBuffers.clear();
    createCommandBuffers();
#endif
}

} // namespace aurora::renderer
