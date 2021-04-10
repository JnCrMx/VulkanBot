#pragma once

#include <bits/stdint-uintn.h>
#include <functional>
#include <tuple>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ResourceLimits.h>
#include <ShaderLang.h>
#include <GlslangToSpv.h>

namespace vulkanbot
{
	struct Vertex {
		glm::vec3 pos;

		static vk::VertexInputBindingDescription getBindingDescription()
		{
			return vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
		}

		static std::array<vk::VertexInputAttributeDescription, 1> getAttributeDescriptions()
		{
			std::array<vk::VertexInputAttributeDescription, 1> attributeDescriptions{
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, pos))
			};

			return attributeDescriptions;
		}
	};

	struct UniformBufferObject {
		float time;
		float random;
	};

	struct ImageData {
		vk::Format format;
    	vk::UniqueImage image;
    	vk::UniqueDeviceMemory deviceMemory;
    	vk::UniqueImageView imageView;

		ImageData(	vk::PhysicalDevice const & physicalDevice,
					vk::UniqueDevice const & device,
					vk::Format format_, 
					vk::Extent2D const & extent, 
					vk::ImageUsageFlags usage,
					vk::MemoryPropertyFlags memoryProperties,
					vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor);
	};

	class VulkanBackend
	{
		public:
			~VulkanBackend();

			void initVulkan(int width, int height);
			void uploadShader(vk::UniqueShaderModule& vertexShader, vk::UniqueShaderModule& fragment,
				vk::CullModeFlags cullMode = vk::CullModeFlagBits::eFront, bool depth = true);

			std::tuple<bool, std::string> uploadShaderMix(const std::string vertex, bool vertexFile, const std::string fragment, bool fragmentFile,
				vk::CullModeFlags cullMode = vk::CullModeFlagBits::eFront, bool depth = true);

			void readImage(const std::vector<unsigned char>& data);
			void updateUniformObject(std::function<void(UniformBufferObject*)> updater);

			template<typename ImageProvider>
			void loadImage(ImageProvider const & provider)
			{
				uint8_t * pData = static_cast<uint8_t *>(m_device->mapMemory(m_inputImageMemory.get(), 0, m_inputImageMemoryRequirements.size));
				provider(pData);
				m_device->unmapMemory(m_inputImageMemory.get());
			}

			void renderFrame(std::function<void(uint8_t*, vk::DeviceSize, int, int, vk::Result, long)> consumer);
		private:
			uint32_t m_width = 1024;
			uint32_t m_height = 1024;

			uint32_t m_textureWidth = 128;
			uint32_t m_textureHeight = 128;

			vk::UniqueShaderModule createShader(const std::vector<unsigned int>& code);
			vk::UniqueShaderModule createShader(const std::vector<char>& code);
			vk::UniquePipeline createPipeline(vk::UniqueShaderModule& vertexShader, vk::UniqueShaderModule& fragment,
				vk::CullModeFlags cullMode = vk::CullModeFlagBits::eFront, bool depth = true);

			vk::UniqueInstance m_instance;
			vk::PhysicalDevice m_physicalDevice;
			vk::UniqueDevice m_device;
			vk::Queue m_queue;
			vk::UniqueFence m_fence;
			
			vk::UniqueCommandPool m_commandPool;
			vk::UniqueCommandBuffer m_commandBuffer;

			vk::UniqueBuffer m_inputImageBuffer;
			vk::MemoryRequirements m_inputImageMemoryRequirements;
			vk::UniqueDeviceMemory m_inputImageMemory;

			vk::UniqueBuffer m_outputImageBuffer;
			vk::MemoryRequirements m_outputImageMemoryRequirements;
			vk::UniqueDeviceMemory m_outputImageMemory;

			vk::UniqueBuffer m_vertexBuffer;
			vk::UniqueDeviceMemory m_vertexMemory;
			vk::UniqueBuffer m_indexBuffer;
			vk::UniqueDeviceMemory m_indexMemory;
			int m_indexCount;

			vk::UniqueBuffer m_uniformBuffer;
			vk::UniqueDeviceMemory m_uniformMemory;

			ImageData* m_texture;
			vk::UniqueSampler m_textureSampler;

			ImageData* m_renderImage;
			ImageData* m_depthImage;
			vk::UniqueRenderPass m_renderPass;
			vk::UniqueFramebuffer m_framebuffer;

			vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
			vk::UniqueDescriptorPool m_descriptorPool;
			vk::UniqueDescriptorSet m_descriptorSet;

			vk::UniquePipelineLayout m_pipelineLayout;
			vk::UniquePipeline m_pipeline;
	};
}