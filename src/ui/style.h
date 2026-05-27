#pragma once

#include <algorithm>
#include <cmath>

class Style {
public:
  // Spacing
  static constexpr float spaceXs = 4.0f;
  static constexpr float spaceSm = 8.0f;
  static constexpr float spaceMd = 12.0f;
  static constexpr float spaceLg = 16.0f;
  static constexpr float spaceXl = 24.0f;
  static constexpr float space2xl = 32.0f;
  static constexpr float space3xl = 48.0f;

  // Sizes
  static constexpr float controlHeight = 36.0f;
  static constexpr float controlHeightSm = 30.0f;
  static constexpr float controlHeightLg = 44.0f;

  // Font sizes
  static constexpr float fontSizeCaption = 12.0f;
  static constexpr float fontSizeBody = 14.0f;
  static constexpr float fontSizeTitle = 16.0f;
  static constexpr float fontSizeHeading = 20.0f;
  static constexpr float fontSizeDisplay = 28.0f;

  // Border
  static constexpr float borderWidth = 1.0f;

  // Radii
  static constexpr float radiusSm = 4.0f;
  static constexpr float radiusMd = 8.0f;
  static constexpr float radiusLg = 12.0f;
  static constexpr float radiusXl = 16.0f;
  static constexpr float radiusFull = 9999.0f;

  static float scaledRadiusSm(float scale = 1.0f) noexcept { return radiusSm * scale; }
  static float scaledRadiusMd(float scale = 1.0f) noexcept { return radiusMd * scale; }
  static float scaledRadiusLg(float scale = 1.0f) noexcept { return radiusLg * scale; }
  static float scaledRadiusXl(float scale = 1.0f) noexcept { return radiusXl * scale; }

  // Animation
  static constexpr float animFast = 150.0f;
  static constexpr float animNormal = 250.0f;
  static constexpr float animSlow = 400.0f;
};
