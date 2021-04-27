#pragma once

#include <bits/stdint-uintn.h>
#include <cctype>
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

	union OutputStorageObject {
		float as_float;
		int as_int;
		glm::vec4 as_vec4;
		glm::ivec4 as_ivec4;

		uint32_t as_uints[16];

		std::string charsToString()
		{
			size_t len = sizeof(as_uints)/sizeof(uint32_t);

			char chars[len];
			for(size_t i=0; i<len; i++)
			{
				chars[i] = as_uints[i];
			}

			std::string s(chars, len);
			s.replace(std::remove_if(s.begin(), s.end(), []( auto const& c ) -> bool { return !std::isalnum(c) && c != 0; }), s.end(), "\0");
			return s;
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
					vk::MemoryPropertyFlags memoryProperties,
					vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor);
	};

	struct Mesh {
		vk::UniqueBuffer vertexBuffer;
		vk::UniqueBuffer texCoordBuffer;
		vk::UniqueBuffer normalBuffer;

		vk::UniqueDeviceMemory memory;

		vk::UniqueBuffer indexBuffer;
		vk::UniqueDeviceMemory indexMemory;

		int vertexCount;
		int indexCount;

		static std::array<vk::VertexInputBindingDescription, 3> getBindingDescriptions()
		{
			return {
				vk::VertexInputBindingDescription(0, sizeof(glm::vec3), vk::VertexInputRate::eVertex),
				vk::VertexInputBindingDescription(1, sizeof(glm::vec2), vk::VertexInputRate::eVertex),
				vk::VertexInputBindingDescription(2, sizeof(glm::vec3), vk::VertexInputRate::eVertex)
			};
		}

		static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
		{
			return {
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32A32Sfloat, 0),
				vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32Sfloat, 0),
				vk::VertexInputAttributeDescription(2, 2, vk::Format::eR32G32B32A32Sfloat, 0)
			};
		}

		std::array<vk::Buffer, 3> getBuffers()
		{
			return {
				vertexBuffer.get(), texCoordBuffer.get(), normalBuffer.get()
			};
		}

		std::array<vk::DeviceSize, 3> getBufferOffsets()
		{
			return {0, 0, 0};
		}

		Mesh(	vk::PhysicalDevice const & physicalDevice,
				vk::UniqueDevice const & device,
				int const vertexCount, int const indexCount);
	};

	class VulkanBackend
	{
		public:
			~VulkanBackend();

			void initVulkan(int width, int height, bool validation = false, int debugSeverity = 0, int debugType = 0);

			std::tuple<bool, std::string> uploadShaderMix(const std::string vertex, bool vertexFile, const std::string fragment, bool fragmentFile,
				vk::CullModeFlags cullMode = vk::CullModeFlagBits::eFront, bool depth = true);
			std::tuple<bool, std::string> uploadComputeShader(const std::string compute, bool file);

			void buildCommandBuffer(Mesh* mesh = nullptr, bool yuv420p = false);
			void buildComputeCommandBuffer(int x, int y, int z);

			std::unique_ptr<ImageData> uploadImage(int width, int height, const std::vector<unsigned char>& data);
			std::unique_ptr<Mesh> uploadMesh(	std::vector<glm::vec3> vertices,
												std::vector<glm::vec2> texCoords,
												std::vector<glm::vec3> normals,
												std::vector<uint16_t> indices);

			void updateUniformObject(std::function<void(UniformBufferObject*)> updater);

			void renderFrame(std::function<void(uint8_t*, vk::DeviceSize, int, int, vk::Result, long)> consumer, bool yuv420p = false);
			void doComputation(std::function<void(OutputStorageObject*, vk::Result, long)> consumer);
		private:
			uint32_t m_width = 1024;
			uint32_t m_height = 1024;

			vk::UniqueShaderModule createShader(const std::vector<unsigned int>& code);
			vk::UniqueShaderModule createShader(const std::vector<char>& code);
			vk::UniquePipeline createPipeline(vk::UniqueShaderModule& vertexShader, vk::UniqueShaderModule& fragment,
				vk::CullModeFlags cullMode = vk::CullModeFlagBits::eFront, bool depth = true);
			vk::UniquePipeline createComputePipeline(vk::UniqueShaderModule& computeShader);

			vk::UniqueInstance m_instance;
			vk::DispatchLoaderDynamic m_dispatch;
			vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> m_debugMessenger;

			vk::PhysicalDevice m_physicalDevice;
			vk::UniqueDevice m_device;
			vk::Queue m_queue;
			vk::Queue m_transferQueue;
			vk::UniqueFence m_fence;
			vk::UniqueFence m_transferFence;

			vk::UniqueCommandPool m_commandPool;
			vk::UniqueCommandBuffer m_commandBuffer;
			vk::UniqueCommandBuffer m_computeCommandBuffer;

			vk::UniqueBuffer m_outputImageBuffer;
			vk::MemoryRequirements m_outputImageMemoryRequirements;
			vk::UniqueDeviceMemory m_outputImageMemory;

			std::unique_ptr<Mesh> m_gridMesh;

			vk::UniqueBuffer m_uniformBuffer;
			vk::UniqueDeviceMemory m_uniformMemory;

			vk::UniqueBuffer m_outputStorageBuffer;
			vk::UniqueDeviceMemory m_outputStorageMemory;

			vk::UniqueSampler m_sampler;

			ImageData* m_renderImage;
			ImageData* m_depthImage;
			vk::UniqueRenderPass m_renderPass;
			vk::UniqueFramebuffer m_framebuffer;

			vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
			vk::UniqueDescriptorSetLayout m_computeDescriptorSetLayout;
			vk::UniqueDescriptorPool m_descriptorPool;
			vk::UniqueDescriptorSet m_descriptorSet;
			vk::UniqueDescriptorSet m_computeDescriptorSet;

			vk::UniquePipelineLayout m_pipelineLayout;
			vk::UniquePipeline m_pipeline;

			vk::UniquePipelineLayout m_computePipelineLayout;
			vk::UniquePipeline m_computePipeline;

			std::unique_ptr<ImageData> m_encodedImageY;
			std::unique_ptr<ImageData> m_encodedImageCr;
			std::unique_ptr<ImageData> m_encodedImageCb;
			vk::UniqueDescriptorSetLayout m_descriptorSetLayoutEncode;
			vk::UniqueDescriptorSet m_descriptorSetEncode;
			vk::UniquePipelineLayout m_pipelineLayoutEncode;
			vk::UniquePipeline m_encodePipeline;
			vk::UniquePipeline createEncodePipeline();
	};
}