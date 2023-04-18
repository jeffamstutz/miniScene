// Copyright 2020 Jefferson Amstutz
// SPDX-License-Identifier: Unlicense

#pragma once

#include "linalg.h"

using namespace linalg::aliases;

enum class OrbitAxis
{
  POS_X,
  POS_Y,
  POS_Z,
  NEG_X,
  NEG_Y,
  NEG_Z
};

class Orbit
{
 public:
  Orbit() = default;
  Orbit(float3 at, float dist, float2 azel = float2(0.f, -20.f));

  void startNewRotation();

  void rotate(float2 delta);
  void zoom(float delta);
  void pan(float2 delta);

  void setAxis(OrbitAxis axis);

  float2 azel() const;

  float3 eye() const;
  float3 dir() const;
  float3 at() const;
  float3 up() const;

  float distance() const;

  float3 eye_FixedDistance() const; // using original distance

 protected:
  void update();

  // Data //

  // NOTE: degrees
  float2 m_azel;

  float m_distance{1.f};
  float m_originalDistance{1.f};
  float m_speed{0.25f};

  bool m_invertRotation{false};

  float3 m_eye;
  float3 m_eyeFixedDistance;
  float3 m_at;
  float3 m_up;
  float3 m_right;

  OrbitAxis m_axis{OrbitAxis::POS_Y};
};
