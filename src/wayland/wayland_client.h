#pragma once

#include "wayland/wayland_seat.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

struct wl_compositor;
struct wl_display;
struct wl_output;
struct wl_registry;
struct wl_seat;
struct xdg_wm_base;

struct WaylandOutputInfo {
  wl_output* output = nullptr;
  std::uint32_t registryName = 0;
  int32_t pixelWidth = 0;
  int32_t pixelHeight = 0;
  int32_t scale = 1;
  bool done = false;
};

class WaylandClient {
public:
  WaylandClient();
  ~WaylandClient();

  WaylandClient(const WaylandClient&) = delete;
  WaylandClient& operator=(const WaylandClient&) = delete;

  bool connect();
  void disconnect();

  [[nodiscard]] int dispatch() const;
  [[nodiscard]] int dispatchPending() const;
  [[nodiscard]] int flush() const;

  void setPointerEventCallback(WaylandSeat::PointerEventCallback callback);
  void setKeyboardEventCallback(WaylandSeat::KeyboardEventCallback callback);
  void setOutputsChangedCallback(std::function<void()> callback);

  [[nodiscard]] int repeatPollTimeoutMs() const;
  void repeatTick();

  [[nodiscard]] wl_display* display() const noexcept { return m_display; }
  [[nodiscard]] wl_compositor* compositor() const noexcept { return m_compositor; }
  [[nodiscard]] wl_seat* seat() const noexcept { return m_seat; }
  [[nodiscard]] xdg_wm_base* xdgWmBase() const noexcept { return m_xdgWmBase; }
  [[nodiscard]] bool hasXdgShell() const noexcept { return m_xdgWmBase != nullptr; }

  // Buffer scale for wl_surface_set_buffer_scale and HiDPI buffer sizing.
  [[nodiscard]] int32_t effectiveBufferScale() const noexcept;
  // Layout/rendering size in surface-local coordinates.
  [[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>> primaryLogicalSize() const noexcept;

  static void handleGlobal(void* data, wl_registry* registry, std::uint32_t name, const char* interface,
                           std::uint32_t version);
  static void handleGlobalRemove(void* data, wl_registry* registry, std::uint32_t name);
  static void handleOutputMode(void* data, wl_output* output, std::uint32_t flags, std::int32_t width,
                               std::int32_t height, std::int32_t refresh);
  static void handleOutputDone(void* data, wl_output* output);
  static void handleOutputScale(void* data, wl_output* output, std::int32_t factor);
  static void handleOutputName(void* data, wl_output* output, const char* name);
  static void handleOutputDescription(void* data, wl_output* output, const char* description);

private:
  void bindGlobal(wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version);
  void bindOutput(wl_registry* registry, std::uint32_t name, std::uint32_t version);
  [[nodiscard]] const WaylandOutputInfo* primaryOutput() const noexcept;
  void notifyOutputsChanged();

  wl_display* m_display = nullptr;
  wl_registry* m_registry = nullptr;
  wl_compositor* m_compositor = nullptr;
  wl_seat* m_seat = nullptr;
  xdg_wm_base* m_xdgWmBase = nullptr;
  WaylandSeat m_seatHandler;
  std::vector<WaylandOutputInfo> m_outputs;
  std::function<void()> m_outputsChangedCallback;
};
