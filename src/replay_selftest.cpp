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

#include "replay_selftest.h"

#include "replay_command.h"
#include "replay_session.h"

#include "lib/framework/wzapp.h"
#include "lib/gamelib/gtime.h"
#include "lib/netplay/sync_debug.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

namespace
{

enum class Phase
{
	Off,
	WaitT,
	RunA,
	Seeking,
	RunB,
	Done
};

/// Captures exactly `remaining` boundaries into `out`, then detaches itself.
class BucketCapture : public ISyncLogSink
{
public:
	std::map<uint32_t, std::string> out;
	int remaining = 0;

	void arm(int n)
	{
		out.clear();
		remaining = n;
		syncLogAddSink(this);
	}

	void onSyncBoundary(const SyncLogRecord &record) override
	{
		if (record.text.empty())
		{
			// dumpLocalDebugSyncLogByTime returns 0 when gameTime is not in the ring.
			// An empty record compared with an empty record would be a false pass.
			syncLogRemoveSink(this);
			fprintf(stderr, "[replay-selftest] FAIL: empty sync log at gameTime=%u\n", record.gameTime);
			wzQuit(EXIT_FAILURE);
			return;
		}
		out.emplace(record.gameTime, record.text);
		if (--remaining == 0)
		{
			syncLogRemoveSink(this);
		}
	}

	bool done() const { return remaining == 0; }
};

struct State : IReplaySessionObserver
{
	Phase phase = Phase::Off;
	uint32_t T = 0;
	uint32_t N = 0;
	uint32_t actualT = 0;
	bool checkedLanding = false;
	bool spedUp = false;
	BucketCapture a;
	BucketCapture b;

	void onKeyframeRestored(uint32_t keyframeTime) override;
	void onTickCompleted(uint32_t t) override;
};

State st;

void printFirstDifferingLine(const std::string &straight, const std::string &restored)
{
	size_t ia = 0;
	size_t ib = 0;
	while (ia < straight.size() && ib < restored.size())
	{
		const size_t ea = straight.find('\n', ia);
		const size_t eb = restored.find('\n', ib);
		const std::string la = straight.substr(ia, ea == std::string::npos ? std::string::npos : ea - ia);
		const std::string lb = restored.substr(ib, eb == std::string::npos ? std::string::npos : eb - ib);
		if (la != lb)
		{
			fprintf(stderr, "[replay-selftest]   straight: %s\n[replay-selftest]   restored: %s\n", la.c_str(), lb.c_str());
			return;
		}
		if (ea == std::string::npos || eb == std::string::npos)
		{
			break;
		}
		ia = ea + 1;
		ib = eb + 1;
	}
	fprintf(stderr, "[replay-selftest]   (texts differ only in length: %zu vs %zu)\n", straight.size(), restored.size());
}

void finish()
{
	st.phase = Phase::Done;
	// Bucket actualT+100 holds the pre-restore tick on the straight run only. Its CRC is
	// equalised by applyResumeSyncDebugCrc; its text is not. Compare from the next boundary.
	const uint32_t first = st.actualT + 2 * GAME_TICKS_PER_UPDATE;
	size_t compared = 0;
	for (const auto &kv : st.b.out)
	{
		if (kv.first < first)
		{
			continue;
		}
		const auto straight = st.a.out.find(kv.first);
		if (straight == st.a.out.end())
		{
			continue;
		}
		++compared;
		if (straight->second != kv.second)
		{
			fprintf(stderr, "[replay-selftest] FAIL: T=%u (state %u) N=%u first divergent bucket gameTime=%u\n",
			        st.T, st.actualT, st.N, kv.first);
			printFirstDifferingLine(straight->second, kv.second);
			wzQuit(EXIT_FAILURE);
			return;
		}
	}
	if (compared < st.N)
	{
		fprintf(stderr, "[replay-selftest] FAIL: only %zu of %u buckets could be compared\n", compared, st.N);
		wzQuit(EXIT_FAILURE);
		return;
	}
	fprintf(stderr, "[replay-selftest] OK: T=%u (state %u) %zu buckets identical from gameTime=%u\n",
	        st.T, st.actualT, compared, first);
	wzQuit(EXIT_SUCCESS);
}

void State::onKeyframeRestored(uint32_t keyframeTime)
{
	// commitPendingSeek runs after the tick loop, so this arrives between ticks. Capture B
	// must be armed here: the next tick's sync boundary happens before the next onTickCompleted.
	if (phase != Phase::Seeking)
	{
		return;
	}
	if (keyframeTime != T)
	{
		fprintf(stderr, "[replay-selftest] FAIL: restored keyframe %u, expected %u\n", keyframeTime, T);
		wzQuit(EXIT_FAILURE);
		return;
	}
	// First key is actualT+100 (excluded in finish); then N keys actualT+200 .. actualT+(N+1)*100.
	b.arm(static_cast<int>(N) + 1);
	phase = Phase::RunB;
}

void State::onTickCompleted(uint32_t t)
{
	if (!ReplaySession::active())
	{
		return;
	}
	switch (phase)
	{
	case Phase::WaitT:
		if (!spedUp)
		{
			// --loadreplay skips the autogame 500x modifier. Set it here so reaching T is not
			// wall-clock realtime. One tick has already run at 1x.
			gameTimeSetMod(Rational(500));
			spedUp = true;
		}
		if (t >= T)
		{
			// State time is t (T+2 normally). Overwrites the scheduler's plant of the same slot
			// with the same state.
			ReplaySession::instance().plantKeyframeNow(T);
			actualT = t;
			a.arm(static_cast<int>(N) + 1);
			phase = Phase::RunA;
		}
		break;
	case Phase::RunA:
		if (a.done())
		{
			phase = Phase::Seeking;
			ReplaySession::instance().dispatch(CmdSeekAbsolute{ T });
		}
		break;
	case Phase::RunB:
		if (!checkedLanding)
		{
			checkedLanding = true;
			if (t != actualT + GAME_TICKS_PER_UPDATE)
			{
				fprintf(stderr, "[replay-selftest] FAIL: first post-restore tick %u, expected %u\n",
				        t, actualT + GAME_TICKS_PER_UPDATE);
				wzQuit(EXIT_FAILURE);
				return;
			}
		}
		if (b.done())
		{
			finish();
		}
		break;
	default:
		break;
	}
}

} // namespace

void replaySelfTestConfigure(uint32_t T, uint32_t N)
{
	st.T = T;
	st.N = N;
	st.phase = Phase::WaitT;
	// Latency fields are in the snapshot, but sendPlayerGameTime folds wall-clock updateReadyTime
	// into wantedLatency unless this is set. After the restore the wall clock has moved, so the
	// next wlat sync line would differ on an otherwise exact restore.
	gameTimeSetDeterministicLatency(true);
	replaySessionAddObserver(&st);
}

bool replaySelfTestActive()
{
	return st.phase != Phase::Off && st.phase != Phase::Done;
}
