// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(WZ_VULKAN_ENABLED)

#include "gfx_api_vk.h"
#include "gfx_api_vk_as.h"
#include "src/scene_description.h"

#include "lib/framework/wzglobal.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <vector>

namespace
{

constexpr size_t TLAS_SLOT_COUNT = 2;

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

void deferDestroyAsGpuBuffer(AsGpuBuffer& buffer)
{
	if (!buffer.buffer || buffer.allocation == VK_NULL_HANDLE)
	{
		buffer = {};
		return;
	}

	perFrameResources_t& frameResources = buffering_mechanism::get_current_resources();
	frameResources.buffer_to_delete.push_back(buffer.buffer);
	frameResources.vmamemory_to_free.push_back(buffer.allocation);
	buffer = {};
}

void deferDestroyAccelerationStructure(vk::AccelerationStructureKHR& handle)
{
	if (!handle)
	{
		return;
	}

	buffering_mechanism::get_current_resources().acceleration_structure_to_delete.push_back(handle);
	handle = nullptr;
}

vk::DeviceAddress getBufferDeviceAddress(const AsBuildContext& ctx, vk::Buffer buffer)
{
	const vk::BufferDeviceAddressInfo addressInfo = vk::BufferDeviceAddressInfo().setBuffer(buffer);
	return ctx.device.getBufferAddress(addressInfo, *ctx.loader);
}

vk::DeviceAddress getAccelerationStructureDeviceAddress(const AsBuildContext& ctx, vk::AccelerationStructureKHR handle)
{
	const vk::AccelerationStructureDeviceAddressInfoKHR addressInfo =
		vk::AccelerationStructureDeviceAddressInfoKHR().setAccelerationStructure(handle);
	return ctx.device.getAccelerationStructureAddressKHR(addressInfo, *ctx.loader);
}

void barrierGeometryVisibleToAsBuild(const AsBuildContext& ctx, vk::CommandBuffer cmd)
{
	const vk::MemoryBarrier barrier = vk::MemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eVertexAttributeRead)
		.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);
	cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eVertexInput,
		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
		vk::DependencyFlags(),
		barrier,
		{},
		{},
		*ctx.loader);
}

void barrierBlasBuildComplete(const AsBuildContext& ctx, vk::CommandBuffer cmd)
{
	const vk::MemoryBarrier barrier = vk::MemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR)
		.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);
	cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
		vk::DependencyFlags(),
		barrier,
		{},
		{},
		*ctx.loader);
}

vk::DeviceSize scratchBufferAllocationSize(const AsBuildContext& ctx, vk::DeviceSize buildScratchSize)
{
	const vk::DeviceSize alignment = std::max<vk::DeviceSize>(
		ctx.asProperties.minAccelerationStructureScratchOffsetAlignment, 1);
	return buildScratchSize + alignment;
}

AsGpuBuffer createScratchBuffer(const AsBuildContext& ctx, vk::DeviceSize buildScratchSize)
{
	AsGpuBuffer result;
	const vk::DeviceSize allocSize = scratchBufferAllocationSize(ctx, buildScratchSize);
	if (allocSize == 0)
	{
		return result;
	}

	const vk::BufferUsageFlags usage =
		vk::BufferUsageFlagBits::eShaderDeviceAddress
		| vk::BufferUsageFlagBits::eStorageBuffer;

	const vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
		.setSize(allocSize)
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
		debug(LOG_ERROR, "VkASManager: failed to create scratch buffer");
		return {};
	}

	result.size = allocSize;
	return result;
}

vk::DeviceAddress getScratchDeviceAddress(const AsBuildContext& ctx, vk::Buffer scratchBuffer)
{
	const vk::DeviceSize alignment = ctx.asProperties.minAccelerationStructureScratchOffsetAlignment;
	const vk::DeviceAddress base = getBufferDeviceAddress(ctx, scratchBuffer);
	if (alignment <= 1)
	{
		return base;
	}
	const vk::DeviceAddress alignMask = static_cast<vk::DeviceAddress>(alignment - 1);
	return (base + alignMask) & ~alignMask;
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

struct TempStagingBuffer
{
	vk::Buffer buffer {};
	VmaAllocation allocation = VK_NULL_HANDLE;
};

void destroyTempStagingBuffer(const AsBuildContext& ctx, TempStagingBuffer& staging)
{
	if (staging.buffer && staging.allocation != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(ctx.allocator, static_cast<VkBuffer>(staging.buffer), staging.allocation);
	}
	staging = {};
}

void deferDestroyTempStagingBuffer(TempStagingBuffer& staging)
{
	AsGpuBuffer asGpuBuffer;
	asGpuBuffer.buffer = staging.buffer;
	asGpuBuffer.allocation = staging.allocation;
	deferDestroyAsGpuBuffer(asGpuBuffer);
	staging = {};
}

bool uploadToDeviceLocalBuffer(const AsBuildContext& ctx, vk::CommandBuffer cmd,
                               vk::Buffer dst, const void* srcData, vk::DeviceSize size,
                               TempStagingBuffer& stagingOut)
{
	if (size == 0 || srcData == nullptr)
	{
		return false;
	}

	const vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
		.setSharingMode(vk::SharingMode::eExclusive);

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	const VkResult createResult = vmaCreateBuffer(
		ctx.allocator,
		reinterpret_cast<const VkBufferCreateInfo*>(&bufferInfo),
		&allocInfo,
		reinterpret_cast<VkBuffer*>(&stagingOut.buffer),
		&stagingOut.allocation,
		nullptr);
	if (createResult != VK_SUCCESS)
	{
		debug(LOG_ERROR, "VkASManager: failed to create staging buffer");
		return false;
	}

	void* mapped = nullptr;
	const VkResult mapResult = vmaMapMemory(ctx.allocator, stagingOut.allocation, &mapped);
	if (mapResult != VK_SUCCESS || mapped == nullptr)
	{
		destroyTempStagingBuffer(ctx, stagingOut);
		debug(LOG_ERROR, "VkASManager: failed to map staging buffer");
		return false;
	}

	memcpy(mapped, srcData, static_cast<size_t>(size));
	vmaUnmapMemory(ctx.allocator, stagingOut.allocation);

	return copyBufferRegion(ctx, cmd, stagingOut.buffer, 0, dst, 0, size);
}

void destroyCachedBlas(const AsBuildContext& ctx, CachedBlas& blas)
{
	if (blas.handle)
	{
		ctx.device.destroyAccelerationStructureKHR(blas.handle, nullptr, *ctx.loader);
	}
	destroyAsGpuBuffer(ctx, blas.structure);
	destroyAsGpuBuffer(ctx, blas.scratch);
	blas = {};
}

void deferDestroyCachedBlas(CachedBlas& blas)
{
	deferDestroyAccelerationStructure(blas.handle);
	deferDestroyAsGpuBuffer(blas.structure);
	deferDestroyAsGpuBuffer(blas.scratch);
	blas.structureAddress = 0;
}

vk::DeviceAddress meshBufferDeviceAddress(const AsBuildContext& ctx, const gfx_api::buffer* buffer, uint32_t byteOffset)
{
	const VkBuf* vkBuffer = toVkBuf(buffer);
	ASSERT_OR_RETURN(0, vkBuffer != nullptr, "VkASManager: invalid mesh buffer");
	return getBufferDeviceAddress(ctx, vkBuffer->object) + static_cast<vk::DeviceAddress>(byteOffset);
}

bool buildBlasFromGeometries(const AsBuildContext& ctx, vk::CommandBuffer cmd,
                             const std::vector<vk::AccelerationStructureGeometryKHR>& geometries,
                             const std::vector<uint32_t>& primitiveCounts,
                             CachedBlas& outBlas)
{
	ASSERT_OR_RETURN(false, !geometries.empty() && geometries.size() == primitiveCounts.size(),
		"VkASManager: geometry/range mismatch");

	vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
		.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
		.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
		.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
		.setDstAccelerationStructure({})
		.setGeometries(geometries)
		.setGeometryCount(static_cast<uint32_t>(geometries.size()));

	vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
		ctx.device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice,
			geometryInfo,
			primitiveCounts,
			*ctx.loader);

	outBlas.structure = createDeviceAddressBuffer(ctx, sizeInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);
	outBlas.scratch = createScratchBuffer(ctx, sizeInfo.buildScratchSize);
	ASSERT_OR_RETURN(false, outBlas.structure.buffer && outBlas.scratch.buffer, "VkASManager: failed to allocate BLAS structure/scratch");

	outBlas.handle = ctx.device.createAccelerationStructureKHR(
		vk::AccelerationStructureCreateInfoKHR()
			.setBuffer(outBlas.structure.buffer)
			.setSize(sizeInfo.accelerationStructureSize)
			.setType(vk::AccelerationStructureTypeKHR::eBottomLevel),
		nullptr, *ctx.loader);
	outBlas.structureAddress = getAccelerationStructureDeviceAddress(ctx, outBlas.handle);

	geometryInfo.setDstAccelerationStructure(outBlas.handle);
	geometryInfo.setScratchData(getScratchDeviceAddress(ctx, outBlas.scratch.buffer));

	std::vector<vk::AccelerationStructureBuildRangeInfoKHR> rangeInfos;
	rangeInfos.reserve(primitiveCounts.size());
	for (const uint32_t primitiveCount : primitiveCounts)
	{
		rangeInfos.push_back(vk::AccelerationStructureBuildRangeInfoKHR().setPrimitiveCount(primitiveCount));
	}

	const vk::AccelerationStructureBuildRangeInfoKHR* rangeInfoArray[] = { rangeInfos.data() };
	cmd.buildAccelerationStructuresKHR(geometryInfo, rangeInfoArray, *ctx.loader);
	barrierBlasBuildComplete(ctx, cmd);
	return true;
}

bool makeTriangleGeometry(const AsBuildContext& ctx, const SceneDescription::BlasMeshSource& mesh,
                          vk::AccelerationStructureGeometryKHR& outGeometry, uint32_t& outPrimitiveCount)
{
	ASSERT_OR_RETURN(false, mesh.vertexCount > 0 && mesh.indexCount >= 3, "VkASManager: empty mesh");
	ASSERT_OR_RETURN(false, mesh.indexCount % 3 == 0, "VkASManager: index count is not a multiple of 3");

	const vk::DeviceAddress vertexAddress = meshBufferDeviceAddress(ctx, mesh.vertexBuffer, mesh.vertexByteOffset);
	const vk::DeviceAddress indexAddress = meshBufferDeviceAddress(ctx, mesh.indexBuffer, mesh.indexByteOffset);

	vk::AccelerationStructureGeometryTrianglesDataKHR triangles = vk::AccelerationStructureGeometryTrianglesDataKHR()
		.setVertexFormat(vk::Format::eR32G32B32Sfloat)
		.setVertexData(vertexAddress)
		.setVertexStride(mesh.vertexStride)
		.setMaxVertex(mesh.vertexCount > 0 ? mesh.vertexCount - 1 : 0)
		.setIndexType(mesh.uses32BitIndices ? vk::IndexType::eUint32 : vk::IndexType::eUint16)
		.setIndexData(indexAddress);

	outGeometry = vk::AccelerationStructureGeometryKHR()
		.setGeometryType(vk::GeometryTypeKHR::eTriangles)
		.setGeometry(triangles);
	outPrimitiveCount = mesh.indexCount / 3;
	return true;
}

bool buildBlas(const AsBuildContext& ctx, vk::CommandBuffer cmd, const SceneDescription::StaticBlasEntry& entry, CachedBlas& outBlas)
{
	std::vector<vk::AccelerationStructureGeometryKHR> geometries(1);
	uint32_t primitiveCount = 0;
	ASSERT_OR_RETURN(false, makeTriangleGeometry(ctx, entry.mesh, geometries[0], primitiveCount), "VkASManager: invalid mesh geometry");
	return buildBlasFromGeometries(ctx, cmd, geometries, { primitiveCount }, outBlas);
}

bool buildTerrainBatchBlas(const AsBuildContext& ctx, vk::CommandBuffer cmd,
                           const std::vector<const SceneDescription::StaticBlasEntry*>& terrainSectors,
                           CachedBlas& outBlas)
{
	std::vector<vk::AccelerationStructureGeometryKHR> geometries;
	std::vector<uint32_t> primitiveCounts;
	geometries.reserve(terrainSectors.size());
	primitiveCounts.reserve(terrainSectors.size());

	for (const SceneDescription::StaticBlasEntry* entry : terrainSectors)
	{
		if (entry == nullptr)
		{
			continue;
		}
		geometries.emplace_back();
		uint32_t primitiveCount = 0;
		if (!makeTriangleGeometry(ctx, entry->mesh, geometries.back(), primitiveCount))
		{
			geometries.pop_back();
			continue;
		}
		primitiveCounts.push_back(primitiveCount);
	}

	ASSERT_OR_RETURN(false, !geometries.empty(), "VkASManager: no valid terrain geometries");
	return buildBlasFromGeometries(ctx, cmd, geometries, primitiveCounts, outBlas);
}

vk::TransformMatrixKHR toVkTransform(const glm::mat4& transform)
{
	vk::TransformMatrixKHR matrix {};
	for (uint32_t row = 0; row < 3; ++row)
	{
		for (uint32_t col = 0; col < 4; ++col)
		{
			matrix.matrix[row][col] = transform[col][row];
		}
	}
	return matrix;
}

} // namespace

struct VkASManager::Impl
{
	AsBuildContext ctx {};
	std::unordered_map<uint64_t, CachedBlas> blasCache;
	std::array<CachedBlas, TLAS_SLOT_COUNT> terrainBlasSlots {};
	std::array<CachedBlas, TLAS_SLOT_COUNT> tlasSlots {};
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
		for (CachedBlas& terrainBlasSlot : _impl->terrainBlasSlots)
		{
			destroyCachedBlas(ctx, terrainBlasSlot);
		}
		for (CachedBlas& tlasSlot : _impl->tlasSlots)
		{
			destroyCachedBlas(ctx, tlasSlot);
		}
	}

	delete _impl;
	_impl = nullptr;
	_capabilities = {};
	_initialized = false;
	_root = nullptr;
}

void VkASManager::build(const SceneDescription& scene, vk::CommandBuffer cmd)
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
	const size_t tlasSlotIndex = buffering_mechanism::get_current_frame_num() % TLAS_SLOT_COUNT;
	CachedBlas& frameTlas = _impl->tlasSlots[tlasSlotIndex];
	CachedBlas& terrainBlas = _impl->terrainBlasSlots[tlasSlotIndex];
	deferDestroyCachedBlas(frameTlas);

	std::vector<const SceneDescription::StaticBlasEntry*> terrainSectors;
	terrainSectors.reserve(staticEntries.size());
	for (const SceneDescription::StaticBlasEntry& entry : staticEntries)
	{
		if (entry.includeInTlas)
		{
			terrainSectors.push_back(&entry);
		}
	}

	barrierGeometryVisibleToAsBuild(ctx, cmd);

	deferDestroyCachedBlas(terrainBlas);
	if (!terrainSectors.empty())
	{
		if (!buildTerrainBatchBlas(ctx, cmd, terrainSectors, terrainBlas))
		{
			debug(LOG_ERROR, "VkASManager: failed to build terrain BLAS");
			return;
		}
	}

	for (const SceneDescription::StaticBlasEntry& entry : staticEntries)
	{
		if (entry.includeInTlas)
		{
			continue;
		}

		CachedBlas& cached = _impl->blasCache[entry.cacheKey];
		if (!cached.handle)
		{
			if (!buildBlas(ctx, cmd, entry, cached))
			{
				debug(LOG_ERROR, "VkASManager: failed to build mesh BLAS");
				return;
			}
		}
	}

	const auto& dynamicInstances = scene.instances();
	const size_t tlasInstanceCount = (terrainBlas.handle ? 1 : 0) + dynamicInstances.size();
	if (tlasInstanceCount == 0)
	{
		return;
	}

	std::vector<vk::AccelerationStructureInstanceKHR> instances;
	instances.reserve(tlasInstanceCount);

	if (terrainBlas.handle)
	{
		const glm::mat4 terrainTransform = terrainSectors.empty() ? glm::mat4(1.f) : terrainSectors.front()->transform;
		instances.push_back(vk::AccelerationStructureInstanceKHR()
			.setTransform(toVkTransform(terrainTransform))
			.setInstanceCustomIndex(0)
			.setMask(0xFF)
			.setInstanceShaderBindingTableRecordOffset(0)
			.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
			.setAccelerationStructureReference(terrainBlas.structureAddress));
	}

	for (const SceneDescription::DynamicInstance& dynamicInstance : dynamicInstances)
	{
		if (dynamicInstance.blasIndex >= staticEntries.size())
		{
			continue;
		}

		const SceneDescription::StaticBlasEntry& blasEntry = staticEntries[dynamicInstance.blasIndex];
		const auto blasIt = _impl->blasCache.find(blasEntry.cacheKey);
		if (blasIt == _impl->blasCache.end() || !blasIt->second.handle)
		{
			continue;
		}

		instances.push_back(vk::AccelerationStructureInstanceKHR()
			.setTransform(toVkTransform(dynamicInstance.transform))
			.setInstanceCustomIndex(dynamicInstance.instanceCustomIndex)
			.setMask(dynamicInstance.mask)
			.setInstanceShaderBindingTableRecordOffset(0)
			.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
			.setAccelerationStructureReference(blasIt->second.structureAddress));
	}

	if (instances.empty())
	{
		return;
	}

	const vk::DeviceSize instanceBufferSize = instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
	AsGpuBuffer instanceBuffer = createDeviceAddressBuffer(ctx, instanceBufferSize, vk::BufferUsageFlagBits::eTransferDst);
	ASSERT_OR_RETURN(, instanceBuffer.buffer, "VkASManager: failed to allocate TLAS instance buffer");

	TempStagingBuffer instanceStaging {};
	ASSERT_OR_RETURN(, uploadToDeviceLocalBuffer(ctx, cmd, instanceBuffer.buffer, instances.data(), instanceBufferSize, instanceStaging),
		"VkASManager: failed to upload TLAS instance buffer");

	const vk::BufferMemoryBarrier instanceUploadBarrier = vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
		.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR)
		.setBuffer(instanceBuffer.buffer)
		.setOffset(0)
		.setSize(instanceBufferSize);
	cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
		vk::DependencyFlags(),
		{},
		instanceUploadBarrier,
		{},
		*ctx.loader);

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

	frameTlas.structure = createDeviceAddressBuffer(ctx, tlasSizeInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);
	frameTlas.scratch = createScratchBuffer(ctx, tlasSizeInfo.buildScratchSize);
	ASSERT_OR_RETURN(, frameTlas.structure.buffer && frameTlas.scratch.buffer, "VkASManager: failed to allocate TLAS buffers");

	frameTlas.handle = ctx.device.createAccelerationStructureKHR(
		vk::AccelerationStructureCreateInfoKHR()
			.setBuffer(frameTlas.structure.buffer)
			.setSize(tlasSizeInfo.accelerationStructureSize)
			.setType(vk::AccelerationStructureTypeKHR::eTopLevel),
		nullptr, *ctx.loader);
	frameTlas.structureAddress = getAccelerationStructureDeviceAddress(ctx, frameTlas.handle);

	vk::AccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = tlasGeometryInfo;
	tlasBuildInfo.setDstAccelerationStructure(frameTlas.handle);
	tlasBuildInfo.setScratchData(getScratchDeviceAddress(ctx, frameTlas.scratch.buffer));

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

	deferDestroyTempStagingBuffer(instanceStaging);
	deferDestroyAsGpuBuffer(instanceBuffer);
}

void* VkASManager::tlasHandleForBinding() const
{
	if (!_impl)
	{
		return nullptr;
	}
	const size_t tlasSlotIndex = buffering_mechanism::get_current_frame_num() % TLAS_SLOT_COUNT;
	const CachedBlas& frameTlas = _impl->tlasSlots[tlasSlotIndex];
	if (!frameTlas.handle)
	{
		return nullptr;
	}
	return static_cast<void*>(static_cast<VkAccelerationStructureKHR>(frameTlas.handle));
}

#endif // defined(WZ_VULKAN_ENABLED)
