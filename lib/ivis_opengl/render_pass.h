#pragma once

#include "lib/ivis_opengl/gfx_api_formats_def.h"

#include <array>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <assert.h>
#include <stdint.h>

namespace gfx_api
{

struct attachment_load_op
{
	enum class type
	{
		load,
		clear,
		dont_care
	};

	struct color_clear_value
	{
		float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	};

	struct depth_stencil_clear_value
	{
		float depth = 1.0f;
		uint32_t stencil = 0;
	};
	using clear_value_t = std::variant<color_clear_value, depth_stencil_clear_value>;

	type op = type::clear;
	clear_value_t clearValue = color_clear_value{};
};

struct attachment_store_op
{
	enum class type
	{
		store,
		dont_care
	};
	type op = type::store;
};

struct abstract_texture;

enum class msaa_sample_count_bits
{
	sample_count_1   = 0x1,
	sample_count_2   = 0x2,
	sample_count_4   = 0x4,
	sample_count_8   = 0x8,
	sample_count_16  = 0x10,
	sample_count_32  = 0x20,
	sample_count_64  = 0x40,
};

struct attachment_description
{
	pixel_format;
	attachment_load_op loadOp;
	attachment_store_op storeOp;
	msaa_sample_count_bits samples = msaa_sample_count_bits::sample_count_1;
	abstract_texture* texture = nullptr;
};

struct render_pass_description
{
	std::vector<attachment_description> colorAttachments;
	std::optional<attachment_description> depthStencilAttachment;
	std::array<uint32_t, 2> dimensions = { 0, 0 };
};

class render_pass_impl_base
{
public:

	virtual ~render_pass_impl_base() = default;
	virtual size_t identifier() const = 0;
	virtual bool valid() const = 0;
};

// Assumed to be subclassed to provide render_pass_impl_base implementation
class render_pass
{
public:

	explicit render_pass(const render_pass_description& rpdesc)
		: rpdesc_(rpdesc)
	{}
	virtual ~render_pass() = default;

	const render_pass_description& description() const { return rpdesc_; }
	size_t identifier() const
	{
		assert(implPass_ != nullptr);
		return implPass_->identifier();
	}
	bool valid() const
	{
		return implPass_ != nullptr && implPass_->valid();
	}

protected:

	render_pass_description rpdesc_;
	std::unique_ptr<render_pass_impl_base> implPass_;
};

} // namespace gfx_api
