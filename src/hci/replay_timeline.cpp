/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project

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

#include "replay_timeline.h"

#include "../replay_command.h"
#include "../replay_session.h"

#include "lib/framework/input.h"
#include "lib/ivis_opengl/pieblitfunc.h"
#include "lib/ivis_opengl/piepalette.h"

#include <algorithm>

std::shared_ptr<ReplayTimelineWidget> ReplayTimelineWidget::make()
{
	return std::make_shared<ReplayTimelineWidget>();
}

uint32_t ReplayTimelineWidget::xToMs(int localX) const
{
	const uint32_t duration = ReplaySession::instance().timelineDurationMs();
	if (duration == 0 || width() <= 0)
	{
		return 0;
	}
	const int clamped = std::clamp(localX, 0, width());
	return static_cast<uint32_t>((static_cast<uint64_t>(clamped) * duration) / static_cast<uint32_t>(width()));
}

uint32_t ReplayTimelineWidget::seekMsFromContext(W_CONTEXT *psContext) const
{
	const uint32_t duration = ReplaySession::instance().timelineDurationMs();
	return quantizeReplaySeekMs(xToMs(psContext->mx - x()), duration);
}

void ReplayTimelineWidget::updateGhostFromContext(W_CONTEXT *psContext)
{
	ghostMs_ = seekMsFromContext(psContext);
}

void ReplayTimelineWidget::clicked(W_CONTEXT *psContext, WIDGET_KEY)
{
	dragging_ = true;
	updateGhostFromContext(psContext);
}

void ReplayTimelineWidget::mouseDragged(WIDGET_KEY, W_CONTEXT *, W_CONTEXT *current)
{
	if (dragging_)
	{
		updateGhostFromContext(current);
	}
}

void ReplayTimelineWidget::released(W_CONTEXT *psContext, WIDGET_KEY)
{
	if (dragging_)
	{
		updateGhostFromContext(psContext);
		ReplaySession::instance().dispatch(CmdSeekAbsolute{*ghostMs_});
	}
	dragging_ = false;
	ghostMs_.reset();
}

std::string ReplayTimelineWidget::getTip()
{
	auto& session = ReplaySession::instance();
	const uint32_t duration = session.timelineDurationMs();
	if (duration == 0)
	{
		return {};
	}
	const uint32_t ms = dragging_ && ghostMs_.has_value()
		? *ghostMs_
		: quantizeReplaySeekMs(xToMs(mouseX() - screenPosX()), duration);
	const bool useHours = duration >= 3600u * GAME_TICKS_PER_SEC;
	return formatReplayTime(ms, useHours);
}

void ReplayTimelineWidget::display(int xOffset, int yOffset)
{
	const int x0 = x() + xOffset;
	const int y0 = y() + yOffset;
	const int x1 = x0 + width();
	const int y1 = y0 + height();
	pie_UniTransBoxFill(x0, y0, x1, y1, pal_RGBA(255, 255, 255, 35));

	auto& session = ReplaySession::instance();
	const uint32_t duration = session.timelineDurationMs();
	if (duration == 0 || width() <= 0)
	{
		return;
	}

	const auto msToX = [this, x0, duration](uint32_t timeMs) {
		const uint32_t clamped = std::min(timeMs, duration);
		return x0 + static_cast<int>((static_cast<uint64_t>(clamped) * static_cast<uint32_t>(width())) / duration);
	};

	const int playheadX = msToX(session.playheadMs());
	pie_UniTransBoxFill(x0, y0, playheadX, y1, pal_RGBA(218, 207, 255, 160));

	if (ghostMs_.has_value())
	{
		const int ghostX = msToX(*ghostMs_);
		pie_UniTransBoxFill(ghostX - 1, y0, ghostX + 2, y1, pal_RGBA(255, 255, 255, 200));
	}

	pie_UniTransBoxFill(playheadX - 1, y0, playheadX + 2, y1, pal_RGBA(255, 255, 255, 230));
}
