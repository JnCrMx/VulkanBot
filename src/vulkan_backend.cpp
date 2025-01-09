#include "vulkan_backend.h"

#include <bits/stdint-uintn.h>
#include <cstdint>
#include <functional>
#include <glm/fwd.hpp>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <string>
#include <tuple>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

#include "LimitedIncluder.h"

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
		texCoordBuffer = device->createBufferUnique(vk::BufferCreateInfo({}, vertexCount * sizeof(glm::vec2),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));
		vk::MemoryRequirements texCoordMemoryRequirements = device->getBufferMemoryRequirements(texCoordBuffer.get());
		normalBuffer = device->createBufferUnique(vk::BufferCreateInfo({}, vertexCount * sizeof(glm::vec3),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));
		vk::MemoryRequirements normalMemoryRequirements = device->getBufferMemoryRequirements(normalBuffer.get());

		vk::DeviceSize totalSize = vertexMemoryRequirements.size + texCoordMemoryRequirements.size + normalMemoryRequirements.size;
		uint32_t typeBits = vertexMemoryRequirements.memoryTypeBits | texCoordMemoryRequirements.memoryTypeBits | normalMemoryRequirements.memoryTypeBits;
		uint32_t memoryTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), typeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		memory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(totalSize, memoryTypeIndex));

		device->bindBufferMemory(vertexBuffer.get(), 	memory.get(), 0);
		device->bindBufferMemory(texCoordBuffer.get(), 	memory.get(), vertexMemoryRequirements.size);
		device->bindBufferMemory(normalBuffer.get(), 	memory.get(), vertexMemoryRequirements.size + texCoordMemoryRequirements.size);

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

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		vk::DebugUtilsMessageSeverityFlagBitsEXT severity = (vk::DebugUtilsMessageSeverityFlagBitsEXT) messageSeverity;
		vk::DebugUtilsMessageTypeFlagsEXT type(messageType);

		std::cout << "[Vulkan: " << vk::to_string(type) << " " << vk::to_string(severity) << "]: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}

	void VulkanBackend::initVulkan(int width, int height,
		const std::filesystem::path& shadersPath, const std::filesystem::path& shaderIncludePath,
		bool validation, int debugSeverity, int debugType)
	{
		m_width = static_cast<uint32_t>(width);
		m_height = static_cast<uint32_t>(height);
		m_shadersPath = shadersPath;
		m_shaderIncludePath = shaderIncludePath;

		vk::ApplicationInfo applicationInfo("VulkanBot", 1, "VulkanBot", 1, VK_API_VERSION_1_1);

		std::vector<const char*> layers;
		std::vector<const char*> extensions;
		if(validation)
		{
			layers.push_back("VK_LAYER_KHRONOS_validation");
			extensions.push_back("VK_EXT_debug_utils");
		}
		vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, layers, extensions);
		m_instance = vk::createInstanceUnique(instanceCreateInfo);

		if(validation)
		{
			m_dispatch = vk::DispatchLoaderDynamic(m_instance.get(), vkGetInstanceProcAddr);
			m_debugMessenger = m_instance->createDebugUtilsMessengerEXTUnique(vk::DebugUtilsMessengerCreateInfoEXT({},
				vk::DebugUtilsMessageSeverityFlagsEXT(debugSeverity), vk::DebugUtilsMessageTypeFlagsEXT(debugType),
				&debugCallback), nullptr, m_dispatch);
		}

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
		m_device = m_physicalDevice.createDeviceUnique(vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo, layers));

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
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		m_depthImage = new ImageData(m_physicalDevice, m_device, vk::Format::eD32Sfloat,
			{m_width, m_height},
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth);

		m_encodedImageY = std::make_unique<ImageData>(m_physicalDevice, m_device, vk::Format::eR8Unorm,
			vk::Extent2D{m_width, m_height},
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		m_encodedImageCr = std::make_unique<ImageData>(m_physicalDevice, m_device, vk::Format::eR8Unorm,
			vk::Extent2D{m_width/2, m_height/2},
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		m_encodedImageCb = std::make_unique<ImageData>(m_physicalDevice, m_device, vk::Format::eR8Unorm,
			vk::Extent2D{m_width/2, m_height/2},
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eDeviceLocal);

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
		{
			m_outputStorageBuffer = m_device->createBufferUnique(vk::BufferCreateInfo({}, sizeof(OutputStorageObject),
				vk::BufferUsageFlagBits::eStorageBuffer));
			vk::MemoryRequirements memoryRequirements = m_device->getBufferMemoryRequirements(m_outputStorageBuffer.get());
			uint32_t memoryTypeIndex = findMemoryType(m_physicalDevice.getMemoryProperties(),
				memoryRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			m_outputStorageMemory = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
			m_device->bindBufferMemory(m_outputStorageBuffer.get(), m_outputStorageMemory.get(), 0);
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
		std::array<vk::SubpassDependency, 2> dependencies = {
			vk::SubpassDependency{VK_SUBPASS_EXTERNAL, 0,
				vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
				vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests, {},
				vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite, {}},
			vk::SubpassDependency{0, vk::SubpassExternal,
				vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead},
		};

		m_renderPass = m_device->createRenderPassUnique(
			vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpass, dependencies));

		std::array<vk::ImageView, 2> attachments = {
			m_renderImage->imageView.get(),
			m_depthImage->imageView.get()
		};
		m_framebuffer = m_device->createFramebufferUnique(
			vk::FramebufferCreateInfo({}, m_renderPass.get(), attachments, m_width, m_height, 1));

		std::array<vk::DescriptorSetLayoutBinding, 2> bindings;
		bindings[0] = vk::DescriptorSetLayoutBinding(
			0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute);
		bindings[1] = vk::DescriptorSetLayoutBinding(
			1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute);
		m_descriptorSetLayout = m_device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo({}, bindings));

		std::array<vk::DescriptorSetLayoutBinding, 1> computeBindings = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
		};
		m_computeDescriptorSetLayout = m_device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo({}, computeBindings));

		std::array<vk::DescriptorSetLayoutBinding, 4> encodeBindings = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute)
		};
		m_descriptorSetLayoutEncode = m_device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo({}, encodeBindings));

		vk::DescriptorPoolSize poolSize(vk::DescriptorType::eCombinedImageSampler, 1);
		vk::DescriptorPoolSize uniformPoolSize(vk::DescriptorType::eUniformBuffer, 1);
		vk::DescriptorPoolSize storagePoolSize(vk::DescriptorType::eStorageBuffer, 1);
		vk::DescriptorPoolSize encodePoolSize(vk::DescriptorType::eStorageImage, 4);
		std::array<vk::DescriptorPoolSize, 4> poolSizes{poolSize, uniformPoolSize, storagePoolSize, encodePoolSize};

		m_descriptorPool = m_device->createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 3, poolSizes));

		m_descriptorSet = std::move(
			m_device->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(m_descriptorPool.get(), m_descriptorSetLayout.get())).front());
		m_computeDescriptorSet = std::move(
			m_device->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(m_descriptorPool.get(), m_computeDescriptorSetLayout.get())).front());
		m_descriptorSetEncode = std::move(
			m_device->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(m_descriptorPool.get(), m_descriptorSetLayoutEncode.get())).front());

		vk::DescriptorBufferInfo descriptorBufferInfo(m_uniformBuffer.get(), 0, sizeof(UniformBufferObject));
		vk::DescriptorBufferInfo descriptorStorageInfo(m_outputStorageBuffer.get(), 0, sizeof(OutputStorageObject));
		vk::DescriptorImageInfo encodeImage1(nullptr, m_renderImage->imageView.get(), vk::ImageLayout::eGeneral);
		vk::DescriptorImageInfo encodeImageY(nullptr, m_encodedImageY->imageView.get(), vk::ImageLayout::eGeneral);
		vk::DescriptorImageInfo encodeImageCr(nullptr, m_encodedImageCr->imageView.get(), vk::ImageLayout::eGeneral);
		vk::DescriptorImageInfo encodeImageCb(nullptr, m_encodedImageCb->imageView.get(), vk::ImageLayout::eGeneral);
		std::array<vk::WriteDescriptorSet, 6> writeDescriptorSets = {
			vk::WriteDescriptorSet(m_descriptorSet.get(), 1, 0, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfo, nullptr),
			vk::WriteDescriptorSet(m_computeDescriptorSet.get(), 0, 0, vk::DescriptorType::eStorageBuffer, nullptr, descriptorStorageInfo, nullptr),

			vk::WriteDescriptorSet(m_descriptorSetEncode.get(), 0, 0, vk::DescriptorType::eStorageImage, encodeImage1),
			vk::WriteDescriptorSet(m_descriptorSetEncode.get(), 1, 0, vk::DescriptorType::eStorageImage, encodeImageY),
			vk::WriteDescriptorSet(m_descriptorSetEncode.get(), 2, 0, vk::DescriptorType::eStorageImage, encodeImageCr),
			vk::WriteDescriptorSet(m_descriptorSetEncode.get(), 3, 0, vk::DescriptorType::eStorageImage, encodeImageCb),
		};
		m_device->updateDescriptorSets(writeDescriptorSets, nullptr);

		m_pipelineLayout = m_device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo({}, m_descriptorSetLayout.get()));
		m_pipelineLayoutEncode = m_device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo({}, m_descriptorSetLayoutEncode.get()));

		std::array<vk::DescriptorSetLayout, 2> computeLayouts = {m_descriptorSetLayout.get(), m_computeDescriptorSetLayout.get()};
		m_computePipelineLayout = m_device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo({}, computeLayouts));

		m_commandBuffer = std::move(m_device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
									m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1)).front());
		m_computeCommandBuffer = std::move(m_device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
									m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1)).front());

		m_encodePipeline = createEncodePipeline();
	}

	static std::vector<char> readFile(const std::filesystem::path& filename)
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

	vk::UniquePipeline VulkanBackend::createComputePipeline(vk::UniqueShaderModule& computeShader)
	{
		vk::PipelineShaderStageCreateInfo computeShaderInfo({}, vk::ShaderStageFlagBits::eCompute, computeShader.get(), "main");

		vk::Result result;
		vk::UniquePipeline pipeline;
		std::tie( result, pipeline ) = m_device->createComputePipelineUnique(nullptr,
			vk::ComputePipelineCreateInfo({}, computeShaderInfo, m_computePipelineLayout.get())).asTuple();
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

	vk::UniquePipeline VulkanBackend::createEncodePipeline()
	{
		vk::UniqueShaderModule computeShader = createShader(readFile(m_shadersPath / "yuv420p_encode.comp.spv"));
		vk::PipelineShaderStageCreateInfo shaderInfo({}, vk::ShaderStageFlagBits::eCompute, computeShader.get(), "main");

		vk::Result result;
		vk::UniquePipeline pipeline;
		std::tie( result, pipeline ) = m_device->createComputePipelineUnique(nullptr,
			vk::ComputePipelineCreateInfo({}, shaderInfo, m_pipelineLayoutEncode.get())).asTuple();
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

	std::tuple<bool, std::string> compileShader(EShLanguage stage, std::string glslCode, std::vector<unsigned int>& shaderCode, const std::filesystem::path& includePath)
	{
		vulkan_bot::LimitedIncluder includer(includePath);

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
		if(!shader.parse(GetDefaultResources(), 100, false, messages, includer))
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
					vertexShader = createShader(readFile(m_shadersPath / (vertex+".vert.spv")));
				}
				catch(const std::runtime_error& err)
				{
					vertexResult = {false, err.what()};
				}
			}
		}
		else
		{
			vertexResult = compileShader(EShLangVertex, vertex, vertexBin, m_shaderIncludePath);
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
					fragmentShader = createShader(readFile(m_shadersPath / (fragment+".frag.spv")));
				}
				catch(const std::runtime_error& err)
				{
					fragmentResult = {false, err.what()};
				}
			}
		}
		else
		{
			fragmentResult = compileShader(EShLangFragment, fragment, fragmentBin, m_shaderIncludePath);
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

	std::tuple<bool, std::string> VulkanBackend::uploadComputeShader(const std::string compute, bool file)
	{
		std::vector<unsigned int> computeBin;
		std::tuple<bool, std::string> computeResult = {true, ""};
		vk::UniqueShaderModule computeShader;

		glslang::InitializeProcess();
		if(file)
		{
			if(compute.find("/") != std::string::npos)
				computeResult = {false, "shader name contains /"};
			else
			{
				try
				{
					computeShader = createShader(readFile(m_shadersPath / (compute+".comp.spv")));
				}
				catch(const std::runtime_error& err)
				{
					computeResult = {false, err.what()};
				}
			}
		}
		else
		{
			computeResult = compileShader(EShLangCompute, compute, computeBin, m_shaderIncludePath);
			if(std::get<0>(computeResult))
				computeShader = createShader(computeBin);
		}

		if(!std::get<0>(computeResult))
			return {std::get<0>(computeResult), "compute: "+std::get<1>(computeResult)};

		m_computePipeline = createComputePipeline(computeShader);
		return {true, ""};
	}

	void VulkanBackend::buildCommandBuffer(Mesh* mesh, bool yuv420p)
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

		// I have no idea why we need this... but it works... Oh well, it's staying here.
		m_commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
			{}, {}, {},
			vk::ImageMemoryBarrier(
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
				vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				m_renderImage->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));

		if(yuv420p)
		{
			m_commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
				{}, {}, {}, {
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead,
					vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					m_renderImage->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
				vk::ImageMemoryBarrier(
					{}, vk::AccessFlagBits::eShaderWrite,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					m_encodedImageY->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
				vk::ImageMemoryBarrier(
					{}, vk::AccessFlagBits::eShaderWrite,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					m_encodedImageCr->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
				vk::ImageMemoryBarrier(
					{}, vk::AccessFlagBits::eShaderWrite,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					m_encodedImageCb->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))});

			m_commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, m_encodePipeline.get());
			m_commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipelineLayoutEncode.get(), 0, m_descriptorSetEncode.get(), {});
			m_commandBuffer->dispatch(m_width/2, m_height/2, 1);

			m_commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
				{}, {}, {}, {
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead,
					vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					m_encodedImageY->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead,
					vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					m_encodedImageCr->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
				vk::ImageMemoryBarrier(
					vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead,
					vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					m_encodedImageCb->image.get(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))});

			std::array<vk::BufferImageCopy, 1> regions;
			regions[0] = vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
				{0, 0, 0}, {m_width, m_height, 1});
			m_commandBuffer->copyImageToBuffer(m_encodedImageY->image.get(), vk::ImageLayout::eTransferSrcOptimal, m_outputImageBuffer.get(), regions);

			regions[0] = vk::BufferImageCopy(m_width*m_height, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
				{0, 0, 0}, {m_width/2, m_height/2, 1});
			m_commandBuffer->copyImageToBuffer(m_encodedImageCr->image.get(), vk::ImageLayout::eTransferSrcOptimal, m_outputImageBuffer.get(), regions);

			regions[0] = vk::BufferImageCopy(m_width*m_height * 1.25, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
				{0, 0, 0}, {m_width/2, m_height/2, 1});
			m_commandBuffer->copyImageToBuffer(m_encodedImageCb->image.get(), vk::ImageLayout::eTransferSrcOptimal, m_outputImageBuffer.get(), regions);
		}
		else
		{
			std::array<vk::BufferImageCopy, 1> regions = {
				vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {0, 0, 0}, {m_width, m_height, 1})
			};
			m_commandBuffer->copyImageToBuffer(m_renderImage->image.get(), vk::ImageLayout::eTransferSrcOptimal, m_outputImageBuffer.get(), regions);
		}

		m_commandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
			{}, {},
			vk::BufferMemoryBarrier(
				vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eHostRead,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_outputImageBuffer.get(), 0, VK_WHOLE_SIZE),
			{}
		);

		m_commandBuffer->end();
	}

	void VulkanBackend::buildComputeCommandBuffer(int x, int y, int z)
	{
		m_computeCommandBuffer->reset();
		m_computeCommandBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

		m_computeCommandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipeline.get());
		m_computeCommandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_computePipelineLayout.get(), 0,
			{m_descriptorSet.get(), m_computeDescriptorSet.get()}, {});
		m_computeCommandBuffer->dispatch(x, y, z);

		m_computeCommandBuffer->end();
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

	void VulkanBackend::renderFrame(std::function<void(uint8_t*, vk::DeviceSize, int, int, vk::Result, long)> consumer, bool yuv420p)
	{
		m_queue.submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &m_commandBuffer.get()), m_fence.get());

		auto t1 = std::chrono::high_resolution_clock::now();
		vk::Result r = m_device->waitForFences(m_fence.get(), true, UINT64_MAX);
		auto t2 = std::chrono::high_resolution_clock::now();
		long duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

		m_device->resetFences(m_fence.get());

		auto size = (m_width * m_height) * (yuv420p ? 1.5 : 4);
		uint8_t *pData = static_cast<uint8_t *>(m_device->mapMemory(m_outputImageMemory.get(), 0, size));
		consumer(pData, size, m_width, m_height, r, duration);
		m_device->unmapMemory(m_outputImageMemory.get());
	}

	void VulkanBackend::doComputation(std::function<void(OutputStorageObject*, vk::Result, long)> consumer)
	{
		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eComputeShader);
		m_queue.submit(vk::SubmitInfo(0, nullptr, &waitDestinationStageMask, 1, &m_computeCommandBuffer.get()), m_fence.get());

		auto t1 = std::chrono::high_resolution_clock::now();
		vk::Result r = m_device->waitForFences(m_fence.get(), true, UINT64_MAX);
		auto t2 = std::chrono::high_resolution_clock::now();
		long duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

		m_device->resetFences(m_fence.get());

		OutputStorageObject *pData = static_cast<OutputStorageObject*>(m_device->mapMemory(m_outputStorageMemory.get(), 0, sizeof(OutputStorageObject)));
		consumer(pData, r, duration);
		m_device->unmapMemory(m_outputStorageMemory.get());
	}

	VulkanBackend::~VulkanBackend()
	{
		if(m_renderImage)
			delete m_renderImage;
		if(m_depthImage)
			delete m_depthImage;
	}
}
