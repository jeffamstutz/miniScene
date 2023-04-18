// Copyright 2020 Jefferson Amstutz
// SPDX-License-Identifier: Unlicense

#include "Orbit.h"

// Helper functions ///////////////////////////////////////////////////////////

static float3 azelToDirection(float az, float el, OrbitAxis axis) {
  const float x = std::sin(az) * std::cos(el);
  const float y = std::cos(az) * std::cos(el);
  const float z = std::sin(el);
  switch (axis) {
  case OrbitAxis::POS_X:
    return -normalize(float3(z, y, x));
  case OrbitAxis::POS_Y:
    return -normalize(float3(x, z, y));
  case OrbitAxis::POS_Z:
    return -normalize(float3(x, y, z));
  case OrbitAxis::NEG_X:
    return normalize(float3(z, y, x));
  case OrbitAxis::NEG_Y:
    return normalize(float3(x, z, y));
  case OrbitAxis::NEG_Z:
    return normalize(float3(x, y, z));
  }
  return {};
}

static OrbitAxis negateAxis(OrbitAxis current) {
  switch (current) {
  case OrbitAxis::POS_X:
    return OrbitAxis::NEG_X;
  case OrbitAxis::POS_Y:
    return OrbitAxis::NEG_Y;
  case OrbitAxis::POS_Z:
    return OrbitAxis::NEG_Z;
  case OrbitAxis::NEG_X:
    return OrbitAxis::POS_X;
  case OrbitAxis::NEG_Y:
    return OrbitAxis::POS_Y;
  case OrbitAxis::NEG_Z:
    return OrbitAxis::POS_Z;
  }
  return {};
}

static float maintainUnitCircle(float inDegrees) {
  while (inDegrees > 360.f)
    inDegrees -= 360.f;
  while (inDegrees < 0.f)
    inDegrees += 360.f;
  return inDegrees;
}

static float radians(float degrees) {
  return degrees * static_cast<float>(0.01745329251994329576923690768489);
}

// Orbit definitions //////////////////////////////////////////////////////////

Orbit::Orbit(float3 at, float dist, float2 azel)
    : m_azel(azel), m_distance(dist), m_at(at) {
  m_speed = m_distance;
  m_originalDistance = dist;
  update();
}

void Orbit::startNewRotation() {
  m_invertRotation = m_azel.y > 90.f && m_azel.y < 270.f;
}

void Orbit::rotate(float2 delta) {
  delta *= 100.f;
  delta.x = m_invertRotation ? -delta.x : delta.x;
  delta.y = m_distance < 0.f ? -delta.y : delta.y;
  m_azel += delta;
  m_azel.x = maintainUnitCircle(m_azel.x);
  m_azel.y = maintainUnitCircle(m_azel.y);
  update();
}

void Orbit::zoom(float delta) {
  m_distance += m_speed * delta;
  update();
}

void Orbit::pan(float2 delta) {
  delta *= m_speed;
  delta.y = -delta.y;

  const float3 amount = delta.x * m_right + delta.y * m_up;

  m_eye += amount;
  m_at += amount;

  update();
}

void Orbit::setAxis(OrbitAxis axis) {
  m_axis = axis;
  update();
}

float2 Orbit::azel() const { return m_azel; }

float3 Orbit::eye() const { return m_eye; }

float3 Orbit::dir() const { return normalize(m_at - m_eye); }

float3 Orbit::at() const { return m_at; }

float3 Orbit::up() const { return m_up; }

float Orbit::distance() const { return m_distance; }

float3 Orbit::eye_FixedDistance() const { return m_eyeFixedDistance; }

void Orbit::update() {
  const float distance = std::abs(m_distance);

  const OrbitAxis axis = m_distance < 0.f ? negateAxis(m_axis) : m_axis;

  const float azimuth = radians(m_azel.x);
  const float elevation = radians(m_azel.y);

  const float3 toLocalOrbit = azelToDirection(azimuth, elevation, axis);

  const float3 localOrbitPos = toLocalOrbit * distance;
  const float3 fromLocalOrbit = -localOrbitPos;

  const float3 alteredElevation =
      azelToDirection(azimuth, elevation + 3, m_axis);

  const float3 cameraRight = cross(toLocalOrbit, alteredElevation);
  const float3 cameraUp = cross(cameraRight, fromLocalOrbit);

  m_eye = localOrbitPos + m_at;
  m_up = normalize(cameraUp);
  m_right = normalize(cameraRight);

  m_eyeFixedDistance = (m_originalDistance * toLocalOrbit) + m_at;
}
