#pragma once

#include "render/render_target.h"

#include <cstdint>
#include <functional>

class GlSharedContext;
class GreeterSurface;
class RenderContext;
class WaylandClient;
struct wl_array;
struct wl_callback;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;

// Wayland window using the same frame flow as noctalia-shell's Surface::render().
class GreeterWindow {
public:
  GreeterWindow(WaylandClient& client, GlSharedContext& gl, RenderContext& renderContext, GreeterSurface& surface);
  ~GreeterWindow();

  GreeterWindow(const GreeterWindow&) = delete;
  GreeterWindow& operator=(const GreeterWindow&) = delete;

  bool initialize();

  void setSceneReady(bool ready);

  void requestLayout();
  void requestRedraw();
  void matchPrimaryOutputSize();

  // Immediate layout + render + swap (shell Surface::renderNow / render).
  void renderNow();

  [[nodiscard]] std::uint32_t width() const noexcept { return m_width; }
  [[nodiscard]] std::uint32_t height() const noexcept { return m_height; }
  [[nodiscard]] wl_surface* wlSurface() const noexcept { return m_wlSurface; }
  [[nodiscard]] RenderTarget& renderTarget() noexcept { return m_renderTarget; }

  static void handleXdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial);
  static void handleToplevelConfigure(void* data, xdg_toplevel* toplevel, std::int32_t width, std::int32_t height,
                                      wl_array* states);
  static void handleToplevelClose(void* data, xdg_toplevel* toplevel);
  static void handleToplevelConfigureBounds(void* data, xdg_toplevel* toplevel, std::int32_t width,
                                            std::int32_t height);
  static void handleToplevelWmCapabilities(void* data, xdg_toplevel* toplevel, wl_array* capabilities);
  static void handleFrameDone(void* data, wl_callback* callback, std::uint32_t time);

private:
  void destroyRoleObjects();
  void destroySurface();
  void applyConfigure(std::uint32_t width, std::uint32_t height);
  bool ensureRenderTarget();
  void paintFrame();
  void requestNextFrame();

  WaylandClient& m_client;
  GlSharedContext& m_gl;
  RenderContext& m_renderContext;
  GreeterSurface& m_greeterSurface;

  wl_surface* m_wlSurface = nullptr;
  xdg_surface* m_xdgSurface = nullptr;
  xdg_toplevel* m_toplevel = nullptr;
  wl_callback* m_frameCallback = nullptr;

  RenderTarget m_renderTarget;
  void applySurfaceScale();
  [[nodiscard]] std::uint32_t bufferWidthForLogical(std::uint32_t logical) const noexcept;
  [[nodiscard]] std::uint32_t bufferHeightForLogical(std::uint32_t logical) const noexcept;

  std::uint32_t m_width = 0;
  std::uint32_t m_height = 0;
  std::int32_t m_pendingWidth = 0;
  std::int32_t m_pendingHeight = 0;
  std::int32_t m_lastToplevelWidth = 0;
  std::int32_t m_lastToplevelHeight = 0;
  int32_t m_bufferScale = 1;
  bool m_configured = false;
  bool m_sceneReady = false;
  bool m_layoutNeeded = true;
  bool m_redrawNeeded = true;
};
