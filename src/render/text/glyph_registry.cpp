#include "render/text/glyph_registry.h"

#include "core/log.h"

#include <json.hpp>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <vector>

namespace {
  constexpr Logger kLog("glyph-registry");

  std::vector<std::string> assetDirCandidates() {
    std::vector<std::string> dirs;
    if (const char* env = std::getenv("NOCTALIA_GREETER_ASSETS_DIR"); env && env[0] != '\0') {
      dirs.emplace_back(env);
    }
#ifdef NOCTALIA_GREETER_INSTALLED_ASSETS_DIR
    dirs.emplace_back(NOCTALIA_GREETER_INSTALLED_ASSETS_DIR);
    const std::string installed = NOCTALIA_GREETER_INSTALLED_ASSETS_DIR;
    if (!installed.empty() && installed[0] != '/') {
      dirs.emplace_back("/usr/share/" + installed);
    }
#endif
    dirs.emplace_back(NOCTALIA_GREETER_ASSETS_DIR);
    dirs.emplace_back("/usr/share/noctalia-greeter/assets");
    return dirs;
  }

  std::string getAssetsDir() {
    for (const auto& dir : assetDirCandidates()) {
      if (dir.empty()) continue;
      std::ifstream probe(dir + "/fonts/tabler.json");
      if (probe.is_open()) {
        return dir;
      }
    }
    return assetDirCandidates().empty() ? std::string{} : assetDirCandidates().front();
  }

  std::string loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
  }

  char32_t parseUnicode(std::string_view str) {
    if (str.size() < 3 || str[0] != 'U' || str[1] != '+') return 0;
    std::string hex(str.substr(2));
    unsigned long codepoint = 0;
    std::istringstream iss(hex);
    iss >> std::hex >> codepoint;
    return static_cast<char32_t>(codepoint);
  }
}

void GlyphRegistry::initialize() {
  loadTablerGlyphs();
}

void GlyphRegistry::loadTablerGlyphs() {
  std::string assetsDir = getAssetsDir();
  std::string jsonPath = assetsDir + "/fonts/tabler.json";
  std::string content = loadFile(jsonPath);
  if (content.empty()) {
    kLog.warn("failed to load tabler.json from {}", jsonPath);
    return;
  }

  try {
    nlohmann::json data = nlohmann::json::parse(content);
    auto& g = glyphs();
    for (auto& [name, value] : data.items()) {
      if (value.is_string()) {
        char32_t cp = parseUnicode(value.get<std::string>());
        if (cp != 0) {
          g[name] = cp;
        }
      }
    }
    kLog.info("loaded {} tabler glyphs", g.size());
  } catch (const std::exception& e) {
    kLog.error("failed to parse tabler.json: {}", e.what());
  }
}

char32_t GlyphRegistry::lookup(std::string_view name) {
  auto& g = glyphs();
  auto it = g.find(std::string(name));
  if (it != g.end()) return it->second;
  return 0xFFFD;
}

void GlyphRegistry::registerGlyph(std::string_view name, char32_t codepoint) {
  glyphs()[std::string(name)] = codepoint;
}

std::optional<std::string> GlyphRegistry::fontPath() {
  const std::string path = getAssetsDir() + "/fonts/tabler.ttf";
  std::ifstream probe(path);
  if (!probe.is_open()) {
    kLog.warn("tabler.ttf not found at {}", path);
    return std::nullopt;
  }
  return path;
}

std::unordered_map<std::string, char32_t>& GlyphRegistry::glyphs() {
  static std::unordered_map<std::string, char32_t> s_glyphs;
  return s_glyphs;
}
