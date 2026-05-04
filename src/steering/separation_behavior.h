// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file separation_behavior.h
 * Separation steering: steer away from nearby flock mates (same-player modifiers).
 */

#pragma once

#include "steering.h"

namespace steering
{

class SeparationBehavior : public ISteeringBehavior
{
public:
	SteeringForce calculate(const SteeringContext& ctx) override;

	const char* name() const override { return "Separation"; }

	SteeringBehaviorCategory category() const override { return SteeringBehaviorCategory::Modifier; }

	bool isEnabled(const SteeringContext& ctx) const override;
};

} // namespace steering
