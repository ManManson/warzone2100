// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(WZ_VULKAN_ENABLED)

#include "gfx_api_vk_as.h"

#include "gfx_api_vk.h"
#include "src/scene_description.h"

#include "lib/framework/wzglobal.h"

#include <glm/gtc/type_ptr.hpp>

#include <unordered_map>
#include <vector>

namespace
{

bool extensionEnabled(const std::vector<const char*>& enabledExtensions, const char* name)
{
	return std::find_if(enabledExtensions.begin(), enabledExtensions.end(),
		[name](const char* ext) { return strcmp(ext, name) == 0; }) != enabledExtensions.end();
}

const VkBuf* toVkBuf(const gfx_api::buffer* buffer)
{
	return dynamic_cast<const VkBuf*>(buffer);
}

struct AsGpuBuffer
{
	vk::Buffer buffer {};
	VmaAllocation allocation = VK_NULL_HANDLE;
	vk::DeviceSize size = 0;
};

struct CachedBlas
{
	vk::AccelerationStructureKHR handle {};
	AsGpuBuffer structure;
	AsGpuBuffer scratch;
	AsGpuBuffer vertexData;
	AsGpuBuffer indexData;
	vk::DeviceAddress structureAddress = 0;
};

struct AsBuildContext
{
	VkRoot* root = nullptr;
	vk::Device device {};
	VmaAllocator allocator = VK_NULL_HANDLE;
	vk::Queue queue {};
	uint32_t queueFamilyIndex = 0;
	const WZ_vk::DispatchLoaderDynamic* loader = nullptr;
	vk::PhysicalDeviceAccelerationStructurePropertiesKHR asProperties {};
};

AsGpuBuffer createDeviceAddressBuffer(const AsBuildContext& ctx, vk::DeviceSize size, vk::BufferUsageFlags extraUsage)
{
	AsGpuBuffer result;
	if (size == 0)
	{
		return result;
	}

	const vk::BufferUsageFlags usage =
		vk::BufferUsageFlagBits::eShaderDeviceAddress
		| vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
		| vk::BufferUsageFlagBits::eTransferDst
		| extraUsage;

	const vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive);

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	const VkResult createResult = vmaCreateBuffer(
		ctx.allocator,
		reinterpret_cast<const VkBufferCreateInfo*>(&bufferInfo),
		&allocInfo,
		reinterpret_cast<VkBuffer*>(&result.buffer),
		&result.allocation,
		nullptr);
	if (createResult != VK_SUCCESS)
	{
		debug(LOG_ERROR, "VkASManager: failed to create device-address buffer");
		return {};
	}

	result.size = size;
	return result;
}

void destroyAsGpuBuffer(const AsBuildContext& ctx, AsGpuBuffer& buffer)
{
	if (buffer.buffer && buffer.allocation != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(ctx.allocator, static_cast<VkBuffer>(buffer.buffer), buffer.allocation);
	}
	buffer = {};
}

vk::DeviceAddress getBufferDeviceAddress(const AsBuildContext& ctx, vk::Buffer buffer)
{
	const vk::BufferDeviceAddressInfo addressInfo = vk::BufferDeviceAddressInfo().setBuffer(buffer);
	return ctx.device.getBufferAddress(addressInfo, *ctx.loader);
}

bool copyBufferRegion(const AsBuildContext& ctx, vk::CommandBuffer cmd,
                      vk::Buffer src, vk::DeviceSize srcOffset,
                      vk::Buffer dst, vk::DeviceSize dstOffset, vk::DeviceSize size)
{
	if (size == 0)
	{
		return true;
	}

	const vk::BufferCopy copyRegion = vk::BufferCopy().setSrcOffset(srcOffset).setDstOffset(dstOffset).setSize(size);
	cmd.copyBuffer(src, dst, copyRegion, *ctx.loader);
	return true;
}

void destroyCachedBlas(const AsBuildContext& ctx, CachedBlas& blas)
{
	if (blas.handle)
	{
		ctx.device.destroyAccelerationStructureKHR(blas.handle, nullptr, *ctx.loader);
	}
	destroyAsGpuBuffer(ctx, blas.structure);
	destroyAsGpuBuffer(ctx, blas.scratch);
	destroyAsGpuBuffer(ctx, blas.vertexData);
	destroyAsGpuBuffer(ctx, blas.indexData);
	blas = {};
}

bool buildBlas(const AsBuildContext& ctx, vk::CommandBuffer cmd, const SceneDescription::StaticBlasEntry& entry, CachedBlas& outBlas)
{
	const VkBuf* srcVertices = toVkBuf(entry.mesh.vertexBuffer);
	const VkBuf* srcIndices = toVkBuf(entry.mesh.indexBuffer);
	ASSERT_OR_RETURN(false, srcVertices != nullptr && srcIndices != nullptr, "VkASManager: invalid terrain buffers");
	ASSERT_OR_RETURN(false, entry.mesh.vertexCount > 0 && entry.mesh.indexCount > 0, "VkASManager: empty mesh");

	const vk::DeviceSize vertexBytes = static_cast<vk::DeviceSize>(entry.mesh.vertexCount) * entry.mesh.vertexStride;
	const vk::DeviceSize indexBytes = static_cast<vk::DeviceSize>(entry.mesh.indexCount) * (entry.mesh.uses32BitIndices ? sizeof(uint32_t) : sizeof(uint16_t));

	outBlas.vertexData = createDeviceAddressBuffer(ctx, vertexBytes, vk::BufferUsageFlagBits::eVertexBuffer);
	outBlas.indexData = createDeviceAddressBuffer(ctx, indexBytes, vk::BufferUsageFlagBits::eIndexBuffer);
	ASSERT_OR_RETURN(false, outBlas.vertexData.buffer && outBlas.indexData.buffer, "VkASManager: failed to allocate BLAS geometry buffers");

	copyBufferRegion(ctx, cmd,
		srcVertices->object, entry.mesh.vertexByteOffset,
		outBlas.vertexData.buffer, 0, vertexBytes);
	copyBufferRegion(ctx, cmd,
		srcIndices->object, entry.mesh.indexByteOffset,
		outBlas.indexData.buffer, 0, indexBytes);

	const vk::DeviceAddress vertexAddress = getBufferDeviceAddress(ctx, outBlas.vertexData.buffer);
	const vk::DeviceAddress indexAddress = getBufferDeviceAddress(ctx, outBlas.indexData.buffer);

	vk::AccelerationStructureGeometryTrianglesDataKHR triangles = vk::AccelerationStructureGeometryTrianglesDataKHR()
		.setVertexFormat(vk::Format::eR32G32B32Sfloat)
		.setVertexData(vertexAddress)
		.setVertexStride(entry.mesh.vertexStride)
		.setMaxVertex(entry.mesh.vertexCount)
		.setIndexType(entry.mesh.uses32BitIndices ? vk::IndexType::eUint32 : vk::IndexType::eUint16)
		.setIndexData(indexAddress);

	vk::AccelerationStructureGeometryKHR geometry = vk::AccelerationStructureGeometryKHR()
		.setGeometryType(vk::GeometryTypeKHR::eTriangles)
		.setGeometry(triangles);

	const uint32_t primitiveCount = entry.mesh.indexCount / 3;
	vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
		.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
		.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
		.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
		.setDstAccelerationStructure({})
		.setGeometries(geometry)
		.setGeometryCount(1);

	vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
		ctx.device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, geometryInfo, primitiveCount, *ctx.loader);

	outBlas.structure = createDeviceAddressBuffer(ctx, sizeInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);
	outBlas.scratch = createDeviceAddressBuffer(ctx, sizeInfo.buildScratchSize, {});
	ASSERT_OR_RETURN(false, outBlas.structure.buffer && outBlas.scratch.buffer, "VkASManager: failed to allocate BLAS structure/scratch");

	const vk::AccelerationStructureCreateInfoKHR createInfo = vk::AccelerationStructureCreateInfoKHR()
		.setBuffer(outBlas.structure.buffer)
		.setSize(sizeInfo.accelerationStructureSize)
		.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

	outBlas.handle = ctx.device.createAccelerationStructureKHR(createInfo, nullptr, *ctx.loader);
	outBlas.structureAddress = getBufferDeviceAddress(ctx, outBlas.structure.buffer);

	geometryInfo.setDstAccelerationStructure(outBlas.handle);
	geometryInfo.setScratchData(getBufferDeviceAddress(ctx, outBlas.scratch.buffer));

	const vk::AccelerationStructureBuildRangeInfoKHR rangeInfo = vk::AccelerationStructureBuildRangeInfoKHR()
		.setPrimitiveCount(primitiveCount);

	const vk::AccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };
	cmd.buildAccelerationStructuresKHR(geometryInfo, rangeInfos, *ctx.loader);
	return true;
}

vk::TransformMatrixKHR toVkTransform(const glm::mat4& transform)
{
	vk::TransformMatrixKHR matrix {};
	const glm::mat4 transposed = glm::transpose(transform);
	for (uint32_t row = 0; row < 3; ++row)
	{
		for (uint32_t col = 0; col < 4; ++col)
		{
			matrix.matrix[row][col] = transposed[col][row];
		}
	}
	return matrix;
}

} // namespace

struct VkASManager::Impl
{
	AsBuildContext ctx {};
	std::unordered_map<uint64_t, CachedBlas> blasCache;
	CachedBlas tlas {};
	vk::CommandPool commandPool {};
	vk::Fence fence {};
	bool supported = false;
};

VkASManager::~VkASManager()
{
	shutdown();
}

void VkASManager::init(VkRoot& root)
{
	shutdown();
	_root = &root;
	_impl = new Impl();

#if !defined(VK_KHR_acceleration_structure)
	debug(LOG_3D, "VkASManager: VK_KHR_acceleration_structure not available in headers");
	return;
#else
	const bool hasAs = extensionEnabled(root.enabledDeviceExtensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	const bool hasRayQuery = extensionEnabled(root.enabledDeviceExtensions, VK_KHR_RAY_QUERY_EXTENSION_NAME);
	const bool hasBda = extensionEnabled(root.enabledDeviceExtensions, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	const bool hasDeferredHostOps = extensionEnabled(root.enabledDeviceExtensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	if (!hasAs || !hasRayQuery || !hasBda || !hasDeferredHostOps)
	{
		debug(LOG_3D, "VkASManager: RT extensions not enabled (as=%d rayQuery=%d bda=%d deferred=%d)",
		      hasAs, hasRayQuery, hasBda, hasDeferredHostOps);
		return;
	}

	_impl->ctx.root = &root;
	_impl->ctx.device = root.dev;
	_impl->ctx.allocator = root.allocator;
	_impl->ctx.queue = root.graphicsQueue;
	_impl->ctx.queueFamilyIndex = root.queueFamilyIndices.graphicsFamily.value();
	_impl->ctx.loader = &root.vkDynLoader;

	vk::StructureChain<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceAccelerationStructurePropertiesKHR> propertiesChain;
	root.physicalDevice.getProperties2(&propertiesChain.get(), root.vkDynLoader);
	_impl->ctx.asProperties = propertiesChain.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

	_impl->commandPool = root.dev.createCommandPool(
		vk::CommandPoolCreateInfo()
			.setQueueFamilyIndex(_impl->ctx.queueFamilyIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eTransient),
		nullptr, root.vkDynLoader);
	_impl->fence = root.dev.createFence(vk::FenceCreateInfo(), nullptr, root.vkDynLoader);

	_impl->supported = true;
	_capabilities.accelerationStructure = true;
	_capabilities.rayQuery = true;
	_initialized = true;
	debug(LOG_3D, "VkASManager: initialized (maxPrimitiveCount=%" PRIu64 ")",
	      static_cast<uint64_t>(_impl->ctx.asProperties.maxPrimitiveCount));
#endif
}

void VkASManager::shutdown()
{
	if (!_impl)
	{
		_capabilities = {};
		_initialized = false;
		_root = nullptr;
		return;
	}

	if (_initialized && _root)
	{
		AsBuildContext ctx = _impl->ctx;
		for (auto& pair : _impl->blasCache)
		{
			destroyCachedBlas(ctx, pair.second);
		}
		_impl->blasCache.clear();
		destroyCachedBlas(ctx, _impl->tlas);

		if (_impl->fence)
		{
			_root->dev.destroyFence(_impl->fence, nullptr, _root->vkDynLoader);
			_impl->fence = vk::Fence();
		}
		if (_impl->commandPool)
		{
			_root->dev.destroyCommandPool(_impl->commandPool, nullptr, _root->vkDynLoader);
			_impl->commandPool = vk::CommandPool();
		}
	}

	delete _impl;
	_impl = nullptr;
	_capabilities = {};
	_initialized = false;
	_root = nullptr;
}

void VkASManager::build(const SceneDescription& scene)
{
	if (!_initialized || !_impl || !_impl->supported)
	{
		return;
	}

	const auto& staticEntries = scene.staticBlas();
	if (staticEntries.empty())
	{
		return;
	}

	AsBuildContext& ctx = _impl->ctx;
	destroyCachedBlas(ctx, _impl->tlas);

	const vk::CommandBuffer cmd = ctx.device.allocateCommandBuffers(
		vk::CommandBufferAllocateInfo()
			.setCommandPool(_impl->commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1),
		*ctx.loader).front();

	cmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit), *ctx.loader);

	std::vector<vk::AccelerationStructureInstanceKHR> instances;
	instances.reserve(staticEntries.size());

	for (const SceneDescription::StaticBlasEntry& entry : staticEntries)
	{
		CachedBlas& cached = _impl->blasCache[entry.cacheKey];
		if (!cached.handle)
		{
			if (!buildBlas(ctx, cmd, entry, cached))
			{
				cmd.end(*ctx.loader);
				ctx.device.freeCommandBuffers(_impl->commandPool, 1, &cmd, *ctx.loader);
				return;
			}
		}

		vk::AccelerationStructureInstanceKHR instance = vk::AccelerationStructureInstanceKHR()
			.setTransform(toVkTransform(entry.transform))
			.setInstanceCustomIndex(0)
			.setMask(0xFF)
			.setInstanceShaderBindingTableRecordOffset(0)
			.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
			.setAccelerationStructureReference(cached.structureAddress);
		instances.push_back(instance);
	}

	const vk::DeviceSize instanceBufferSize = instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
	AsGpuBuffer instanceBuffer = createDeviceAddressBuffer(ctx, instanceBufferSize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
	ASSERT_OR_RETURN(, instanceBuffer.buffer, "VkASManager: failed to allocate TLAS instance buffer");

	void* mapped = nullptr;
	vmaMapMemory(ctx.allocator, instanceBuffer.allocation, &mapped);
	memcpy(mapped, instances.data(), static_cast<size_t>(instanceBufferSize));
	vmaUnmapMemory(ctx.allocator, instanceBuffer.allocation);

	vk::AccelerationStructureGeometryInstancesDataKHR instancesData = vk::AccelerationStructureGeometryInstancesDataKHR()
		.setData(getBufferDeviceAddress(ctx, instanceBuffer.buffer));

	vk::AccelerationStructureGeometryKHR tlasGeometry = vk::AccelerationStructureGeometryKHR()
		.setGeometryType(vk::GeometryTypeKHR::eInstances)
		.setGeometry(instancesData);

	const vk::AccelerationStructureBuildGeometryInfoKHR tlasGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
		.setType(vk::AccelerationStructureTypeKHR::eTopLevel)
		.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
		.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
		.setGeometries(tlasGeometry)
		.setGeometryCount(1);

	const vk::AccelerationStructureBuildSizesInfoKHR tlasSizeInfo =
		ctx.device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, tlasGeometryInfo, static_cast<uint32_t>(instances.size()), *ctx.loader);

	_impl->tlas.structure = createDeviceAddressBuffer(ctx, tlasSizeInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);
	_impl->tlas.scratch = createDeviceAddressBuffer(ctx, tlasSizeInfo.buildScratchSize, {});
	ASSERT_OR_RETURN(, _impl->tlas.structure.buffer && _impl->tlas.scratch.buffer, "VkASManager: failed to allocate TLAS buffers");

	_impl->tlas.handle = ctx.device.createAccelerationStructureKHR(
		vk::AccelerationStructureCreateInfoKHR()
			.setBuffer(_impl->tlas.structure.buffer)
			.setSize(tlasSizeInfo.accelerationStructureSize)
			.setType(vk::AccelerationStructureTypeKHR::eTopLevel),
		nullptr, *ctx.loader);
	_impl->tlas.structureAddress = getBufferDeviceAddress(ctx, _impl->tlas.structure.buffer);

	vk::AccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = tlasGeometryInfo;
	tlasBuildInfo.setDstAccelerationStructure(_impl->tlas.handle);
	tlasBuildInfo.setScratchData(getBufferDeviceAddress(ctx, _impl->tlas.scratch.buffer));

	const vk::AccelerationStructureBuildRangeInfoKHR tlasRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR()
		.setPrimitiveCount(static_cast<uint32_t>(instances.size()));
	const vk::AccelerationStructureBuildRangeInfoKHR* tlasRangeInfos[] = { &tlasRangeInfo };
	cmd.buildAccelerationStructuresKHR(tlasBuildInfo, tlasRangeInfos, *ctx.loader);

	const vk::MemoryBarrier memoryBarrier = vk::MemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR)
		.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);
	cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
		vk::PipelineStageFlagBits::eFragmentShader,
		vk::DependencyFlags(),
		memoryBarrier,
		{},
		{},
		*ctx.loader);

	cmd.end(*ctx.loader);

	const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR;
	ctx.queue.submit(vk::SubmitInfo().setCommandBuffers(cmd).setPWaitDstStageMask(&waitStage), _impl->fence, *ctx.loader);
	(void)ctx.device.waitForFences(1, &_impl->fence, VK_TRUE, UINT64_MAX, *ctx.loader);
	(void)ctx.device.resetFences(1, &_impl->fence, *ctx.loader);
	ctx.device.freeCommandBuffers(_impl->commandPool, 1, &cmd, *ctx.loader);

	destroyAsGpuBuffer(ctx, instanceBuffer);
}

#endif // defined(WZ_VULKAN_ENABLED)
