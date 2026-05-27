#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <string>

class GlyphRegistry {
public:
  static void initialize();
  static char32_t lookup(std::string_view name);
  static void registerGlyph(std::string_view name, char32_t codepoint);
  [[nodiscard]] static std::optional<std::string> fontPath();

private:
  static void loadTablerGlyphs();
  static std::unordered_map<std::string, char32_t>& glyphs();
};
