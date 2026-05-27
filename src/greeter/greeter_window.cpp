#include "greeter/greeter_window.h"

#include "core/log.h"
#include "greeter/greeter_surface.h"
#include "render/gl_shared_context.h"
#include "render/render_context.h"
#include "render/scene/node.h"
#include "wayland/wayland_client.h"
#include "xdg-shell-client-protocol.h"

#include <algorithm>
#include <wayland-client.h>

namespace {
  constexpr Logger kLog("greeter-window");

  const xdg_surface_listener kXdgSurfaceListener = {
      .configure = &GreeterWindow::handleXdgSurfaceConfigure,
  };

  const xdg_toplevel_listener kToplevelListener = {
      .configure = &GreeterWindow::handleToplevelConfigure,
      .close = &GreeterWindow::handleToplevelClose,
      .configure_bounds = &GreeterWindow::handleToplevelConfigureBounds,
      .wm_capabilities = &GreeterWindow::handleToplevelWmCapabilities,
  };

  const wl_callback_listener kFrameListener = {
      .done = &GreeterWindow::handleFrameDone,
  };
}

GreeterWindow::GreeterWindow(WaylandClient& client, GlSharedContext& gl, RenderContext& renderContext,
                             GreeterSurface& surface)
    : m_client(client), m_gl(gl), m_renderContext(renderContext), m_greeterSurface(surface) {}

GreeterWindow::~GreeterWindow() {
  destroyRoleObjects();
  destroySurface();
}

bool GreeterWindow::initialize() {
  if (!m_client.hasXdgShell() || m_client.compositor() == nullptr) {
    kLog.error("missing xdg-shell or compositor");
    return false;
  }

  m_wlSurface = wl_compositor_create_surface(m_client.compositor());
  if (m_wlSurface == nullptr) {
    kLog.error("wl_compositor_create_surface failed");
    return false;
  }

  m_xdgSurface = xdg_wm_base_get_xdg_surface(m_client.xdgWmBase(), m_wlSurface);
  if (m_xdgSurface == nullptr) {
    destroySurface();
    return false;
  }
  xdg_surface_add_listener(m_xdgSurface, &kXdgSurfaceListener, this);

  m_toplevel = xdg_surface_get_toplevel(m_xdgSurface);
  if (m_toplevel == nullptr) {
    destroyRoleObjects();
    destroySurface();
    return false;
  }

  xdg_toplevel_add_listener(m_toplevel, &kToplevelListener, this);
  xdg_toplevel_set_title(m_toplevel, "Noctalia Greeter");
  xdg_toplevel_set_app_id(m_toplevel, "dev.noctalia.Greeter");
  xdg_toplevel_set_fullscreen(m_toplevel, nullptr);

  // Initial commit (shell ToplevelSurface::initialize).
  wl_surface_commit(m_wlSurface);
  if (m_client.flush() < 0) {
    kLog.error("Wayland flush failed during window init");
    destroyRoleObjects();
    destroySurface();
    return false;
  }

  if (wl_display_roundtrip(m_client.display()) < 0) {
    kLog.error("initial configure roundtrip failed");
    destroyRoleObjects();
    destroySurface();
    return false;
  }

  kLog.info("greeter window created");
  return true;
}

void GreeterWindow::setSceneReady(bool ready) {
  m_sceneReady = ready;
  if (ready && m_configured) {
    m_layoutNeeded = true;
    m_redrawNeeded = true;
    renderNow();
  }
}

void GreeterWindow::requestLayout() {
  m_layoutNeeded = true;
  m_redrawNeeded = true;
  renderNow();
}

void GreeterWindow::requestRedraw() {
  m_redrawNeeded = true;
  renderNow();
}

void GreeterWindow::matchPrimaryOutputSize() {
  if (m_lastToplevelWidth > 0 && m_lastToplevelHeight > 0) {
    return;
  }

  const auto logical = m_client.primaryLogicalSize();
  if (!logical) {
    return;
  }

  const int32_t nextScale = m_client.effectiveBufferScale();
  const bool sizeChanged = m_width != logical->first || m_height != logical->second || !m_configured;
  const bool scaleChanged = m_bufferScale != nextScale;
  if (sizeChanged || scaleChanged) {
    applyConfigure(logical->first, logical->second);
  }
}

void GreeterWindow::renderNow() {
  if (!m_sceneReady || m_width == 0 || m_height == 0 || m_wlSurface == nullptr) {
    return;
  }

  paintFrame();
  requestNextFrame();
}

void GreeterWindow::applySurfaceScale() {
  if (m_wlSurface == nullptr) {
    return;
  }
  m_bufferScale = m_client.effectiveBufferScale();
  wl_surface_set_buffer_scale(m_wlSurface, m_bufferScale);
}

std::uint32_t GreeterWindow::bufferWidthForLogical(std::uint32_t logical) const noexcept {
  return std::max<std::uint32_t>(
      1, static_cast<std::uint32_t>(std::lround(static_cast<float>(logical) * static_cast<float>(m_bufferScale))));
}

std::uint32_t GreeterWindow::bufferHeightForLogical(std::uint32_t logical) const noexcept {
  return std::max<std::uint32_t>(
      1, static_cast<std::uint32_t>(std::lround(static_cast<float>(logical) * static_cast<float>(m_bufferScale))));
}

bool GreeterWindow::ensureRenderTarget() {
  const std::uint32_t bufferWidth = bufferWidthForLogical(m_width);
  const std::uint32_t bufferHeight = bufferHeightForLogical(m_height);

  if (m_renderTarget.isReady()) {
    if (m_renderTarget.bufferWidth() != bufferWidth || m_renderTarget.bufferHeight() != bufferHeight) {
      m_renderTarget.destroy();
    }
  }

  if (m_renderTarget.isReady()) {
    m_renderTarget.resize(bufferWidth, bufferHeight);
    m_renderTarget.setLogicalSize(m_width, m_height);
    m_renderContext.syncContentScale(m_renderTarget);
    return true;
  }

  m_renderTarget.create(m_wlSurface, m_renderContext);
  m_renderTarget.resize(bufferWidth, bufferHeight);
  m_renderTarget.setLogicalSize(m_width, m_height);
  m_renderContext.syncContentScale(m_renderTarget);

  if (!m_renderTarget.isReady()) {
    kLog.error("render target not ready ({}x{} logical, {}x{} buffer)", m_width, m_height, bufferWidth, bufferHeight);
    return false;
  }

  return true;
}

void GreeterWindow::paintFrame() {
  if (!ensureRenderTarget()) {
    return;
  }

  const bool needsLayout = m_layoutNeeded || m_greeterSurface.sceneRoot()->layoutDirty();
  const bool needsRedraw =
      m_redrawNeeded || needsLayout || m_greeterSurface.sceneRoot()->paintDirty();

  if (!needsRedraw) {
    return;
  }

  if (needsLayout) {
    m_greeterSurface.prepareFrame(false, true);
    m_layoutNeeded = false;
  } else {
    m_greeterSurface.prepareFrame(false, false);
  }

  m_renderContext.renderScene(m_renderTarget, m_greeterSurface.sceneRoot());

  m_redrawNeeded = false;
  m_greeterSurface.sceneRoot()->clearDirty();

  static bool loggedFirstFrame = false;
  if (!loggedFirstFrame) {
    kLog.info("presented first frame {}x{} logical ({}x{} buffer, scale={})", m_width, m_height,
              m_renderTarget.bufferWidth(), m_renderTarget.bufferHeight(), m_bufferScale);
    loggedFirstFrame = true;
  }
}

void GreeterWindow::requestNextFrame() {
  if (m_wlSurface == nullptr || m_frameCallback != nullptr) {
    return;
  }

  m_frameCallback = wl_surface_frame(m_wlSurface);
  if (m_frameCallback == nullptr) {
    return;
  }

  wl_callback_add_listener(m_frameCallback, &kFrameListener, this);
  if (m_client.flush() < 0) {
    kLog.error("Wayland flush failed while requesting next frame");
  }
}

void GreeterWindow::handleFrameDone(void* data, wl_callback* callback, std::uint32_t /*time*/) {
  auto* self = static_cast<GreeterWindow*>(data);
  wl_callback_destroy(callback);
  self->m_frameCallback = nullptr;

  if (!self->m_sceneReady) {
    return;
  }

  const bool dirty = self->m_redrawNeeded || self->m_layoutNeeded || self->m_greeterSurface.sceneRoot()->paintDirty() ||
                     self->m_greeterSurface.sceneRoot()->layoutDirty();
  if (dirty) {
    self->paintFrame();
  }

  self->requestNextFrame();
}

void GreeterWindow::applyConfigure(std::uint32_t width, std::uint32_t height) {
  width = std::max<std::uint32_t>(1, width);
  height = std::max<std::uint32_t>(1, height);

  const int32_t nextScale = m_client.effectiveBufferScale();
  const bool sizeChanged = m_width != width || m_height != height || !m_configured;
  const bool scaleChanged = m_bufferScale != nextScale;

  applySurfaceScale();

  if (sizeChanged || scaleChanged) {
    m_renderTarget.destroy();
    m_width = width;
    m_height = height;
    m_configured = true;
    m_layoutNeeded = true;
    m_redrawNeeded = true;
    kLog.info("configured {}x{} logical (scale={})", m_width, m_height, m_bufferScale);
  }

  if (m_sceneReady) {
    renderNow();
  }
}

void GreeterWindow::handleXdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* self = static_cast<GreeterWindow*>(data);
  xdg_surface_ack_configure(surface, serial);

  std::uint32_t width = self->m_pendingWidth > 0 ? static_cast<std::uint32_t>(self->m_pendingWidth) : 0;
  std::uint32_t height = self->m_pendingHeight > 0 ? static_cast<std::uint32_t>(self->m_pendingHeight) : 0;
  if (self->m_lastToplevelWidth > 0) {
    width = static_cast<std::uint32_t>(self->m_lastToplevelWidth);
  }
  if (self->m_lastToplevelHeight > 0) {
    height = static_cast<std::uint32_t>(self->m_lastToplevelHeight);
  }
  if (width == 0 || height == 0) {
    if (const auto logical = self->m_client.primaryLogicalSize()) {
      if (width == 0) {
        width = logical->first;
      }
      if (height == 0) {
        height = logical->second;
      }
    }
  }
  if (width == 0) {
    width = 1280;
  }
  if (height == 0) {
    height = 720;
  }
  if (const auto output = self->m_client.primaryLogicalSize()) {
    if (width > output->first || height > output->second) {
      kLog.info("clamping configure {}x{} to output {}x{}", width, height, output->first, output->second);
      width = std::min(width, output->first);
      height = std::min(height, output->second);
    }
  }
  self->applyConfigure(width, height);
}

void GreeterWindow::handleToplevelConfigure(void* data, xdg_toplevel* /*toplevel*/, std::int32_t width,
                                            std::int32_t height, wl_array* /*states*/) {
  auto* self = static_cast<GreeterWindow*>(data);
  if (width > 0) {
    self->m_lastToplevelWidth = width;
    self->m_pendingWidth = width;
  }
  if (height > 0) {
    self->m_lastToplevelHeight = height;
    self->m_pendingHeight = height;
  }
}

void GreeterWindow::handleToplevelClose(void* /*data*/, xdg_toplevel* /*toplevel*/) {
  kLog.info("toplevel close requested");
}

void GreeterWindow::handleToplevelConfigureBounds(void* /*data*/, xdg_toplevel* /*toplevel*/, std::int32_t /*width*/,
                                                  std::int32_t /*height*/) {}

void GreeterWindow::handleToplevelWmCapabilities(void* /*data*/, xdg_toplevel* /*toplevel*/,
                                                 wl_array* /*capabilities*/) {}

void GreeterWindow::destroyRoleObjects() {
  if (m_frameCallback != nullptr) {
    wl_callback_destroy(m_frameCallback);
    m_frameCallback = nullptr;
  }
  if (m_toplevel != nullptr) {
    xdg_toplevel_destroy(m_toplevel);
    m_toplevel = nullptr;
  }
  if (m_xdgSurface != nullptr) {
    xdg_surface_destroy(m_xdgSurface);
    m_xdgSurface = nullptr;
  }
  m_renderTarget.destroy();
}

void GreeterWindow::destroySurface() {
  if (m_wlSurface != nullptr) {
    wl_surface_destroy(m_wlSurface);
    m_wlSurface = nullptr;
  }
}
