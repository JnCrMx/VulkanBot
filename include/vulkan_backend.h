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

#include <glslang/Include/ResourceLimits.h>
#include <glslang/MachineIndependent/Versions.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

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

	static TBuiltInResource InitResources()
	{
    	TBuiltInResource Resources;

    	Resources.maxLights                                 = 32;
    	Resources.maxClipPlanes                             = 6;
    	Resources.maxTextureUnits                           = 32;
    	Resources.maxTextureCoords                          = 32;
    	Resources.maxVertexAttribs                          = 64;
    	Resources.maxVertexUniformComponents                = 4096;
    	Resources.maxVaryingFloats                          = 64;
    	Resources.maxVertexTextureImageUnits                = 32;
    	Resources.maxCombinedTextureImageUnits              = 80;
    	Resources.maxTextureImageUnits                      = 32;
    	Resources.maxFragmentUniformComponents              = 4096;
    	Resources.maxDrawBuffers                            = 32;
    	Resources.maxVertexUniformVectors                   = 128;
    	Resources.maxVaryingVectors                         = 8;
    	Resources.maxFragmentUniformVectors                 = 16;
    	Resources.maxVertexOutputVectors                    = 16;
    	Resources.maxFragmentInputVectors                   = 15;
    	Resources.minProgramTexelOffset                     = -8;
    	Resources.maxProgramTexelOffset                     = 7;
    	Resources.maxClipDistances                          = 8;
    	Resources.maxComputeWorkGroupCountX                 = 65535;
    	Resources.maxComputeWorkGroupCountY                 = 65535;
    	Resources.maxComputeWorkGroupCountZ                 = 65535;
    	Resources.maxComputeWorkGroupSizeX                  = 1024;
    	Resources.maxComputeWorkGroupSizeY                  = 1024;
    	Resources.maxComputeWorkGroupSizeZ                  = 64;
    	Resources.maxComputeUniformComponents               = 1024;
    	Resources.maxComputeTextureImageUnits               = 16;
    	Resources.maxComputeImageUniforms                   = 8;
    	Resources.maxComputeAtomicCounters                  = 8;
    	Resources.maxComputeAtomicCounterBuffers            = 1;
    	Resources.maxVaryingComponents                      = 60;
	    Resources.maxVertexOutputComponents                 = 64;
    	Resources.maxGeometryInputComponents                = 64;
    	Resources.maxGeometryOutputComponents               = 128;
    	Resources.maxFragmentInputComponents                = 128;
    	Resources.maxImageUnits                             = 8;
    	Resources.maxCombinedImageUnitsAndFragmentOutputs   = 8;
    	Resources.maxCombinedShaderOutputResources          = 8;
    	Resources.maxImageSamples                           = 0;
    	Resources.maxVertexImageUniforms                    = 0;
    	Resources.maxTessControlImageUniforms               = 0;
    	Resources.maxTessEvaluationImageUniforms            = 0;
    	Resources.maxGeometryImageUniforms                  = 0;
    	Resources.maxFragmentImageUniforms                  = 8;
    	Resources.maxCombinedImageUniforms                  = 8;
    	Resources.maxGeometryTextureImageUnits              = 16;
    	Resources.maxGeometryOutputVertices                 = 256;
    	Resources.maxGeometryTotalOutputComponents          = 1024;
    	Resources.maxGeometryUniformComponents              = 1024;
    	Resources.maxGeometryVaryingComponents              = 64;
    	Resources.maxTessControlInputComponents             = 128;
    	Resources.maxTessControlOutputComponents            = 128;
    	Resources.maxTessControlTextureImageUnits           = 16;
    	Resources.maxTessControlUniformComponents           = 1024;
    	Resources.maxTessControlTotalOutputComponents       = 4096;
    	Resources.maxTessEvaluationInputComponents          = 128;
    	Resources.maxTessEvaluationOutputComponents         = 128;
    	Resources.maxTessEvaluationTextureImageUnits        = 16;
    	Resources.maxTessEvaluationUniformComponents        = 1024;
    	Resources.maxTessPatchComponents                    = 120;
    	Resources.maxPatchVertices                          = 32;
    	Resources.maxTessGenLevel                           = 64;
    	Resources.maxViewports                              = 16;
    	Resources.maxVertexAtomicCounters                   = 0;
    	Resources.maxTessControlAtomicCounters              = 0;
    	Resources.maxTessEvaluationAtomicCounters           = 0;
    	Resources.maxGeometryAtomicCounters                 = 0;
    	Resources.maxFragmentAtomicCounters                 = 8;
    	Resources.maxCombinedAtomicCounters                 = 8;
    	Resources.maxAtomicCounterBindings                  = 1;
    	Resources.maxVertexAtomicCounterBuffers             = 0;
    	Resources.maxTessControlAtomicCounterBuffers        = 0;
    	Resources.maxTessEvaluationAtomicCounterBuffers     = 0;
    	Resources.maxGeometryAtomicCounterBuffers           = 0;
    	Resources.maxFragmentAtomicCounterBuffers           = 1;
    	Resources.maxCombinedAtomicCounterBuffers           = 1;
    	Resources.maxAtomicCounterBufferSize                = 16384;
    	Resources.maxTransformFeedbackBuffers               = 4;
    	Resources.maxTransformFeedbackInterleavedComponents = 64;
    	Resources.maxCullDistances                          = 8;
    	Resources.maxCombinedClipAndCullDistances           = 8;
    	Resources.maxSamples                                = 4;
    	Resources.maxMeshOutputVerticesNV                   = 256;
    	Resources.maxMeshOutputPrimitivesNV                 = 512;
    	Resources.maxMeshWorkGroupSizeX_NV                  = 32;
    	Resources.maxMeshWorkGroupSizeY_NV                  = 1;
    	Resources.maxMeshWorkGroupSizeZ_NV                  = 1;
    	Resources.maxTaskWorkGroupSizeX_NV                  = 32;
    	Resources.maxTaskWorkGroupSizeY_NV                  = 1;
    	Resources.maxTaskWorkGroupSizeZ_NV                  = 1;
    	Resources.maxMeshViewCountNV                        = 4;

    	Resources.limits.nonInductiveForLoops                 = 1;
    	Resources.limits.whileLoops                           = 1;
    	Resources.limits.doWhileLoops                         = 1;
    	Resources.limits.generalUniformIndexing               = 1;
    	Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    	Resources.limits.generalVaryingIndexing               = 1;
    	Resources.limits.generalSamplerIndexing               = 1;
    	Resources.limits.generalVariableIndexing              = 1;
    	Resources.limits.generalConstantMatrixVectorIndexing  = 1;

    	return Resources;
	}

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