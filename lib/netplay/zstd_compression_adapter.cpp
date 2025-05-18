// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2025  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "zstd_compression_adapter.h"

#include "lib/framework/frame.h"

#include "lib/netplay/error_categories.h"

#include <cstring>

ZstdCompressionAdapter::ZstdCompressionAdapter()
{
	std::memset(&compressInBuf_, 0, sizeof(compressInBuf_));
	std::memset(&compressOutBuf_, 0, sizeof(compressOutBuf_));

	std::memset(&decompressInBuf_, 0, sizeof(decompressInBuf_));
	std::memset(&decompressOutBuf_, 0, sizeof(decompressOutBuf_));
}

ZstdCompressionAdapter::~ZstdCompressionAdapter()
{
	if (compressCtx_)
	{
		ZSTD_freeCCtx(compressCtx_);
	}
	if (decompressCtx_)
	{
		ZSTD_freeDCtx(decompressCtx_);
	}
}

net::result<void> ZstdCompressionAdapter::initialize()
{
	compressCtx_ = ZSTD_createCCtx();
	if (!compressCtx_)
	{
		debug(LOG_ERROR, "Failed to create ZSTD compression context");
		return tl::make_unexpected(make_network_error_code(EBADF));
	}
	decompressCtx_ = ZSTD_createDCtx();
	if (!decompressCtx_)
	{
		ZSTD_freeCCtx(compressCtx_);
		compressCtx_ = nullptr;
		debug(LOG_ERROR, "Failed to create ZSTD decompression context");
		return tl::make_unexpected(make_network_error_code(EBADF));
	}
	return {};
}

void ZstdCompressionAdapter::resetCompressionStreamInput(const void* src, size_t size)
{
	compressInBuf_.src = src;
	compressInBuf_.size = size;
	compressInBuf_.pos = 0;
}

void ZstdCompressionAdapter::resetDecompressionStreamOutput(void* dst, size_t size)
{
	decompressOutBuf_.dst = dst;
	decompressOutBuf_.size = size;
	decompressOutBuf_.pos = 0;
}

net::result<void> ZstdCompressionAdapter::compress(const void* src, size_t size)
{
	resetCompressionStreamInput(src, size);

	// FIXME: Should call as many times as needed to ensure that all input has been consumed.
	auto res = ZSTD_compressStream2(compressCtx_, &compressOutBuf_, &compressInBuf_, ZSTD_e_continue);
	if (ZSTD_isError(res))
	{
		debug(LOG_ERROR, "ZSTD_compressStream2 failed: %s", ZSTD_getErrorName(res));
		return tl::make_unexpected(make_zstd_error_code(res));
	}
}

net::result<void> ZstdCompressionAdapter::flushCompressionStream()
{
	do
	{
		auto res = ZSTD_compressStream2(compressCtx_, &compressOutBuf_, &compressInBuf_, ZSTD_e_flush);
		if (ZSTD_isError(res))
		{
			debug(LOG_ERROR, "ZSTD_compressStream2 flush failed: %s", ZSTD_getErrorName(res));
			return tl::make_unexpected(make_zstd_error_code(res));
		}
	} while (res != 0);

	return {};
}

net::result<void> ZstdCompressionAdapter::decompress(const void* src, size_t size)
{

	auto res = ZSTD_decompressStream(decompressCtx_, &decompressOutBuf_, &decompressInBuf_);
	if (ZSTD_isError(res))
	{
		debug(LOG_ERROR, "ZSTD_decompressStream failed: %s", ZSTD_getErrorName(res));
		return tl::make_unexpected(make_zstd_error_code(res));
	}
	return {};
}
