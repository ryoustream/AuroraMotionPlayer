#pragma once
#include "RendererBase.h"
#ifdef AURORA_VULKAN
#  include <vulkan/vulkan.h>
#endif
#include <vector>
#include <memory>

namespace aurora::renderer {

class VulkanRenderer : public RendererBase {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    bool init(void* nativeWindowHandle, const RendererConfig& cfg) override;
    void shutdown() override;
    void renderFrame(video::VideoFramePtr frame) override;
    void resize(int width, int height) override;
    void setHDRMetadata(float maxLuminance, float minLuminance) override;
    void present() override;

private:
    bool createInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain(void* windowHandle, int width, int height);
    bool createRenderPass();
    bool createPipeline();
    bool createCommandBuffers();
    void recreateSwapchain();
    void uploadFrame(video::VideoFramePtr frame);

#ifdef AURORA_VULKAN
    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physDevice     = VK_NULL_HANDLE;
    VkDevice                 m_device         = VK_NULL_HANDLE;
    VkQueue                  m_graphicsQueue  = VK_NULL_HANDLE;
    VkQueue                  m_presentQueue   = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface        = VK_NULL_HANDLE;
    VkSwapchainKHR           m_swapchain      = VK_NULL_HANDLE;
    VkRenderPass             m_renderPass     = VK_NULL_HANDLE;
    VkPipeline               m_pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout         m_pipelineLayout = VK_NULL_HANDLE;
    VkCommandPool            m_cmdPool        = VK_NULL_HANDLE;
    std::vector<VkImage>     m_swapImages;
    std::vector<VkImageView> m_swapImageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_cmdBuffers;
    VkSemaphore              m_imageAvailable = VK_NULL_HANDLE;
    VkSemaphore              m_renderFinished = VK_NULL_HANDLE;
    VkFence                  m_inFlight       = VK_NULL_HANDLE;
    // Video texture
    VkImage                  m_videoTexture   = VK_NULL_HANDLE;
    VkDeviceMemory           m_videoTexMem    = VK_NULL_HANDLE;
    VkImageView              m_videoTexView   = VK_NULL_HANDLE;
    VkSampler                m_sampler        = VK_NULL_HANDLE;
    VkDescriptorSetLayout    m_descLayout     = VK_NULL_HANDLE;
    VkDescriptorPool         m_descPool       = VK_NULL_HANDLE;
    VkDescriptorSet          m_descSet        = VK_NULL_HANDLE;
    uint32_t m_graphicsFamily = 0, m_presentFamily = 0;
    int m_width = 0, m_height = 0;
    bool m_hdrEnabled = false;
#endif
};

} // namespace aurora::renderer
