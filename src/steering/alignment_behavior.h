// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file alignment_behavior.h
 * Alignment steering: match average velocity of nearby flock mates.
 */

#pragma once

#include "steering.h"

namespace steering
{

class AlignmentBehavior : public ISteeringBehavior
{
public:
	SteeringForce calculate(const SteeringContext& ctx) override;

	const char* name() const override { return "Alignment"; }

	SteeringBehaviorCategory category() const override { return SteeringBehaviorCategory::Modifier; }

	bool isEnabled(const SteeringContext& ctx) const override;
};

} // namespace steering
