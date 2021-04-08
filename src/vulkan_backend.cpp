#include "vulkan_backend.h"
#include "ShaderLang.h"

#include <array>
#include <bits/stdint-uintn.h>
#include <functional>
#include <glm/fwd.hpp>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <string>
#include <tuple>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace vulkanbot
{
	uint32_t findMemoryType(vk::PhysicalDeviceMemoryProperties const & memoryProperties, int32_t typeBits, vk::MemoryPropertyFlags requirementsMask)
	{
		uint32_t typeIndex = uint32_t( ~0 );
		for ( uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++ )
		{
			if((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask))
			{
				typeIndex = i;
				break;
			}
			typeBits >>= 1;
		}
		assert( typeIndex != uint32_t( ~0 ) );
		return typeIndex;
	}

	ImageData::ImageData(	vk::PhysicalDevice const & physicalDevice,
							vk::UniqueDevice const & device,
				  			vk::Format format_, 
				  			vk::Extent2D const & extent, 
				  			vk::ImageUsageFlags usage,
				  			vk::MemoryPropertyFlags memoryProperties) : format(format_)
	{
		image = device->createImageUnique(vk::ImageCreateInfo({}, 
			vk::ImageType::e2D, format_,
			vk::Extent3D(extent, 1), 1, 1,
			vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
			usage,
			vk::SharingMode::eExclusive, 0, nullptr, 
			vk::ImageLayout::eUndefined));

		vk::MemoryRequirements memoryRequirements = device->getImageMemoryRequirements(image.get());

		uint32_t memoryTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(),
			memoryRequirements.memoryTypeBits, memoryProperties);

		deviceMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));

		device->bindImageMemory(image.get(), deviceMemory.get(), 0);

		vk::ComponentMapping componentMapping(vk::ComponentSwizzle::eIdentity, 
			vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity);
		vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(),
                                                	image.get(),
                                                	vk::ImageViewType::e2D,
                                                	format,
                                                	componentMapping,
                                                	vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		imageView = device->createImageViewUnique(imageViewCreateInfo);
	}

	void generate_grid(int N, std::vector<glm::vec3> &vertices, std::vector<uint16_t> &indices)
	{
		for(int j=0; j<=N; ++j)
		{
			for(int i=0; i<=N; ++i)
			{
				float x = 2.0*((float)i/(float)N)-1.0;
				float y = 2.0*((float)j/(float)N)-1.0;
				float z = 0.0f;
				vertices.push_back(glm::vec3(x, y, z));
			}
		}

		for(int j=0; j<N; ++j)
		{
			for(int i=0; i<N; ++i)
			{
				int row1 = j * (N+1);
				int row2 = (j+1) * (N+1);

				// triangle 1
				indices.push_back(row1+i);
				indices.push_back(row1+i+1);
				indices.push_back(row2+i+1);

				// triangle 2
				indices.push_back(row1+i);
				indices.push_back(row2+i+1);
				indices.push_back(row2+i);
			}
		}
	}

	void VulkanBackend::initVulkan(int width, int height)
	{
		m_width = static_cast<uint32_t>(width);
		m_height = static_cast<uint32_t>(height);

		vk::ApplicationInfo applicationInfo("VulkanBot", 1, "VulkanBot", 1, VK_API_VERSION_1_1);
		vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);
		m_instance = vk::createInstanceUnique(instanceCreateInfo);

		m_physicalDevice = m_instance->enumeratePhysicalDevices().front();
		std::cout << "Using Vulkan device " << m_physicalDevice.getProperties().deviceName << std::endl;

		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();
		size_t graphicsQueueFamilyIndex = std::distance(
      		queueFamilyProperties.begin(),
      		std::find_if(
        		queueFamilyProperties.begin(), queueFamilyProperties.end(), []( vk::QueueFamilyProperties const & qfp ) {
          			return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
        	}));
    	assert(graphicsQueueFamilyIndex < queueFamilyProperties.size());

		float queuePriority = 0.0f;
    	vk::DeviceQueueCreateInfo deviceQueueCreateInfo(
      		vk::DeviceQueueCreateFlags(), static_cast<uint32_t>(graphicsQueueFamilyIndex), 1, &queuePriority);
    	m_device = m_physicalDevice.createDeviceUnique(vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo));

		m_commandPool = m_device->createCommandPoolUnique(
    		vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsQueueFamilyIndex));

		m_queue = m_device->getQueue(graphicsQueueFamilyIndex, 0);


		m_texture = new ImageData(m_physicalDevice, m_device, vk::Format::eR8G8B8A8Unorm, 
			{m_textureWidth, m_textureHeight},
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, 
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		m_textureSampler = m_device->createSamplerUnique(vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear,
			vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat,
			0.0f, false, 16.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f, vk::BorderColor::eFloatOpaqueBlack));

		m_renderImage = new ImageData(m_physicalDevice, m_device, vk::Format::eR8G8B8A8Unorm, 
			{m_width, m_height},
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eDeviceLocal);

		{
			m_inputImageBuffer = m_device->createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), m_textureWidth*m_textureHeight*4,
				vk::BufferUsageFlagBits::eTransferSrc));
			m_inputImageMemoryRequirements = m_device->getBufferMemoryRequirements(m_inputImageBuffer.get());
			std::cout << "Input Buffer memory: " << m_inputImageMemoryRequirements.size << std::endl;

			uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(),
				m_inputImageMemoryRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			m_inputImageMemory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(m_inputImageMemoryRequirements.size, memoryTypeIndex));

			m_device->bindBufferMemory(m_inputImageBuffer.get(), m_inputImageMemory.get(), 0);
		}
		{
			m_outputImageBuffer = m_device->createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), m_width*m_height*4,
				vk::BufferUsageFlagBits::eTransferDst));
			m_outputImageMemoryRequirements = m_device->getBufferMemoryRequirements(m_outputImageBuffer.get());
			std::cout << "Output Buffer memory: " << m_outputImageMemoryRequirements.size << std::endl;

			uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(),
				m_outputImageMemoryRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			m_outputImageMemory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(m_outputImageMemoryRequirements.size, memoryTypeIndex));

			m_device->bindBufferMemory(m_outputImageBuffer.get(), m_outputImageMemory.get(), 0);
		}

		std::vector<glm::vec3> vertices;
		std::vector<uint16_t> indices;
		generate_grid(10, vertices, indices);
		m_indexCount = indices.size();
		{
			m_indexBuffer = m_device->createBufferUnique(vk::BufferCreateInfo({}, indices.size()*sizeof(uint16_t),
				vk::BufferUsageFlagBits::eIndexBuffer));
			vk::MemoryRequirements memoryRequirements = m_device->getBufferMemoryRequirements(m_indexBuffer.get());
			uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(),
				memoryRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			m_indexMemory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
			m_device->bindBufferMemory(m_indexBuffer.get(), m_indexMemory.get(), 0);

			uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(m_indexMemory.get(), 0, memoryRequirements.size));
			memcpy(pData, indices.data(), indices.size() * sizeof(uint16_t));
			m_device->unmapMemory(m_indexMemory.get());
		}
		{
			m_vertexBuffer = m_device->createBufferUnique(vk::BufferCreateInfo({}, vertices.size()*sizeof(glm::vec3),
				vk::BufferUsageFlagBits::eVertexBuffer));
			vk::MemoryRequirements memoryRequirements = m_device->getBufferMemoryRequirements(m_vertexBuffer.get());
			uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(),
				memoryRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			m_vertexMemory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
			m_device->bindBufferMemory(m_vertexBuffer.get(), m_vertexMemory.get(), 0);

			uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(m_vertexMemory.get(), 0, memoryRequirements.size));
			memcpy(pData, vertices.data(), vertices.size() * sizeof(glm::vec3));
			m_device->unmapMemory(m_vertexMemory.get());
		}
		{
			m_uniformBuffer = m_device->createBufferUnique(vk::BufferCreateInfo({}, sizeof(UniformBufferObject),
				vk::BufferUsageFlagBits::eUniformBuffer));
			vk::MemoryRequirements memoryRequirements = m_device->getBufferMemoryRequirements(m_uniformBuffer.get());
			uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(),
				memoryRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			m_uniformMemory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
			m_device->bindBufferMemory(m_uniformBuffer.get(), m_uniformMemory.get(), 0);
		}

		std::array<vk::AttachmentDescription, 1> attachmentDescriptions;
		attachmentDescriptions[0] = vk::AttachmentDescription(
			{}, m_renderImage->format, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, 
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
		vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::SubpassDescription subpass(
			{}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef, {}, {});
		vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0, 
			vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {});

		m_renderPass = m_device->createRenderPassUnique(
			vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpass));

		m_framebuffer = m_device->createFramebufferUnique(
			vk::FramebufferCreateInfo({}, m_renderPass.get(), m_renderImage->imageView.get(), m_width, m_height, 1));

		std::array<vk::DescriptorSetLayoutBinding, 2> bindings;
		bindings[0] = vk::DescriptorSetLayoutBinding(
			0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
		bindings[1] = vk::DescriptorSetLayoutBinding(
			1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
		m_descriptorSetLayout = m_device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo({}, bindings));

		vk::DescriptorPoolSize poolSize(vk::DescriptorType::eCombinedImageSampler, 1);
		vk::DescriptorPoolSize uniformPoolSize(vk::DescriptorType::eUniformBuffer, 1);
		std::array<vk::DescriptorPoolSize, 2> poolSizes{poolSize, uniformPoolSize};
		
		m_descriptorPool = m_device->createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 2, poolSizes));
			
		m_descriptorSet = std::move(
      		m_device->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(m_descriptorPool.get(), m_descriptorSetLayout.get())).front());

		vk::DescriptorImageInfo descriptorImageInfo(m_textureSampler.get(), m_texture->imageView.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorBufferInfo descriptorBufferInfo(m_uniformBuffer.get(), 0, sizeof(UniformBufferObject));
		std::array<vk::WriteDescriptorSet, 2> writeDescriptorSets;
		writeDescriptorSets[0] = vk::WriteDescriptorSet(m_descriptorSet.get(), 0, 0, vk::DescriptorType::eCombinedImageSampler, descriptorImageInfo, nullptr, nullptr);
		writeDescriptorSets[1] = vk::WriteDescriptorSet(m_descriptorSet.get(), 1, 0, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfo, nullptr);
		m_device->updateDescriptorSets(writeDescriptorSets, nullptr);

		m_pipelineLayout = m_device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo({}, m_descriptorSetLayout.get()));

		m_commandBuffer = std::move(m_device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
                                    m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1)).front());

		m_fence = m_device->createFenceUnique(vk::FenceCreateInfo());
	}

	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if(!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t) file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	vk::UniqueShaderModule VulkanBackend::createShader(const std::vector<unsigned int>& code)
	{
		return m_device->createShaderModuleUnique(vk::ShaderModuleCreateInfo({}, code));
	}

	vk::UniqueShaderModule VulkanBackend::createShader(const std::vector<char>& code)
	{
		return m_device->createShaderModuleUnique(vk::ShaderModuleCreateInfo({}, code.size(), reinterpret_cast<const uint32_t*>(code.data())));
	}

	vk::UniquePipeline VulkanBackend::createPipeline(vk::UniqueShaderModule& vertexShader, vk::UniqueShaderModule& fragmentShader)
	{
		//vk::UniqueShaderModule vertexShader = createShader(readFile("shaders/base.vert.spv"));
		//vk::UniqueShaderModule fragmentShader = createShader(code);

		vk::PipelineShaderStageCreateInfo vertexShaderInfo({}, vk::ShaderStageFlagBits::eVertex, vertexShader.get(), "main");
		vk::PipelineShaderStageCreateInfo fragmentShaderInfo({}, vk::ShaderStageFlagBits::eFragment, fragmentShader.get(), "main");
		std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = { vertexShaderInfo, fragmentShaderInfo };

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo({},
			1, &bindingDescription,
			static_cast<uint32_t>(attributeDescriptions.size()), attributeDescriptions.data());

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);
		vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f);
		vk::Rect2D scissor({0, 0}, {m_width, m_height});

		vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

		vk::PipelineRasterizationStateCreateInfo rasterizer({}, false, false, vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eFront, vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f);
		vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1);
		vk::PipelineColorBlendAttachmentState colorBlendAttachment(false, 
			vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
			vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, 
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		vk::PipelineColorBlendStateCreateInfo colorBlending({}, false, vk::LogicOp::eNoOp, colorBlendAttachment, {{1.0f, 1.0f, 1.0f, 1.0f}});

		vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &vertexInputInfo, 
			&inputAssembly, nullptr, &viewportState, &rasterizer, &multisampling, nullptr, &colorBlending, nullptr, 
			m_pipelineLayout.get(), m_renderPass.get());

    	vk::Result result;
    	vk::UniquePipeline pipeline;
		std::tie( result, pipeline ) = m_device->createGraphicsPipelineUnique(nullptr, pipelineInfo).asTuple();
		switch ( result )
    	{
      		case vk::Result::eSuccess: break;
      		case vk::Result::ePipelineCompileRequiredEXT:
			  	std::cerr << "Extension required for pipeline!" << std::endl;
        		break;
      		default: assert( false );  // should never happen
    	}

		return pipeline;
	}

	std::tuple<bool, std::string> compileShader(EShLanguage stage, std::string glslCode, std::vector<unsigned int>& shaderCode)
	{
		const char * shaderStrings[1];
	    shaderStrings[0] = glslCode.data();

		glslang::TShader shader(stage);
		shader.setStrings(shaderStrings, 1);
		shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_1);
		shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_3);
		shader.setEntryPoint("main");
		shader.setSourceEntryPoint("main");

		EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
		if(!shader.parse(&glslang::DefaultTBuiltInResource, 100, false, messages))
		{
			return {false, std::string(shader.getInfoLog())};
		}

		glslang::TProgram program;
		program.addShader(&shader);
		if(!program.link(messages))
		{
			return {false, std::string(program.getInfoLog())};
		}
		glslang::GlslangToSpv(*program.getIntermediate(stage), shaderCode);

		return {true, ""};
	}

	std::tuple<bool, std::string> VulkanBackend::uploadShader(std::string glslCode, bool vertex)
	{
		std::vector<unsigned int> shaderCode;
		glslang::InitializeProcess();
		auto [result, message] = compileShader(vertex ? EShLangVertex : EShLangFragment, glslCode, shaderCode);
		glslang::FinalizeProcess();

		if(!result)
		{
			return {result, message};
		}

		vk::UniqueShaderModule baseShader = createShader(readFile(vertex ? "shaders/base.frag.spv" : "shaders/base.vert.spv"));
		vk::UniqueShaderModule customShader = createShader(shaderCode);

		uploadShader(vertex ? customShader : baseShader, vertex ? baseShader : customShader);
		return {true, ""};
	}

	std::tuple<bool, std::string> VulkanBackend::uploadShaders(const std::string vertexCode, const std::string fragmentCode)
	{
		std::vector<unsigned int> vertexBin;
		std::vector<unsigned int> fragmentBin;

		glslang::InitializeProcess();
		auto [vResult, vMessage] = compileShader(EShLangVertex, vertexCode, vertexBin);
		auto [fResult, fMessage] = compileShader(EShLangFragment, fragmentCode, fragmentBin);
		glslang::FinalizeProcess();

		if(!vResult)
			return {vResult, "vertex: "+vMessage};
		if(!fResult)
			return {fResult, "fragment: "+fMessage};

		vk::UniqueShaderModule vertexShader = createShader(vertexBin);
		vk::UniqueShaderModule fragmentShader = createShader(fragmentBin);

		uploadShader(vertexShader, fragmentShader);
		return {true, ""};
	}

	void VulkanBackend::uploadShader(vk::UniqueShaderModule& vertexShader, vk::UniqueShaderModule& fragmentShader)
	{
		if(m_pipeline)
			m_pipeline.release();
		m_pipeline = createPipeline(vertexShader, fragmentShader);

		m_commandBuffer->reset();
		m_commandBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

		m_commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 
			{}, {}, {}, 
			vk::ImageMemoryBarrier(
				{}, vk::AccessFlagBits::eTransferWrite, 
				vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				m_texture->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
		std::array<vk::BufferImageCopy, 1> regions;
		regions[0] = vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {0, 0, 0}, {m_textureWidth, m_textureHeight, 1});
		m_commandBuffer->copyBufferToImage(m_inputImageBuffer.get(), m_texture->image.get(), vk::ImageLayout::eTransferDstOptimal, regions);
		m_commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 
			{}, {}, {},
			vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, 
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 
				m_texture->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));

    	std::array<vk::ClearValue, 1> clearValues;
		clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({{0.0f, 0.0f, 0.0f, 1.0f}}));
		m_commandBuffer->beginRenderPass(
			vk::RenderPassBeginInfo(
				m_renderPass.get(), 
				m_framebuffer.get(), 
				{{0, 0}, {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height)}},
				1, clearValues.data()), 
			vk::SubpassContents::eInline);
		m_commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		m_commandBuffer->bindVertexBuffers(0, m_vertexBuffer.get(), static_cast<vk::DeviceSize>(0));
		m_commandBuffer->bindIndexBuffer(m_indexBuffer.get(), 0, vk::IndexType::eUint16);
		m_commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout.get(), 0, m_descriptorSet.get(), nullptr);

		m_commandBuffer->drawIndexed(m_indexCount, 1, 0, 0, 0);
		m_commandBuffer->endRenderPass();

		regions[0] = vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {0, 0, 0}, {m_width, m_height, 1});
		m_commandBuffer->copyImageToBuffer(m_renderImage->image.get(), vk::ImageLayout::eTransferSrcOptimal, m_outputImageBuffer.get(), regions);

		m_commandBuffer->end();
	}

	void VulkanBackend::readImage(const std::vector<unsigned char>& data)
	{
		uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(m_inputImageMemory.get(), 0, m_inputImageMemoryRequirements.size));
		memcpy(pData, data.data(), data.size());
		m_device->unmapMemory(m_inputImageMemory.get());
	}

	void VulkanBackend::updateUniformObject(std::function<void(UniformBufferObject*)> updater)
	{
		uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(m_uniformMemory.get(), 0, sizeof(UniformBufferObject)));
		updater((UniformBufferObject*)pData);
		m_device->unmapMemory(m_uniformMemory.get());
	}

	void VulkanBackend::renderFrame(std::function<void(uint8_t*, vk::DeviceSize, int, int, vk::Result, long)> consumer)
	{
		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		m_queue.submit(vk::SubmitInfo(0, nullptr, &waitDestinationStageMask, 1, &m_commandBuffer.get()), m_fence.get());

    	auto t1 = std::chrono::high_resolution_clock::now();
		vk::Result r = m_device->waitForFences(m_fence.get(), true, UINT64_MAX);
    	auto t2 = std::chrono::high_resolution_clock::now();
    	long duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

		m_device->resetFences(m_fence.get());

		uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(m_outputImageMemory.get(), 0, m_outputImageMemoryRequirements.size));
		consumer(pData, m_outputImageMemoryRequirements.size, m_width, m_height, r, duration);
		m_device->unmapMemory(m_outputImageMemory.get());
	}

	VulkanBackend::~VulkanBackend()
	{
		if(m_renderImage)
			delete m_renderImage;
		if(m_texture)
			delete m_texture;
	}
}