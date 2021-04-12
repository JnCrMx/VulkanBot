#include "vulkan_backend.h"
#include "ShaderLang.h"

#include <array>
#include <bits/stdint-uintn.h>
#include <functional>
#include <glm/fwd.hpp>
#include <iostream>
#include <fstream>
#include <memory>
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
							vk::MemoryPropertyFlags memoryProperties,
							vk::ImageAspectFlags aspectFlags) : format(format_)
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
													vk::ImageSubresourceRange(aspectFlags, 0, 1, 0, 1));
		imageView = device->createImageViewUnique(imageViewCreateInfo);
	}

	Mesh::Mesh(	vk::PhysicalDevice const & physicalDevice,
				vk::UniqueDevice const & device,
				int const vertexCount, int const indexCount)
	{
		this->vertexCount = vertexCount;
		this->indexCount = indexCount;

		vertexBuffer = device->createBufferUnique(vk::BufferCreateInfo({}, vertexCount * sizeof(glm::vec3),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));
		vk::MemoryRequirements vertexMemoryRequirements = device->getBufferMemoryRequirements(vertexBuffer.get());
		uint32_t vertexMemoryTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), vertexMemoryRequirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		vertexMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(vertexMemoryRequirements.size, vertexMemoryTypeIndex));
		device->bindBufferMemory(vertexBuffer.get(), vertexMemory.get(), 0);

		texCoordBuffer = device->createBufferUnique(vk::BufferCreateInfo({}, vertexCount * sizeof(glm::vec2),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));
		vk::MemoryRequirements texCoordMemoryRequirements = device->getBufferMemoryRequirements(texCoordBuffer.get());
		uint32_t texCoordMemoryTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), texCoordMemoryRequirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		texCoordMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(texCoordMemoryRequirements.size, texCoordMemoryTypeIndex));
		device->bindBufferMemory(texCoordBuffer.get(), texCoordMemory.get(), 0);

		normalBuffer = device->createBufferUnique(vk::BufferCreateInfo({}, vertexCount * sizeof(glm::vec3),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));
		vk::MemoryRequirements normalMemoryRequirements = device->getBufferMemoryRequirements(normalBuffer.get());
		uint32_t normalMemoryTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), normalMemoryRequirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		normalMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(normalMemoryRequirements.size, normalMemoryTypeIndex));
		device->bindBufferMemory(normalBuffer.get(), normalMemory.get(), 0);

		indexBuffer = device->createBufferUnique(vk::BufferCreateInfo({}, indexCount * sizeof(glm::vec3),
			vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst));
		vk::MemoryRequirements indexMemoryRequirements = device->getBufferMemoryRequirements(indexBuffer.get());
		uint32_t indexMemoryTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), indexMemoryRequirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		indexMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(indexMemoryRequirements.size, indexMemoryTypeIndex));
		device->bindBufferMemory(indexBuffer.get(), indexMemory.get(), 0);
	}

	void generate_grid(int N, std::vector<glm::vec3> &vertices, std::vector<glm::vec2> &texCoords, std::vector<uint16_t> &indices)
	{
		for(int j=0; j<=N; ++j)
		{
			for(int i=0; i<=N; ++i)
			{
				float x = 2.0*((float)i/(float)N)-1.0;
				float y = 2.0*((float)j/(float)N)-1.0;
				float z = 0.0f;
				vertices.push_back(glm::vec3(x, y, z));
				texCoords.push_back(glm::vec2(x, y));
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
		size_t transferQueueFamilyIndex = std::distance(
			queueFamilyProperties.begin(),
			std::find_if(
				queueFamilyProperties.begin(), queueFamilyProperties.end(), []( vk::QueueFamilyProperties const & qfp ) {
					return qfp.queueFlags & vk::QueueFlagBits::eTransfer;
		}));
		assert(transferQueueFamilyIndex < queueFamilyProperties.size());

		float queuePriority = 0.0f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo(
			vk::DeviceQueueCreateFlags(), static_cast<uint32_t>(graphicsQueueFamilyIndex), 1, &queuePriority);
		m_device = m_physicalDevice.createDeviceUnique(vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo));

		m_commandPool = m_device->createCommandPoolUnique(
			vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsQueueFamilyIndex));

		m_queue = m_device->getQueue(graphicsQueueFamilyIndex, 0);
		m_transferQueue = m_device->getQueue(transferQueueFamilyIndex, 0);

		m_fence = m_device->createFenceUnique(vk::FenceCreateInfo());
		m_transferFence = m_device->createFenceUnique(vk::FenceCreateInfo());

		m_sampler = m_device->createSamplerUnique(vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear,
			vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat,
			0.0f, false, 16.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f, vk::BorderColor::eFloatOpaqueBlack));

		m_renderImage = new ImageData(m_physicalDevice, m_device, vk::Format::eR8G8B8A8Unorm,
			{m_width, m_height},
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		m_depthImage = new ImageData(m_physicalDevice, m_device, vk::Format::eD32Sfloat,
			{m_width, m_height},
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth);

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
		std::vector<glm::vec2> texCoords;
		std::vector<glm::vec3> normals;
		std::vector<uint16_t> indices;
		generate_grid(10, vertices, texCoords, indices);
		normals.resize(vertices.size());
		m_gridMesh = uploadMesh(vertices, texCoords, normals, indices);

		/*
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
		*/
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

		std::array<vk::AttachmentDescription, 2> attachmentDescriptions;
		attachmentDescriptions[0] = vk::AttachmentDescription(
			{}, m_renderImage->format, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
		attachmentDescriptions[1] = vk::AttachmentDescription(
			{}, m_depthImage->format, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::AttachmentReference depthAttachmentRef(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		vk::SubpassDescription subpass(
			{}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef, {}, &depthAttachmentRef);
		vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0,
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests, {},
			vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite, {});

		m_renderPass = m_device->createRenderPassUnique(
			vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpass, dependency));

		std::array<vk::ImageView, 2> attachments = {
			m_renderImage->imageView.get(),
			m_depthImage->imageView.get()
		};
		m_framebuffer = m_device->createFramebufferUnique(
			vk::FramebufferCreateInfo({}, m_renderPass.get(), attachments, m_width, m_height, 1));

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

		vk::DescriptorBufferInfo descriptorBufferInfo(m_uniformBuffer.get(), 0, sizeof(UniformBufferObject));
		std::array<vk::WriteDescriptorSet, 1> writeDescriptorSets = {
			vk::WriteDescriptorSet(m_descriptorSet.get(), 1, 0, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfo, nullptr)
		};
		m_device->updateDescriptorSets(writeDescriptorSets, nullptr);

		m_pipelineLayout = m_device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo({}, m_descriptorSetLayout.get()));

		m_commandBuffer = std::move(m_device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
									m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1)).front());
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

	vk::UniquePipeline VulkanBackend::createPipeline(vk::UniqueShaderModule& vertexShader, vk::UniqueShaderModule& fragmentShader,
		vk::CullModeFlags cullMode, bool depth)
	{
		//vk::UniqueShaderModule vertexShader = createShader(readFile("shaders/base.vert.spv"));
		//vk::UniqueShaderModule fragmentShader = createShader(code);

		vk::PipelineShaderStageCreateInfo vertexShaderInfo({}, vk::ShaderStageFlagBits::eVertex, vertexShader.get(), "main");
		vk::PipelineShaderStageCreateInfo fragmentShaderInfo({}, vk::ShaderStageFlagBits::eFragment, fragmentShader.get(), "main");
		std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = { vertexShaderInfo, fragmentShaderInfo };

		auto bindingDescription = Mesh::getBindingDescriptions();
		auto attributeDescriptions = Mesh::getAttributeDescriptions();

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo({},
			bindingDescription, attributeDescriptions);

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);
		vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f);
		vk::Rect2D scissor({0, 0}, {m_width, m_height});

		vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

		vk::PipelineRasterizationStateCreateInfo rasterizer({}, false, false, vk::PolygonMode::eFill,
			cullMode, vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f);
		vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1);
		vk::PipelineColorBlendAttachmentState colorBlendAttachment(false,
			vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
			vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		vk::PipelineColorBlendStateCreateInfo colorBlending({}, false, vk::LogicOp::eNoOp, colorBlendAttachment, {{1.0f, 1.0f, 1.0f, 1.0f}});

		vk::PipelineDepthStencilStateCreateInfo depthStencil({}, depth, depth, vk::CompareOp::eLessOrEqual, false);

		vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &vertexInputInfo,
			&inputAssembly, nullptr, &viewportState, &rasterizer, &multisampling, &depthStencil, &colorBlending, nullptr,
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

	std::tuple<bool, std::string> VulkanBackend::uploadShaderMix(const std::string vertex, bool vertexFile, const std::string fragment, bool fragmentFile,
		vk::CullModeFlags cullMode, bool depth)
	{
		std::vector<unsigned int> vertexBin;
		std::vector<unsigned int> fragmentBin;

		std::tuple<bool, std::string> vertexResult = {true, ""};
		std::tuple<bool, std::string> fragmentResult = {true, ""};

		vk::UniqueShaderModule vertexShader;
		vk::UniqueShaderModule fragmentShader;

		glslang::InitializeProcess();
		if(vertexFile)
		{
			if(vertex.find("/") != std::string::npos)
				vertexResult = {false, "shader name contains /"};
			else
			{
				try
				{
					vertexShader = createShader(readFile("shaders/"+vertex+".vert.spv"));
				}
				catch(const std::runtime_error& err)
				{
					vertexResult = {false, err.what()};
				}
			}
		}
		else
		{
			vertexResult = compileShader(EShLangVertex, vertex, vertexBin);
			if(std::get<0>(vertexResult))
				vertexShader = createShader(vertexBin);
		}
		if(fragmentFile)
		{
			if(fragment.find("/") != std::string::npos)
				fragmentResult = {false, "shader name contains /"};
			else
			{
				try
				{
					fragmentShader = createShader(readFile("shaders/"+fragment+".frag.spv"));
				}
				catch(const std::runtime_error& err)
				{
					fragmentResult = {false, err.what()};
				}
			}
		}
		else
		{
			fragmentResult = compileShader(EShLangFragment, fragment, fragmentBin);
			if(std::get<0>(fragmentResult))
				fragmentShader = createShader(fragmentBin);
		}
		glslang::FinalizeProcess();

		if(!std::get<0>(vertexResult))
			return {std::get<0>(vertexResult), "vertex: "+std::get<1>(vertexResult)};
		if(!std::get<0>(fragmentResult))
			return {std::get<0>(fragmentResult), "fragment: "+std::get<1>(fragmentResult)};

		m_pipeline = createPipeline(vertexShader, fragmentShader, cullMode, depth);
		return {true, ""};
	}

	void VulkanBackend::buildCommandBuffer(Mesh* mesh)
	{
		if(mesh == nullptr)
		{
			mesh = m_gridMesh.get();
		}

		m_commandBuffer->reset();
		m_commandBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

		/*m_commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
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
				m_texture->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));*/

		std::array<vk::ClearValue, 2> clearValues;
		clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({{0.0f, 0.0f, 0.0f, 1.0f}}));
		clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
		m_commandBuffer->beginRenderPass(
			vk::RenderPassBeginInfo(
				m_renderPass.get(),
				m_framebuffer.get(),
				{{0, 0}, {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height)}}, clearValues),
			vk::SubpassContents::eInline);
		m_commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		m_commandBuffer->bindVertexBuffers(0, mesh->getBuffers(), mesh->getBufferOffsets());
		m_commandBuffer->bindIndexBuffer(mesh->indexBuffer.get(), 0, vk::IndexType::eUint16);
		m_commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout.get(), 0, m_descriptorSet.get(), nullptr);

		m_commandBuffer->drawIndexed(mesh->indexCount, 1, 0, 0, 0);
		m_commandBuffer->endRenderPass();

		std::array<vk::BufferImageCopy, 1> regions = {
			vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {0, 0, 0}, {m_width, m_height, 1})
		};
		m_commandBuffer->copyImageToBuffer(m_renderImage->image.get(), vk::ImageLayout::eTransferSrcOptimal, m_outputImageBuffer.get(), regions);

		m_commandBuffer->end();
	}

	std::unique_ptr<ImageData> VulkanBackend::uploadImage(int width, int height, const std::vector<unsigned char>& data)
	{
		std::unique_ptr<ImageData> image = std::make_unique<ImageData>(m_physicalDevice, m_device, vk::Format::eR8G8B8A8Unorm,
			vk::Extent2D(static_cast<uint32_t>(width), static_cast<uint32_t>(height)),
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::UniqueBuffer buffer = m_device->createBufferUnique(vk::BufferCreateInfo({}, data.size(), vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive));
		vk::MemoryRequirements memoryRequirements = m_device->getBufferMemoryRequirements(buffer.get());
		uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(),
			memoryRequirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		vk::UniqueDeviceMemory memory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
		m_device->bindBufferMemory(buffer.get(), memory.get(), 0);

		uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(memory.get(), 0, memoryRequirements.size));
		memcpy(pData, data.data(), data.size());
		m_device->unmapMemory(memory.get());

		vk::UniqueCommandBuffer commandBuffer = std::move(m_device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
			m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1)).front());
		commandBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));
		commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
			{}, {}, {},
			vk::ImageMemoryBarrier(
				{}, vk::AccessFlagBits::eTransferWrite,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				image->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
		std::array<vk::BufferImageCopy, 1> regions = {
			vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {0, 0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1})
		};
		commandBuffer->copyBufferToImage(buffer.get(), image->image.get(), vk::ImageLayout::eTransferDstOptimal, regions);
		commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
			{}, {}, {},
			vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				image->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
		commandBuffer->end();

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eTransfer);
		m_queue.submit(vk::SubmitInfo(0, nullptr, &waitDestinationStageMask, 1, &commandBuffer.get()), m_transferFence.get());
		vk::Result r = m_device->waitForFences(m_transferFence.get(), true, UINT64_MAX);
		m_device->resetFences(m_transferFence.get());

		assert(r == vk::Result::eSuccess);

		vk::DescriptorImageInfo descriptorImageInfo(m_sampler.get(), image->imageView.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
		std::array<vk::WriteDescriptorSet, 1> writeDescriptorSets = {
			vk::WriteDescriptorSet(m_descriptorSet.get(), 0, 0, vk::DescriptorType::eCombinedImageSampler, descriptorImageInfo, nullptr, nullptr)
		};
		m_device->updateDescriptorSets(writeDescriptorSets, nullptr);

		return image;
	}

	std::unique_ptr<Mesh> VulkanBackend::uploadMesh(std::vector<glm::vec3> vertices,
													std::vector<glm::vec2> texCoords,
													std::vector<glm::vec3> normals,
													std::vector<uint16_t> indices)
	{
		std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>(m_physicalDevice, m_device, vertices.size(), indices.size());

		size_t vertexSize = vertices.size() * sizeof(glm::vec3);
		size_t texCoordSize = texCoords.size() * sizeof(glm::vec2);
		size_t normalSize = normals.size() * sizeof(glm::vec3);
		size_t indexSize = indices.size() * sizeof(uint16_t);
		size_t totalSize = vertexSize + texCoordSize + normalSize + indexSize;

		vk::UniqueBuffer srcBuffer = m_device->createBufferUnique(vk::BufferCreateInfo({}, totalSize,
			vk::BufferUsageFlagBits::eTransferSrc));

		vk::MemoryRequirements memoryRequirements = m_device->getBufferMemoryRequirements(srcBuffer.get());
		uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(), memoryRequirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		vk::UniqueDeviceMemory memory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));

		m_device->bindBufferMemory(srcBuffer.get(), memory.get(), 0);

		uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(memory.get(), 0, totalSize));
		memcpy(pData + 0, vertices.data(), vertexSize);
		memcpy(pData + vertexSize, texCoords.data(), texCoordSize);
		memcpy(pData + vertexSize + texCoordSize, normals.data(), normalSize);
		memcpy(pData + vertexSize + texCoordSize + normalSize, indices.data(), indexSize);
		m_device->unmapMemory(memory.get());

		vk::UniqueCommandBuffer commandBuffer = std::move(m_device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
			m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1)).front());
		commandBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));
		commandBuffer->copyBuffer(srcBuffer.get(), mesh->vertexBuffer.get(), 	vk::BufferCopy(0, 0, vertexSize));
		commandBuffer->copyBuffer(srcBuffer.get(), mesh->texCoordBuffer.get(), 	vk::BufferCopy(vertexSize, 0, texCoordSize));
		commandBuffer->copyBuffer(srcBuffer.get(), mesh->normalBuffer.get(), 	vk::BufferCopy(vertexSize + texCoordSize, 0, normalSize));
		commandBuffer->copyBuffer(srcBuffer.get(), mesh->indexBuffer.get(), 	vk::BufferCopy(vertexSize + texCoordSize + normalSize, 0, indexSize));
		commandBuffer->end();

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eTransfer);
		m_queue.submit(vk::SubmitInfo(0, nullptr, &waitDestinationStageMask, 1, &commandBuffer.get()), m_transferFence.get());
		vk::Result r = m_device->waitForFences(m_transferFence.get(), true, UINT64_MAX);
		m_device->resetFences(m_transferFence.get());

		assert(r == vk::Result::eSuccess);

		return mesh;
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
		if(m_depthImage)
			delete m_depthImage;
	}
}
