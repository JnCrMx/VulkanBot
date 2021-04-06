#pragma once

#include <bits/stdint-uintn.h>
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
		glm::vec3 color;

		static vk::VertexInputBindingDescription getBindingDescription()
		{
			return vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
		}

		static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
		{
			std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, pos)),
				vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, color))
			};

			return attributeDescriptions;
		}
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
					vk::MemoryPropertyFlags memoryProperties);
	};

	class VulkanBackend
	{
		public:
			~VulkanBackend();

			void initVulkan(int width, int height);
			void uploadShader(const std::vector<unsigned int>& code);
			std::tuple<bool, std::string> uploadShader(const std::string glslCode);

			void readImage(const std::vector<unsigned char>& data);

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
			vk::UniquePipeline createPipeline(const std::vector<unsigned int>& code);

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

			ImageData* m_texture;
			vk::UniqueSampler m_textureSampler;

			ImageData* m_renderImage;
			vk::UniqueRenderPass m_renderPass;
			vk::UniqueFramebuffer m_framebuffer;

			vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
			vk::UniqueDescriptorPool m_descriptorPool;
			vk::UniqueDescriptorSet m_descriptorSet;

			vk::UniquePipelineLayout m_pipelineLayout;
			vk::UniquePipeline m_pipeline;
	};
}