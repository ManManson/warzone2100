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

#include "run_control.h"

#include "lib/framework/frame.h"
#include "lib/framework/wzapp.h"
#include "lib/gamelib/gtime.h"

#include <cstdlib>

namespace
{
RunControl g_runControl;
bool g_quitting = false;
}

const RunControl &runControl()
{
	return g_runControl;
}

RunControl &runControlMutable()
{
	return g_runControl;
}

void runControlMaybeQuit()
{
	// wzQuit is not immediate, and this runs every tick until shutdown. The deadline stays
	// on the struct; the guard is local so a later reader still sees the configured value.
	if (g_quitting)
	{
		return;
	}
	const uint32_t deadline = g_runControl.quitAtGameTime;
	if (deadline != 0 && gameTime >= deadline)
	{
		g_quitting = true;
		debug(LOG_INFO, "Reached --quit-at-gametime=%u; quitting", deadline);
		wzQuit(EXIT_SUCCESS);
	}
}
