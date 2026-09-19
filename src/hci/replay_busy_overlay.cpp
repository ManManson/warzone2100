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

#include "replay_busy_overlay.h"
#include "replay_controller.h"

#include "../replay_session.h"
#include "../sim_clock.h"

#include "lib/framework/frame.h"
#include "lib/framework/i18n.h"
#include "lib/framework/string_ext.h"
#include "lib/gamelib/gtime.h"
#include "lib/ivis_opengl/pieblitfunc.h"
#include "lib/ivis_opengl/piepalette.h"
#include "lib/ivis_opengl/textdraw.h"

ReplayBusyOverlay::ReplayBusyOverlay()
	: W_FULLSCREENOVERLAY_CLICKFORM()
{
}

std::shared_ptr<ReplayBusyOverlay> ReplayBusyOverlay::make()
{
	class make_shared_enabler : public ReplayBusyOverlay
	{
	};

	auto overlay = std::make_shared<make_shared_enabler>();
	overlay->setCalcLayout(LAMBDA_CALCLAYOUT_SIMPLE({
		psWidget->setGeometry(0, 0, screenWidth, screenHeight);
	}));
	overlay->backgroundColor = pal_RGBA(0, 0, 0, 0);
	overlay->setTransparentToMouse(true);
	overlay->swallowsInput = false;
	overlay->onCancelPressed = []() {
		ReplaySession::instance().cancelBusy();
	};
	return overlay;
}

void ReplayBusyOverlay::display(int xOffset, int yOffset)
{
	if (!SimClock::instance().isCatchingUp())
	{
		return;
	}

	if (!pie_BlitSwapchainFreeze())
	{
		pie_UniTransBoxFill(xOffset, yOffset, xOffset + width(), yOffset + height(), pal_RGBA(0, 0, 0, 255));
	}
	pie_UniTransBoxFill(xOffset, yOffset, xOffset + width(), yOffset + height(), pal_RGBA(0, 0, 0, 120));

	const char *base = _("Loading");
	static const char *suffix[] = { "", ".", "..", "..." };
	const unsigned phase = (realTime / GAME_TICKS_PER_SEC) % 4;
	char text[256];
	ssprintf(text, "%s%s", base, suffix[phase]);

	char measure[256];
	ssprintf(measure, "%s%s", base, suffix[3]);
	const int titleW = iV_GetTextWidth(measure, font_large);
	const int titleH = iV_GetTextLineSize(font_large);

	const char *hint = _("(Press ESC to cancel)");
	const int hintW = iV_GetTextWidth(hint, font_regular);
	const int hintH = iV_GetTextLineSize(font_regular);
	const int gap = 8;
	const int blockH = titleH + gap + hintH;
	const int titleBaseline = yOffset + (height() - blockH) / 2 + titleH;

	iV_SetTextColour(WZCOL_TEXT_BRIGHT);
	iV_DrawText(text, xOffset + (width() - titleW) / 2, titleBaseline, font_large);
	iV_SetTextColour(WZCOL_TEXT_MEDIUM);
	iV_DrawText(hint, xOffset + (width() - hintW) / 2, titleBaseline + gap + hintH, font_regular);
}

void ReplayBusyOverlay::run(W_CONTEXT *psContext)
{
	syncReplayTransportBarVisibility();
	const bool busy = ReplaySession::instance().isBusy();
	setTransparentToMouse(!busy);
	swallowsInput = busy;
	if (!busy)
	{
		return;
	}
	W_FULLSCREENOVERLAY_CLICKFORM::run(psContext);
}
