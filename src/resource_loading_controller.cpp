// SPDX-License-Identifier: GPL-2.0-or-later

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
/** \file resource_loading_controller.cpp
 * \brief Implementation of the cooperative loading controller.
 */

#include "resource_loading_controller.h"

#include "lib/framework/frameresource.h"
#include "lib/framework/wzapp.h"

#include "frontend.h"
#include "init.h"
#include "main.h"
#include "main_resource_loading.h"
#include "multiint.h"
#include "wrappers.h"

#include <utility>

using namespace main_resource_loading;

/// <summary>
/// Cooperative loading job: a small per-frame state machine.
/// `ResourceLoadingController::step()` calls `step()` at most once per main-loop
/// iteration while the job is active.
///
/// Each `step()` performs part of the work, advances an internal phase (or leaves
/// it unchanged), and returns:
/// * `InProgress`  — keep the job; controller will call `step()` again on a later frame.
/// * `Completed`   — controller calls `finalizeSuccess()` once, then destroys the job.
/// * `Failed`      — controller calls `finalizeFailure()` once, then destroys the job.
///
/// Do not assume multiple `step()` calls in the same frame. Heavy work should be
/// split across phases (or use a nested cooperative API like `resLoadPlanStep`)
/// so the UI can update between frames.
///
/// `frameProcessingMode()` tells `mainLoop` whether this frame ends after loading UI
/// only (`ConsumeFrame`) or may continue into the normal title/game loop (`ContinueMainLoop`).
/// </summary>
class IResourceLoadingJob
{
public:
	enum class StepResult
	{
		InProgress,
		Completed,
		Failed,
	};

	virtual ~IResourceLoadingJob() = default;
	virtual StepResult step() = 0;
	virtual void finalizeSuccess() = 0;
	virtual void finalizeFailure() = 0;
	virtual ResourceLoadingController::FrameProcessingMode frameProcessingMode() const
	{
		return ResourceLoadingController::FrameProcessingMode::ConsumeFrame;
	}
};

namespace
{

/// <summary>
/// Frontend boot job:
/// 1. Setup
/// 2. cooperative WRF load (`ResLoadPlan` + `resLoadPlanStep`)
///    * prepare load plan
///    * load resources
///    * complete load
/// 3. finalize initialization
/// </summary>
class FrontendInitJob final : public IResourceLoadingJob
{
public:
	explicit FrontendInitJob(ResourceLoadingRequest requestIn)
		: request(std::move(requestIn))
	{
	}

	StepResult step() override
	{
		switch (phase)
		{
		case Phase::Setup:
			SetGameMode(GS_TITLE_SCREEN);
			frontendIsShuttingDown();
			debug(LOG_WZ, "== Initializing frontend == : %s", request.resourceFile.c_str());
			if (!frontendInitialiseSetup())
			{
				return StepResult::Failed;
			}
			phase = Phase::PrepareResLoadPlan;
			return StepResult::InProgress;
		case Phase::PrepareResLoadPlan:
			debug(LOG_MAIN, "frontEndInitialise: loading resource file .....");
			if (!resPrepareLoadPlan(request.resourceFile.c_str(), 0, plan))
			{
				return StepResult::Failed;
			}
			phase = Phase::LoadResources;
			return StepResult::InProgress;
		case Phase::LoadResources:
			if (!resLoadPlanStep(plan, resGetLoadPlanEntriesPerStep()))
			{
				return StepResult::Failed;
			}
			if (!resLoadPlanComplete(plan))
			{
				return StepResult::InProgress;
			}
			phase = Phase::Finalize;
			return StepResult::InProgress;
		case Phase::Finalize:
			return frontendInitialiseFinalize() ? StepResult::Completed : StepResult::Failed;
		}

		return StepResult::Failed;
	}

	void finalizeSuccess() override
	{
		closeLoadingScreen();
	}

	void finalizeFailure() override
	{
		closeLoadingScreen();
		debug(LOG_FATAL, "Shutting down after failure");
		exit(EXIT_FAILURE);
	}

	ResourceLoadingController::FrameProcessingMode frameProcessingMode() const override
	{
		return ResourceLoadingController::FrameProcessingMode::ConsumeFrame;
	}

private:
	enum class Phase
	{
		Setup,
		PrepareResLoadPlan,
		LoadResources,
		Finalize,
	};

	ResourceLoadingRequest request;
	Phase phase = Phase::Setup;
	ResLoadPlan plan;
};

/// <summary>
/// Start game from lobby/menu job:
/// Currently uses hooks before/after a single blocking `levLoadData` path.
/// </summary>
class StartGameResourceJob final : public IResourceLoadingJob
{
public:
	StepResult step() override
	{
		switch (phase)
		{
		case Phase::PresentInitialFrame:
			phase = Phase::BeforeLevelLoad;
			return StepResult::InProgress;
		case Phase::BeforeLevelLoad:
			startGameBeforeLevelLoad();
			phase = Phase::LevelLoad;
			return StepResult::InProgress;
		case Phase::LevelLoad:
			if (!startGameRunLevelLoad())
			{
				return StepResult::Failed;
			}
			phase = Phase::AfterLevelLoad;
			return StepResult::InProgress;
		case Phase::AfterLevelLoad:
			return startGameAfterLevelLoad()
			           ? StepResult::Completed
			           : StepResult::Failed;
		}
		return StepResult::Failed;
	}

	void finalizeSuccess() override
	{
		closeLoadingScreen();
	}

	void finalizeFailure() override
	{
		startGameAbortLevelLoadFailure();
		closeLoadingScreen();
		debug(LOG_POPUP, _("Failed to load level data or map. Exiting to main menu."));
	}

private:
	enum class Phase
	{
		PresentInitialFrame,
		BeforeLevelLoad,
		LevelLoad,
		AfterLevelLoad,
	};

	Phase phase = Phase::PresentInitialFrame;
};

/// <summary>
/// Load a save game job:
/// Currently uses hooks before/after a single blocking save-load path (macro-phases only).
/// </summary>
class LoadSaveGameResourceJob final : public IResourceLoadingJob
{
public:
	StepResult step() override
	{
		switch (phase)
		{
		case Phase::PresentInitialFrame:
			phase = Phase::BeforeSaveLoad;
			return StepResult::InProgress;
		case Phase::BeforeSaveLoad:
			saveGameLoadBefore();
			phase = Phase::RunSaveLoad;
			return StepResult::InProgress;
		case Phase::RunSaveLoad:
			if (!saveGameLoadRun())
			{
				return StepResult::Failed;
			}
			phase = Phase::AfterSaveLoad;
			return StepResult::InProgress;
		case Phase::AfterSaveLoad:
			return saveGameLoadAfter()
			           ? StepResult::Completed
			           : StepResult::Failed;
		}
		return StepResult::Failed;
	}

	void finalizeSuccess() override
	{
		closeLoadingScreen();
	}

	void finalizeFailure() override
	{
		saveGameLoadAbortOnFailure();
		closeLoadingScreen();
	}

private:
	enum class Phase
	{
		PresentInitialFrame,
		BeforeSaveLoad,
		RunSaveLoad,
		AfterSaveLoad,
	};

	Phase phase = Phase::PresentInitialFrame;
};

/// <summary>
/// Lobby map backdrop preview job:
/// One-shot CPU load + raster upload; defers work to controller but finishes same frame.
/// </summary>
class MapPreviewJob final : public IResourceLoadingJob
{
public:
	explicit MapPreviewJob(ResourceLoadingRequest requestIn)
		: request(std::move(requestIn))
	{
	}

	StepResult step() override
	{
		if (request.previewMapName.empty())
		{
			loadMapPreview(request.hideInterface);
		}
		else
		{
			loadMapPreview(request.hideInterface, request.previewMapName.c_str(), request.previewMapHash);
		}
		return StepResult::Completed;
	}

	void finalizeSuccess() override { }

	void finalizeFailure() override { }

	ResourceLoadingController::FrameProcessingMode frameProcessingMode() const override
	{
		return ResourceLoadingController::FrameProcessingMode::ContinueMainLoop;
	}

private:
	ResourceLoadingRequest request;
};

} // anonymous namespace

ResourceLoadingController& ResourceLoadingController::instance()
{
	static ResourceLoadingController instance;
	return instance;
}

void ResourceLoadingController::request(ResourceLoadingRequest requestIn)
{
	if (activeJob)
	{
		queuedRequest = std::move(requestIn);
		return;
	}

	begin(std::move(requestIn));
}

void ResourceLoadingController::begin(ResourceLoadingRequest requestIn)
{
	ASSERT(!activeJob, "LoadingController.begin called while another loading job is active");
	activeRequest = std::move(requestIn);
	activeJob = makeJob(activeRequest.value());
	ASSERT(activeJob, "Failed to create loading job");

	const bool hadLoadingScreen = isLoadingScreenActive();
	if (activeRequest->showLoadingScreen && !hadLoadingScreen)
	{
		initLoadingScreen(activeRequest->drawBackdrop);
	}
}

bool ResourceLoadingController::active() const
{
	return activeJob != nullptr;
}

void ResourceLoadingController::step()
{
	ASSERT(activeJob, "LoadingController.step called without an active job");
	IResourceLoadingJob::StepResult result = activeJob->step();
	switch (result)
	{
	case IResourceLoadingJob::StepResult::InProgress:
		return;
	case IResourceLoadingJob::StepResult::Completed:
		activeJob->finalizeSuccess();
		break;
	case IResourceLoadingJob::StepResult::Failed:
		activeJob->finalizeFailure();
		break;
	}

	activeJob.reset();
	activeRequest.reset();

	if (queuedRequest.has_value())
	{
		ResourceLoadingRequest nextRequest = std::move(queuedRequest.value());
		queuedRequest.reset();
		begin(std::move(nextRequest));
	}
}

void ResourceLoadingController::presentLoadingScreenIfNeeded()
{
	if (activeRequest && activeRequest->showLoadingScreen)
	{
		presentLoadingScreenForCurrentFrame();
	}
}

ResourceLoadingController::FrameProcessingMode ResourceLoadingController::currentFrameProcessingMode() const
{
	ASSERT(activeJob, "LoadingController.currentFrameProcessingMode called without an active job");
	return activeJob->frameProcessingMode();
}

bool ResourceLoadingController::loadingScreenHandledByController() const
{
	return activeJob != nullptr && activeRequest.has_value() && activeRequest->showLoadingScreen;
}

std::unique_ptr<IResourceLoadingJob> ResourceLoadingController::makeJob(const ResourceLoadingRequest &request)
{
	switch (request.kind)
	{
	case ResourceLoadingRequest::Kind::FrontendInit:
		return std::make_unique<FrontendInitJob>(request);
	case ResourceLoadingRequest::Kind::StartGame:
		return std::make_unique<StartGameResourceJob>();
	case ResourceLoadingRequest::Kind::LoadSaveGame:
		return std::make_unique<LoadSaveGameResourceJob>();
	case ResourceLoadingRequest::Kind::MapPreview:
		return std::make_unique<MapPreviewJob>(request);
	}
	return nullptr;
}
