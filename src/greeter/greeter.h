#pragma once

#include "greetd/greetd_client.h"
#include "render/gl_shared_context.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class GreeterSurface;
class GreeterWindow;
class RenderContext;
class WaylandClient;

class Greeter {
public:
  Greeter();
  ~Greeter();

  bool initialize(WaylandClient& client);
  int run(WaylandClient& client);

  void onKeyboardEvent(std::uint32_t sym, std::uint32_t utf32, std::uint32_t modifiers, bool pressed, bool preedit);
  void onPointerMotion(double x, double y);
  void onPointerButton(double x, double y, std::uint32_t button, bool pressed);

  void onThemeChanged();

  bool startSession(const std::string& command);

private:
  void connectGreetd();
  void setupInputCallbacks(WaylandClient& client);

  WaylandClient* m_client = nullptr;
  GlSharedContext m_glSharedContext;
  std::unique_ptr<RenderContext> m_renderContext;
  std::unique_ptr<GreeterSurface> m_surface;
  std::unique_ptr<GreeterWindow> m_window;
  GreetdClient m_greetdClient;

  std::string m_defaultUsername;
  bool m_sessionStarted = false;
  bool m_exitRequested = false;
};
