// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GLM_FORCE_SWIZZLE
#define GLM_FORCE_SWIZZLE
#endif

#include "ssao.h"

#include "lib/framework/frame.h"
#include "lib/ivis_opengl/gfx_api.h"
#include "lib/ivis_opengl/piedraw.h"
#include "lib/ivis_opengl/screen.h"

#include <glm/glm.hpp>
#include <cstring>

void drawTerrainDepthForSSAO(const glm::mat4& mvp);

namespace ssao
{

namespace
{

constexpr float SSAO_RADIUS = 0.07f;
constexpr float SSAO_BIAS_FACTOR = 0.00002f;
constexpr float SSAO_MIN_BIAS = 0.025f;
constexpr float SSAO_RANGE_SCALE = 0.5f;
constexpr float SSAO_BLUR_DEPTH_THRESHOLD = 0.0015f;
constexpr float SSAO_INTENSITY = 1.0f;
constexpr int NOISE_TEXTURE_SIZE = 4;

gfx_api::texture* s_noiseTexture = nullptr;
glm::vec4 s_kernel[gfx_api::SSAO_KERNEL_SIZE] = {};

void drawSceneBlit(gfx_api::abstract_texture* sourceTexture, gfx_api::buffer* fullscreenTriVBO)
{
	gfx_api::WorldToScreenPSO::get().bind();
	gfx_api::WorldToScreenPSO::get().bind_constants({1.0f});
	gfx_api::WorldToScreenPSO::get().bind_vertex_buffers(fullscreenTriVBO);
	gfx_api::WorldToScreenPSO::get().bind_textures(sourceTexture);
	gfx_api::WorldToScreenPSO::get().draw(3, 0);
	gfx_api::WorldToScreenPSO::get().unbind_vertex_buffers(fullscreenTriVBO);
}

void drawSSAOGenerate(
	gfx_api::abstract_texture* depthTexture,
	const glm::mat4& projectionMatrix,
	const glm::mat4& invProjectionMatrix,
	std::pair<uint32_t, uint32_t> sceneDims,
	gfx_api::buffer* fullscreenTriVBO)
{
	gfx_api::constant_buffer_type<SHADER_SSAO_GENERATE> constants {};
	constants.invProjectionMatrix = invProjectionMatrix;
	constants.projectionMatrix = projectionMatrix;
	constants.params = glm::vec4(
		SSAO_RADIUS,
		SSAO_BIAS_FACTOR,
		SSAO_MIN_BIAS,
		SSAO_RANGE_SCALE);
	constants.noiseScale = glm::vec2(
		static_cast<float>(sceneDims.first) / static_cast<float>(NOISE_TEXTURE_SIZE),
		static_cast<float>(sceneDims.second) / static_cast<float>(NOISE_TEXTURE_SIZE));
	std::memcpy(constants.kernel, s_kernel, sizeof(s_kernel));

	gfx_api::SSAOGeneratePSO::get().bind();
	gfx_api::SSAOGeneratePSO::get().bind_constants(constants);
	gfx_api::SSAOGeneratePSO::get().bind_vertex_buffers(fullscreenTriVBO);
	gfx_api::SSAOGeneratePSO::get().bind_textures(depthTexture, s_noiseTexture);
	gfx_api::SSAOGeneratePSO::get().draw(3, 0);
	gfx_api::SSAOGeneratePSO::get().unbind_vertex_buffers(fullscreenTriVBO);
}

void drawSSAOBlur(
	gfx_api::abstract_texture* occlusionTexture,
	gfx_api::abstract_texture* depthTexture,
	const glm::vec2& blurDirection,
	gfx_api::buffer* fullscreenTriVBO)
{
	gfx_api::constant_buffer_type<SHADER_SSAO_BLUR> constants {};
	constants.blurDirection = blurDirection;
	constants.depthThreshold = SSAO_BLUR_DEPTH_THRESHOLD;

	gfx_api::SSAOBlurPSO::get().bind();
	gfx_api::SSAOBlurPSO::get().bind_constants(constants);
	gfx_api::SSAOBlurPSO::get().bind_vertex_buffers(fullscreenTriVBO);
	gfx_api::SSAOBlurPSO::get().bind_textures(occlusionTexture, depthTexture);
	gfx_api::SSAOBlurPSO::get().draw(3, 0);
	gfx_api::SSAOBlurPSO::get().unbind_vertex_buffers(fullscreenTriVBO);
}

void drawSceneCompose(
	gfx_api::abstract_texture* sceneTexture,
	gfx_api::abstract_texture* ssaoTexture,
	gfx_api::buffer* fullscreenTriVBO)
{
	gfx_api::constant_buffer_type<SHADER_SCENE_COMPOSE_SSAO> constants {};
	constants.ssaoIntensity = SSAO_INTENSITY;

	gfx_api::SceneComposeSSAOPSO::get().bind();
	gfx_api::SceneComposeSSAOPSO::get().bind_constants(constants);
	gfx_api::SceneComposeSSAOPSO::get().bind_vertex_buffers(fullscreenTriVBO);
	gfx_api::SceneComposeSSAOPSO::get().bind_textures(sceneTexture, ssaoTexture);
	gfx_api::SceneComposeSSAOPSO::get().draw(3, 0);
	gfx_api::SceneComposeSSAOPSO::get().unbind_vertex_buffers(fullscreenTriVBO);
}

void initKernel()
{
	for (size_t i = 0; i < gfx_api::SSAO_KERNEL_SIZE; ++i)
	{
		glm::vec3 sample(
			static_cast<float>(rand() % 200) / 100.0f - 1.0f,
			static_cast<float>(rand() % 200) / 100.0f - 1.0f,
			static_cast<float>(rand() % 100) / 100.0f);
		sample = glm::normalize(sample);
		const float scale = static_cast<float>(i) / static_cast<float>(gfx_api::SSAO_KERNEL_SIZE);
		sample *= glm::mix(0.1f, 1.0f, scale * scale);
		s_kernel[i] = glm::vec4(sample, 0.0f);
	}
}

bool initNoiseTexture()
{
	iV_Image noiseImage;
	if (!noiseImage.allocate(NOISE_TEXTURE_SIZE, NOISE_TEXTURE_SIZE, 4, false))
	{
		return false;
	}

	unsigned char* pixels = noiseImage.bmp_w();
	for (int i = 0; i < NOISE_TEXTURE_SIZE * NOISE_TEXTURE_SIZE; ++i)
	{
		pixels[i * 4 + 0] = static_cast<unsigned char>(rand() % 256);
		pixels[i * 4 + 1] = static_cast<unsigned char>(rand() % 256);
		pixels[i * 4 + 2] = static_cast<unsigned char>(rand() % 256);
		pixels[i * 4 + 3] = 255;
	}

	s_noiseTexture = gfx_api::context::get().createTextureForCompatibleImageUploads(1, noiseImage, "ssao_noise");
	if (s_noiseTexture == nullptr)
	{
		return false;
	}

	return s_noiseTexture->upload(0, noiseImage);
}

} // namespace

bool isEnabled()
{
	return uses_gfx_debug;
}

void init()
{
	initKernel();
	if (!initNoiseTexture())
	{
		debug(LOG_ERROR, "Failed to initialize SSAO noise texture");
	}
}

void shutdown()
{
	delete s_noiseTexture;
	s_noiseTexture = nullptr;
}

gfx_api::PassHandle addDepthPrePass(
	gfx_api::RenderGraph& graph,
	const SceneMatrices& matrices,
	const glm::vec3& cameraPos,
	const ShadowCascadesInfo& shadowInfo,
	uint64_t currentGameFrame)
{
	if (!isEnabled() || s_noiseTexture == nullptr)
	{
		return gfx_api::kInvalidPassHandle;
	}

	return graph.addRenderPass(
		gfx_api::makeDepthPrePass("DepthPrePass",
			[matrices, cameraPos, shadowInfo, currentGameFrame](const gfx_api::RenderPassContext&)
	{
		drawTerrainDepthForSSAO(matrices.perspectiveViewMatrix);
		pie_DrawAllMeshes(
			currentGameFrame,
			matrices.perspectiveMatrix,
			matrices.viewMatrix,
			cameraPos,
			shadowInfo,
			MeshDepthPassMode::SSAO);
	}));
}

PassHandles addPostScenePasses(
	gfx_api::RenderGraph& graph,
	gfx_api::PassHandle depthPrePass,
	std::pair<uint32_t, uint32_t> sceneDims,
	const glm::mat4& projectionMatrix,
	const glm::mat4& invProjectionMatrix,
	gfx_api::buffer* fullscreenTriVBO)
{
	PassHandles handles;
	if (!isEnabled() || depthPrePass == gfx_api::kInvalidPassHandle || fullscreenTriVBO == nullptr)
	{
		return handles;
	}

	const uint32_t sceneWidth = sceneDims.first;
	const uint32_t sceneHeight = sceneDims.second;
	const glm::vec2 blurStepH(1.0f / static_cast<float>(sceneWidth), 0.0f);
	const glm::vec2 blurStepV(0.0f, 1.0f / static_cast<float>(sceneHeight));

	handles.rawPass = graph.addRenderPass(
		std::move(
			gfx_api::RenderPassBuilder::create("SSAOGenerate")
				.viewport(sceneWidth, sceneHeight)
				.transientColorAttachment()
				.readPassOutput(depthPrePass, gfx_api::AttachmentRole::Depth)
				.record([projectionMatrix, invProjectionMatrix, sceneDims, fullscreenTriVBO](const gfx_api::RenderPassContext& ctx)
	{
		drawSSAOGenerate(ctx.getRead(0), projectionMatrix, invProjectionMatrix, sceneDims, fullscreenTriVBO);
	}))
			.build());

	handles.blurHPass = graph.addRenderPass(
		std::move(
			gfx_api::RenderPassBuilder::create("SSAOBlurH")
				.viewport(sceneWidth, sceneHeight)
				.transientColorAttachment()
				.readPassOutput(handles.rawPass, gfx_api::AttachmentRole::PrimaryColor)
				.readPassOutput(depthPrePass, gfx_api::AttachmentRole::Depth)
				.record([blurStepH, fullscreenTriVBO](const gfx_api::RenderPassContext& ctx)
	{
		drawSSAOBlur(ctx.getRead(0), ctx.getRead(1), blurStepH, fullscreenTriVBO);
	}))
			.build());

	handles.blurVPass = graph.addRenderPass(
		std::move(
			gfx_api::RenderPassBuilder::create("SSAOBlurV")
				.viewport(sceneWidth, sceneHeight)
				.transientColorAttachment()
				.readPassOutput(handles.blurHPass, gfx_api::AttachmentRole::PrimaryColor)
				.readPassOutput(depthPrePass, gfx_api::AttachmentRole::Depth)
				.record([blurStepV, fullscreenTriVBO](const gfx_api::RenderPassContext& ctx)
	{
		drawSSAOBlur(ctx.getRead(0), ctx.getRead(1), blurStepV, fullscreenTriVBO);
	}))
			.build());

	return handles;
}

void addComposePass(
	gfx_api::RenderGraph& graph,
	gfx_api::PassHandle scenePass,
	const PassHandles& ssaoPasses,
	bool backdropEnabled,
	gfx_api::buffer* fullscreenTriVBO)
{
	const bool useSsao = isEnabled()
		&& ssaoPasses.blurVPass != gfx_api::kInvalidPassHandle
		&& scenePass != gfx_api::kInvalidPassHandle;

	if (useSsao)
	{
		graph.addRenderPass(
			std::move(
				gfx_api::RenderPassBuilder::create("SceneComposeSSAO")
					.swapchainAttachment(backdropEnabled
						? gfx_api::AttachmentLoadOp::Load
						: gfx_api::AttachmentLoadOp::Clear)
					.readPassOutput(scenePass, gfx_api::AttachmentRole::PrimaryColor)
					.readPassOutput(ssaoPasses.blurVPass, gfx_api::AttachmentRole::PrimaryColor)
					.record([fullscreenTriVBO](const gfx_api::RenderPassContext& ctx)
		{
			drawSceneCompose(ctx.getRead(0), ctx.getRead(1), fullscreenTriVBO);
		}))
				.build());
		return;
	}

	graph.addRenderPass(
		std::move(
			gfx_api::RenderPassBuilder::create("SceneBlit")
				.swapchainAttachment(backdropEnabled
					? gfx_api::AttachmentLoadOp::Load
					: gfx_api::AttachmentLoadOp::Clear)
				.readPassOutput(scenePass, gfx_api::AttachmentRole::PrimaryColor)
				.record([fullscreenTriVBO](const gfx_api::RenderPassContext& ctx)
	{
		drawSceneBlit(ctx.getRead(0), fullscreenTriVBO);
	}))
			.build());
}

} // namespace ssao
