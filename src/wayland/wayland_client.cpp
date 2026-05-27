#include "wayland/wayland_client.h"

#include "core/log.h"
#include "xdg-shell-client-protocol.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <cstdlib>
#include <wayland-client.h>

namespace {
  constexpr Logger kLog("wayland");

  void xdgWmBasePing(void* /*data*/, xdg_wm_base* wmBase, std::uint32_t serial) { xdg_wm_base_pong(wmBase, serial); }

  const xdg_wm_base_listener kXdgWmBaseListener = {
      .ping = xdgWmBasePing,
  };

  const wl_registry_listener kRegistryListener = {
      .global = &WaylandClient::handleGlobal,
      .global_remove = &WaylandClient::handleGlobalRemove,
  };

  void outputGeometry(void* /*data*/, wl_output* /*output*/, int32_t /*x*/, int32_t /*y*/, int32_t /*physW*/,
                      int32_t /*physH*/, int32_t /*subpixel*/, const char* /*make*/, const char* /*model*/,
                      int32_t /*transform*/) {}

  const wl_output_listener kOutputListener = {
      .geometry = outputGeometry,
      .mode = &WaylandClient::handleOutputMode,
      .done = &WaylandClient::handleOutputDone,
      .scale = &WaylandClient::handleOutputScale,
      .name = &WaylandClient::handleOutputName,
      .description = &WaylandClient::handleOutputDescription,
  };
}

WaylandClient::WaylandClient() = default;

WaylandClient::~WaylandClient() { disconnect(); }

bool WaylandClient::connect() {
  if (m_display != nullptr) {
    return true;
  }

  constexpr int kMaxConnectAttempts = 60; // ~3s total with 50ms backoff
  for (int attempt = 1; attempt <= kMaxConnectAttempts; ++attempt) {
    errno = 0;
    m_display = wl_display_connect(nullptr);
    if (m_display != nullptr) {
      break;
    }

    const int err = errno;
    const bool retryable = (err == ENOENT || err == ECONNREFUSED || err == EACCES);
    if (!retryable || attempt == kMaxConnectAttempts) {
      const char* wlDisplay = std::getenv("WAYLAND_DISPLAY");
      const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR");
      kLog.error(
          "wl_display_connect failed (errno={} '{}', WAYLAND_DISPLAY={}, XDG_RUNTIME_DIR={}, attempt={}/{})", err,
          std::strerror(err), wlDisplay != nullptr ? wlDisplay : "unset", runtimeDir != nullptr ? runtimeDir : "unset",
          attempt, kMaxConnectAttempts
      );
      return false;
    }

    if (attempt == 1 || attempt % 10 == 0) {
      kLog.warn("wl_display_connect retry {}/{} (errno={} '{}')", attempt, kMaxConnectAttempts, err, std::strerror(err));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  m_registry = wl_display_get_registry(m_display);
  if (m_registry == nullptr) {
    kLog.error("wl_display_get_registry failed");
    disconnect();
    return false;
  }

  if (wl_registry_add_listener(m_registry, &kRegistryListener, this) != 0) {
    kLog.error("wl_registry_add_listener failed");
    disconnect();
    return false;
  }

  if (wl_display_roundtrip(m_display) < 0) {
    kLog.error("Wayland registry roundtrip failed: {}", std::strerror(errno));
    disconnect();
    return false;
  }

  if (!m_compositor || !m_seat || !m_xdgWmBase) {
    kLog.error("missing required Wayland globals (compositor/seat/xdg-shell)");
    disconnect();
    return false;
  }

  kLog.info("connected to Wayland compositor");
  return true;
}

void WaylandClient::disconnect() {
  m_seatHandler.cleanup();

  if (m_xdgWmBase != nullptr) {
    xdg_wm_base_destroy(m_xdgWmBase);
    m_xdgWmBase = nullptr;
  }
  if (m_seat != nullptr) {
    wl_seat_destroy(m_seat);
    m_seat = nullptr;
  }
  if (m_compositor != nullptr) {
    wl_compositor_destroy(m_compositor);
    m_compositor = nullptr;
  }
  if (m_registry != nullptr) {
    wl_registry_destroy(m_registry);
    m_registry = nullptr;
  }
  if (m_display != nullptr) {
    wl_display_disconnect(m_display);
    m_display = nullptr;
  }
}

int WaylandClient::dispatch() const {
  return m_display != nullptr ? wl_display_dispatch(m_display) : -1;
}

int WaylandClient::dispatchPending() const {
  return m_display != nullptr ? wl_display_dispatch_pending(m_display) : -1;
}

int WaylandClient::flush() const { return m_display != nullptr ? wl_display_flush(m_display) : -1; }

void WaylandClient::setPointerEventCallback(WaylandSeat::PointerEventCallback callback) {
  m_seatHandler.setPointerEventCallback(std::move(callback));
}

void WaylandClient::setKeyboardEventCallback(WaylandSeat::KeyboardEventCallback callback) {
  m_seatHandler.setKeyboardEventCallback(std::move(callback));
}

void WaylandClient::setOutputsChangedCallback(std::function<void()> callback) {
  m_outputsChangedCallback = std::move(callback);
}

const WaylandOutputInfo* WaylandClient::primaryOutput() const noexcept {
  const WaylandOutputInfo* best = nullptr;
  std::int64_t bestArea = 0;
  for (const auto& out : m_outputs) {
    if (!out.done || out.pixelWidth <= 0 || out.pixelHeight <= 0) {
      continue;
    }
    const std::int64_t area = static_cast<std::int64_t>(out.pixelWidth) * static_cast<std::int64_t>(out.pixelHeight);
    if (area > bestArea) {
      bestArea = area;
      best = &out;
    }
  }
  return best;
}

int32_t WaylandClient::effectiveBufferScale() const noexcept {
  const WaylandOutputInfo* out = primaryOutput();
  if (out == nullptr) {
    return 1;
  }

  const int32_t scale = std::max(1, out->scale);
  if (scale <= 1 || out->pixelWidth <= 0 || out->pixelHeight <= 0) {
    return 1;
  }

  // wl_output mode is physical pixels; divide by scale for logical size.
  const int32_t derivedLogicalW = (out->pixelWidth + scale - 1) / scale;
  // Nested compositors (e.g. cage) sometimes report logical sizes in the mode event.
  if (derivedLogicalW < 640 && out->pixelWidth >= 640) {
    return 1;
  }

  return scale;
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> WaylandClient::primaryLogicalSize() const noexcept {
  const WaylandOutputInfo* out = primaryOutput();
  if (out == nullptr || out->pixelWidth <= 0 || out->pixelHeight <= 0) {
    return std::nullopt;
  }

  const int32_t scale = std::max(1, out->scale);
  if (effectiveBufferScale() == 1) {
    return std::pair{static_cast<std::uint32_t>(out->pixelWidth), static_cast<std::uint32_t>(out->pixelHeight)};
  }

  const auto logicalWidth = static_cast<std::uint32_t>(std::max(1, (out->pixelWidth + scale - 1) / scale));
  const auto logicalHeight = static_cast<std::uint32_t>(std::max(1, (out->pixelHeight + scale - 1) / scale));
  return std::pair{logicalWidth, logicalHeight};
}

void WaylandClient::notifyOutputsChanged() {
  if (m_outputsChangedCallback) {
    m_outputsChangedCallback();
  }
}

void WaylandClient::handleOutputMode(void* data, wl_output* wlOut, std::uint32_t flags, std::int32_t w,
                                     std::int32_t h, std::int32_t /*refresh*/) {
  if ((flags & WL_OUTPUT_MODE_CURRENT) == 0) {
    return;
  }
  auto* client = static_cast<WaylandClient*>(data);
  for (auto& out : client->m_outputs) {
    if (out.output == wlOut) {
      out.pixelWidth = w;
      out.pixelHeight = h;
      if (out.done) {
        client->notifyOutputsChanged();
      }
      break;
    }
  }
}

void WaylandClient::handleOutputDone(void* data, wl_output* wlOut) {
  auto* client = static_cast<WaylandClient*>(data);
  for (auto& out : client->m_outputs) {
    if (out.output == wlOut && !out.done) {
      out.done = true;
      kLog.info("output ready {}x{} scale={}", out.pixelWidth, out.pixelHeight, out.scale);
      client->notifyOutputsChanged();
      break;
    }
  }
}

void WaylandClient::handleOutputScale(void* data, wl_output* wlOut, std::int32_t factor) {
  auto* client = static_cast<WaylandClient*>(data);
  for (auto& out : client->m_outputs) {
    if (out.output == wlOut) {
      const int32_t next = std::max(1, factor);
      if (out.scale != next) {
        out.scale = next;
        if (out.done) {
          kLog.info("output scale updated to {}", next);
          client->notifyOutputsChanged();
        }
      }
      break;
    }
  }
}

void WaylandClient::handleOutputName(void* /*data*/, wl_output* /*output*/, const char* /*name*/) {}

void WaylandClient::handleOutputDescription(void* /*data*/, wl_output* /*output*/, const char* /*description*/) {}

void WaylandClient::bindOutput(wl_registry* registry, std::uint32_t name, std::uint32_t version) {
  const std::uint32_t bindVersion = std::min(version, 4u);
  auto* output = static_cast<wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, bindVersion));
  if (output == nullptr) {
    return;
  }

  WaylandOutputInfo info;
  info.output = output;
  info.registryName = name;
  m_outputs.push_back(info);
  wl_output_add_listener(output, &kOutputListener, this);
}

int WaylandClient::repeatPollTimeoutMs() const { return m_seatHandler.repeatPollTimeoutMs(); }

void WaylandClient::repeatTick() { m_seatHandler.repeatTick(); }

void WaylandClient::handleGlobal(void* data, wl_registry* registry, std::uint32_t name, const char* interface,
                                 std::uint32_t version) {
  static_cast<WaylandClient*>(data)->bindGlobal(registry, name, interface, version);
}

void WaylandClient::handleGlobalRemove(void* /*data*/, wl_registry* /*registry*/, std::uint32_t /*name*/) {}

void WaylandClient::bindGlobal(wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version) {
  const std::string interfaceName = interface != nullptr ? interface : "";
  const std::uint32_t bindVersion = std::min(version, 6u);

  if (interfaceName == wl_compositor_interface.name) {
    m_compositor =
        static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, bindVersion));
    return;
  }

  if (interfaceName == wl_seat_interface.name) {
    m_seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, bindVersion));
    m_seatHandler.bind(m_seat);
    return;
  }

  if (interfaceName == xdg_wm_base_interface.name) {
    m_xdgWmBase = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, bindVersion));
    xdg_wm_base_add_listener(m_xdgWmBase, &kXdgWmBaseListener, this);
    return;
  }

  if (interfaceName == wl_output_interface.name) {
    bindOutput(registry, name, version);
  }
}
