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

#include "../replay_session.h"

#include "lib/framework/frame.h"
#include "lib/framework/i18n.h"
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
	overlay->backgroundColor = pal_RGBA(0, 0, 0, 160);
	overlay->setTransparentToMouse(true);
	overlay->swallowsInput = false;
	overlay->onCancelPressed = []() {
		ReplaySession::instance().cancelBusy();
	};
	return overlay;
}

void ReplayBusyOverlay::display(int xOffset, int yOffset)
{
	if (!ReplaySession::instance().isBusy())
	{
		return;
	}

	W_FULLSCREENOVERLAY_CLICKFORM::display(xOffset, yOffset);

	const char *text = _("Loading...");
	const int textW = iV_GetTextWidth(text, font_large);
	const int textH = iV_GetTextLineSize(font_large);
	iV_SetTextColour(WZCOL_TEXT_BRIGHT);
	iV_DrawText(text,
		xOffset + (width() - textW) / 2,
		yOffset + (height() + textH) / 2,
		font_large);
}

void ReplayBusyOverlay::run(W_CONTEXT *psContext)
{
	const bool busy = ReplaySession::instance().isBusy();
	setTransparentToMouse(!busy);
	swallowsInput = busy;
	if (!busy)
	{
		return;
	}
	W_FULLSCREENOVERLAY_CLICKFORM::run(psContext);
}
