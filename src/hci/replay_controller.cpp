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

#include "replay_controller.h"
#include "replay_busy_overlay.h"
#include "replay_timeline.h"

#include "../hci.h"
#include "../notifications.h"
#include "../replay_command.h"
#include "../replay_session.h"
#include "../sim_clock.h"

#include "lib/framework/frame.h"
#include "lib/gamelib/gtime.h"
#include "lib/ivis_opengl/pieblitfunc.h"
#include "lib/ivis_opengl/piepalette.h"
#include "lib/ivis_opengl/textdraw.h"
#include "lib/widget/button.h"
#include "lib/widget/form.h"
#include "lib/widget/label.h"
#include "lib/widget/widgbase.h"
#include "lib/widget/widget.h"

#include <algorithm>
#include <memory>

constexpr int REPLAY_TRANSPORT_PADDING = 8;
constexpr int REPLAY_ACTION_BUTTONS_PADDING = 5;
constexpr int REPLAY_ACTION_BUTTONS_SPACING = 5;
constexpr int REPLAY_ACTION_BUTTONS_IMAGE_SIZE = 16;
constexpr int REPLAY_ACTION_BUTTONS_WIDTH = REPLAY_ACTION_BUTTONS_IMAGE_SIZE + (REPLAY_ACTION_BUTTONS_PADDING * 2);
constexpr int REPLAY_ACTION_BUTTONS_HEIGHT = REPLAY_ACTION_BUTTONS_WIDTH;
constexpr int REPLAY_TIMELINE_SPACING = 6;
constexpr float REPLAY_TRANSPORT_CONTENT_FRAC = 0.80f;

class W_REPLAY_CONTROL_BUTTON : public W_BUTTON
{
public:
	W_REPLAY_CONTROL_BUTTON() : W_BUTTON() {}

	~W_REPLAY_CONTROL_BUTTON()
	{
		delete pButtonImage;
		pButtonImage = nullptr;
	}

	void display(int xOffset, int yOffset) override
	{
		const int x0 = x() + xOffset;
		const int y0 = y() + yOffset;
		const int x1 = x0 + width();
		const int y1 = y0 + height();

		const bool isDown = (getState() & (WBUT_DOWN | WBUT_LOCK | WBUT_CLICKLOCK)) != 0;
		const bool isDisabled = (getState() & WBUT_DISABLE) != 0;
		const bool isHighlight = !isDisabled && ((getState() & WBUT_HIGHLIGHT) != 0);

		const auto light_border = pal_RGBA(255, 255, 255, 100);
		const auto fill_color = isDown || isDisabled ? pal_RGBA(10, 0, 70, 200) : pal_RGBA(25, 0, 110, 175);
		iV_ShadowBox(x0, y0, x1, y1, 0, isDown ? pal_RGBA(0, 0, 0, 0) : light_border, isDisabled ? light_border : WZCOL_FORM_DARK, fill_color);

		if (isHighlight)
		{
			iV_Box(x0 + 2, y0 + 2, x1 - 2, y1 - 2, WZCOL_FORM_HILITE);
		}

		switch (outline)
		{
			case OutlineState::None:
				break;
			case OutlineState::Double:
				iV_Box(x0 + 3, y0 + 3, x1 - 3, y1 - 3, pal_RGBA(218, 207, 255, 200));
				// fall-through
			case OutlineState::Single:
				iV_Box(x0 + 1, y0 + 1, x1 - 1, y1 - 1, pal_RGBA(218, 207, 255, 200));
				break;
		}

		if (pButtonImage)
		{
			const int imageLeft = x1 - REPLAY_ACTION_BUTTONS_PADDING - REPLAY_ACTION_BUTTONS_IMAGE_SIZE;
			const int imageTop = y() + yOffset + REPLAY_ACTION_BUTTONS_PADDING;
			iV_DrawImageAnisotropic(*pButtonImage, Vector2i(imageLeft, imageTop), Vector2f(0, 0),
				Vector2f(REPLAY_ACTION_BUTTONS_IMAGE_SIZE, REPLAY_ACTION_BUTTONS_IMAGE_SIZE), 0.f, WZCOL_WHITE);
		}
	}

	gfx_api::texture* pButtonImage = nullptr;
	enum class OutlineState
	{
		None,
		Single,
		Double
	};
	OutlineState outline = OutlineState::None;
};

class ReplayTransportBar : public W_FORM
{
public:
	static std::shared_ptr<ReplayTransportBar> make();

	void display(int xOffset, int yOffset) override;
	void geometryChanged() override;
	void run(W_CONTEXT* psContext) override;
	void syncBusyControls();

private:
	std::shared_ptr<W_REPLAY_CONTROL_BUTTON> makeReplayActionButton(const char* imagePath);
	void refreshButtonStates();
	void layoutContent();

	std::shared_ptr<ReplayTimelineWidget> timeline;
	std::shared_ptr<W_REPLAY_CONTROL_BUTTON> startButton;
	std::shared_ptr<W_REPLAY_CONTROL_BUTTON> skipBackButton;
	std::shared_ptr<W_REPLAY_CONTROL_BUTTON> pauseButton;
	std::shared_ptr<W_REPLAY_CONTROL_BUTTON> playButton;
	std::shared_ptr<W_REPLAY_CONTROL_BUTTON> skipForwardButton;
	std::shared_ptr<W_REPLAY_CONTROL_BUTTON> fastForwardButton;
	std::shared_ptr<W_LABEL> timeLabel;
	UDWORD lastUpdateTime = 0;
};

std::shared_ptr<W_REPLAY_CONTROL_BUTTON> ReplayTransportBar::makeReplayActionButton(const char* imagePath)
{
	auto button = std::make_shared<W_REPLAY_CONTROL_BUTTON>();
	button->style |= WBUT_SECONDARY;
	button->FontID = font_regular;
	button->pButtonImage = WZ_Notification_Image(imagePath).loadImageToTexture();
	button->setGeometry(0, 0, REPLAY_ACTION_BUTTONS_WIDTH, REPLAY_ACTION_BUTTONS_HEIGHT);
	return button;
}

std::shared_ptr<ReplayTransportBar> ReplayTransportBar::make()
{
	auto result = std::make_shared<ReplayTransportBar>();

	result->timeline = ReplayTimelineWidget::make();
	result->attach(result->timeline);

	result->startButton = result->makeReplayActionButton("images/replay/skip_start.png");
	result->startButton->setTip(_("Rewind to start"));
	result->attach(result->startButton);
	result->startButton->addOnClickHandler([](W_BUTTON&) {
		ReplaySession::instance().dispatch(CmdSeekToStart{});
	});

	result->skipBackButton = result->makeReplayActionButton("images/replay/skip_backward.png");
	result->skipBackButton->setTip(_("Back 15 seconds"));
	result->attach(result->skipBackButton);
	result->skipBackButton->addOnClickHandler([](W_BUTTON&) {
		ReplaySession::instance().dispatch(CmdSeekRelative{-static_cast<int32_t>(REPLAY_SKIP_STEP_MS)});
	});

	result->pauseButton = result->makeReplayActionButton("images/replay/pause.png");
	result->pauseButton->setTip(_("Pause"));
	result->attach(result->pauseButton);
	result->pauseButton->addOnClickHandler([](W_BUTTON&) {
		ReplaySession::instance().dispatch(CmdPause{});
	});

	result->playButton = result->makeReplayActionButton("images/replay/play.png");
	result->playButton->setTip(_("Resume"));
	result->attach(result->playButton);
	result->playButton->addOnClickHandler([](W_BUTTON&) {
		ReplaySession::instance().dispatch(CmdPlay{});
	});

	result->skipForwardButton = result->makeReplayActionButton("images/replay/skip_forward.png");
	result->skipForwardButton->setTip(_("Forward 15 seconds"));
	result->attach(result->skipForwardButton);
	result->skipForwardButton->addOnClickHandler([](W_BUTTON&) {
		ReplaySession::instance().dispatch(CmdSeekRelative{+static_cast<int32_t>(REPLAY_SKIP_STEP_MS)});
	});

	result->fastForwardButton = result->makeReplayActionButton("images/replay/fast_forward.png");
	result->fastForwardButton->setTip(_("Fast-Forward"));
	result->attach(result->fastForwardButton);
	result->fastForwardButton->addOnClickHandler([](W_BUTTON& button) {
		ReplaySession::instance().dispatch(CmdCycleRate{button.getOnClickButtonPressed() == WKEY_SECONDARY});
	});

	result->timeLabel = std::make_shared<W_LABEL>();
	result->timeLabel->setFont(font_regular, WZCOL_FORM_TEXT);
	result->timeLabel->setTextAlignment(WLAB_ALIGNRIGHT);
	result->timeLabel->setCacheNeverExpires(true);
	result->attach(result->timeLabel);

	const int stripHeight = REPLAY_TRANSPORT_PADDING + REPLAY_PLAYHEAD_SIZE + REPLAY_TIMELINE_SPACING
		+ REPLAY_ACTION_BUTTONS_HEIGHT + REPLAY_TRANSPORT_PADDING;
	result->setGeometry(0, 0, screenWidth, stripHeight);
	result->refreshButtonStates();
	result->layoutContent();

	return result;
}

void ReplayTransportBar::display(int xOffset, int yOffset)
{
	const int x0 = x() + xOffset;
	const int y0 = y() + yOffset;
	pie_UniTransBoxFill(x0, y0, x0 + width(), y0 + height(), pal_RGBA(0, 0, 0, 160));
}

void ReplayTransportBar::geometryChanged()
{
	layoutContent();
}

void ReplayTransportBar::run(W_CONTEXT*)
{
	syncBusyControls();
	if (realTime - lastUpdateTime >= GAME_TICKS_PER_UPDATE)
	{
		refreshButtonStates();
	}
}

void ReplayTransportBar::syncBusyControls()
{
	// Hide the live bar only while CatchUp is presenting. The pending-seek Full
	// frame must still draw it (and other overlays) so the freeze snapshot
	// includes the timeline and player-stats widgets.
	if (SimClock::instance().isCatchingUp())
	{
		hide();
	}
	else
	{
		show();
		timeline->setTransparentToMouse(false);
		setTransparentToMouse(false);
	}
}

void ReplayTransportBar::refreshButtonStates()
{
	auto& session = ReplaySession::instance();
	if (session.isPaused())
	{
		playButton->setState(0);
		pauseButton->setState(WBUT_DISABLE);
	}
	else
	{
		playButton->setState(WBUT_DISABLE);
		pauseButton->setState(0);
	}

	const auto maxFastForwardTicks = session.fastForwardTicks();
	if (maxFastForwardTicks > 0)
	{
		fastForwardButton->outline = (maxFastForwardTicks >= WZ_MAX_REPLAY_FASTFORWARD_TICKS)
			? W_REPLAY_CONTROL_BUTTON::OutlineState::Double
			: W_REPLAY_CONTROL_BUTTON::OutlineState::Single;
	}
	else
	{
		fastForwardButton->outline = W_REPLAY_CONTROL_BUTTON::OutlineState::None;
	}

	const uint32_t playhead = session.playheadMs();
	const uint32_t duration = session.timelineDurationMs();
	const bool atStart = playhead < REPLAY_UI_QUANTUM_MS;
	startButton->setState(atStart ? WBUT_DISABLE : 0);
	skipBackButton->setState(atStart ? WBUT_DISABLE : 0);
	skipForwardButton->setState((playhead + REPLAY_SKIP_STEP_MS >= duration) ? WBUT_DISABLE : 0);

	timeLabel->setString(WzString::fromUtf8(formatReplayClockLabel(playhead, duration)));
	lastUpdateTime = realTime;
	layoutContent();
}

void ReplayTransportBar::layoutContent()
{
	const int contentW = std::max(1, static_cast<int>(static_cast<float>(width()) * REPLAY_TRANSPORT_CONTENT_FRAC));
	const int contentX = (width() - contentW) / 2;
	timeline->setGeometry(contentX, REPLAY_TRANSPORT_PADDING, contentW, REPLAY_PLAYHEAD_SIZE);
	const int buttonsY = REPLAY_TRANSPORT_PADDING + REPLAY_PLAYHEAD_SIZE + REPLAY_TIMELINE_SPACING;

	auto placeAfter = [buttonsY](const std::shared_ptr<W_REPLAY_CONTROL_BUTTON>& button,
		const std::shared_ptr<W_REPLAY_CONTROL_BUTTON>& previous, int fallbackX) {
		const int x = previous ? previous->x() + previous->width() + REPLAY_ACTION_BUTTONS_SPACING : fallbackX;
		button->setGeometry(x, buttonsY, button->width(), button->height());
	};
	startButton->setGeometry(contentX, buttonsY, startButton->width(), startButton->height());
	placeAfter(skipBackButton, startButton, contentX);
	placeAfter(pauseButton, skipBackButton, contentX);
	placeAfter(playButton, pauseButton, contentX);
	placeAfter(skipForwardButton, playButton, contentX);
	placeAfter(fastForwardButton, skipForwardButton, contentX);

	const int labelHeight = iV_GetTextLineSize(font_regular);
	const int labelY = buttonsY + (REPLAY_ACTION_BUTTONS_HEIGHT - labelHeight) / 2;
	const int labelX = contentX;
	timeLabel->setString(WzString::fromUtf8(formatReplayClockLabel(ReplaySession::instance().playheadMs(), ReplaySession::instance().timelineDurationMs())));
	const int labelWidth = std::max(timeLabel->getMaxLineWidth(), 1);
	timeLabel->setGeometry(labelX + contentW - labelWidth, labelY, labelWidth, labelHeight);
}

static std::shared_ptr<W_SCREEN> replayOverlayScreen;
static std::shared_ptr<ReplayTransportBar> replayTransportBar;
static std::shared_ptr<ReplayBusyOverlay> replayBusyOverlay;
static int replayTransportStripHeight = 0;

void syncReplayTransportBarVisibility()
{
	if (!replayTransportBar)
	{
		return;
	}
	replayTransportBar->syncBusyControls();
}

void displayReplayCatchUpOverlay()
{
	if (!replayBusyOverlay)
	{
		return;
	}
	replayBusyOverlay->displayRecursive();
}

bool createReplayControllerOverlay()
{
	ASSERT(psWScreen != nullptr, "psWScreen is not initialized yet?");
	ASSERT_OR_RETURN(false, replayOverlayScreen == nullptr, "Already initialized");

	replayOverlayScreen = W_SCREEN::make();
	replayOverlayScreen->psForm->hide();

	replayTransportBar = ReplayTransportBar::make();
	replayOverlayScreen->psForm->attach(replayTransportBar);
	replayTransportStripHeight = replayTransportBar->height();
	replayTransportBar->setCalcLayout([](WIDGET* psWidget) {
		psWidget->setGeometry(0, screenHeight - psWidget->height(), screenWidth, psWidget->height());
	});

	replayBusyOverlay = ReplayBusyOverlay::make();
	replayOverlayScreen->psForm->attach(replayBusyOverlay);

	widgRegisterOverlayScreenOnTopOfScreen(replayOverlayScreen, psWScreen);
	return true;
}

void destroyReplayControllerOverlay()
{
	if (replayOverlayScreen)
	{
		widgRemoveOverlayScreen(replayOverlayScreen);
		replayOverlayScreen = nullptr;
	}
	replayTransportBar.reset();
	replayBusyOverlay.reset();
	replayTransportStripHeight = 0;
}

void replayOverlayScreenSizeDidChange()
{
	if (replayOverlayScreen)
	{
		replayOverlayScreen->screenSizeDidChange(screenWidth, screenHeight, screenWidth, screenHeight);
	}
}

int replayTransportReservedBottom()
{
	return replayOverlayScreen ? replayTransportStripHeight : 0;
}
