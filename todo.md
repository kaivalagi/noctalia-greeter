# noctalia-greeter - Implementation Progress

## Overview
A self-contained greetd greeter with embedded wlroots compositor. No external compositor needed.
C++20 / OpenGL ES 2 / DRM/KMS / libinput.

## Status: Builds successfully — all core systems implemented, ready for testing

---

## ✅ Completed

### Project Structure
- [x] `meson.build` — wlroots 0.19.0 subproject, all sources, assets install
- [x] `subprojects/wlroots.wrap` — pinned wlroots 0.19.0 via git
- [x] Third-party headers (stb, nlohmann json)
- [x] `Justfile` — build, format, clean, install, run targets
- [x] `.lefthook/lefthook.yml` — pre-commit format, pre-push build

### Core
- [x] `core/log.h` / `log.cpp`
- [x] `core/deferred_call.h` / `deferred_call.cpp`

### Compositor (wlroots 0.19.0)
- [x] `compositor/compositor.h` / `.cpp` — backend, renderer, seat, event loop, input handling
- [x] `compositor/output.h` / `.cpp` — per-output rendering, frame callbacks
- [x] `compositor/input.h` / `.cpp` — keyboard/pointer input via libinput
- [x] `compositor/cursor.h` / `.cpp` — cursor management

### Render - GL/EGL
- [x] `render/gl_shared_context.h` / `.cpp`
- [x] `render/core/renderer.h` — Renderer interface
- [x] `render/core/color.h` / `.cpp`
- [x] `render/core/mat3.h` — 3x3 matrix
- [x] `render/core/shader_program.h` / `.cpp`
- [x] `render/core/render_styles.h` — RoundedRectStyle, FillMode, etc.
- [x] `render/render_target.h` / `.cpp`
- [x] `render/render_context.h` / `.cpp`

### Render - Backend
- [x] `render/backend/render_backend.h` / `.cpp`
- [x] `render/backend/gles_render_backend.h` / `.cpp` — GLES2 drawRect, drawImage, drawGlyph
- [x] `render/backend/gles_framebuffer.h` / `.cpp`
- [x] `render/backend/gles_texture_manager.h` / `.cpp`

### Render - Scene Graph
- [x] `render/scene/node.h` / `.cpp` — base Node with layout, hit testing, dirty tracking
- [x] `render/scene/input_area.h` / `.cpp` — pointer/keyboard input capture
- [x] `render/scene/input_dispatcher.h` / `.cpp` — event routing
- [x] `render/scene/rect_node.h`
- [x] `render/scene/text_node.h`
- [x] `render/scene/image_node.h`
- [x] `render/scene/graph_node.h` / `.cpp`

### Render - Text
- [x] `render/text/glyph_registry.h` / `.cpp` — loads tabler.json (6000+ icons)
- [x] `render/text/cairo_text_renderer.h` / `.cpp` — Pango/Cairo text measurement
- [x] `render/text/cairo_glyph_renderer.h` / `.cpp`

### Render - Animation
- [x] `render/animation/animation.h` / `.cpp` — Animation struct, easing functions
- [x] `render/animation/animation_manager.h` / `.cpp` — animation scheduling
- [x] `render/animation/motion_service.h` / `.cpp`

### Render - Core (stubs)
- [x] `render/core/image_file_loader.h` / `.cpp` (stub)
- [x] `render/core/image_decoder.h` / `.cpp` (stub)
- [x] `render/core/async_texture_cache.h` / `.cpp` (stub)
- [x] `render/core/shared_texture_cache.h` / `.cpp` (stub)
- [x] `render/core/blur_cache.h` / `.cpp` (stub)
- [x] `render/core/cached_layer.h` / `.cpp` (stub)

### Render - Shader Programs (stubs)
- [x] All 11 program files created (stub headers/impls)

### UI System
- [x] `ui/palette.h` / `.cpp` — ColorRole enum, palette, ColorSpec
- [x] `ui/style.h` / `.cpp` — Style constants
- [x] `ui/signal.h` — signal/slot

### UI Controls
- [x] `ui/controls/box.h` / `.cpp`
- [x] `ui/controls/button.h` / `.cpp` — variants, hover/press states
- [x] `ui/controls/input.h` / `.cpp` — text input, password mode
- [x] `ui/controls/label.h` / `.cpp`
- [x] `ui/controls/flex.h` / `.cpp` — flexbox layout
- [x] `ui/controls/glyph.h` / `.cpp` — Tabler Icons glyph
- [x] `ui/controls/image.h` / `.cpp` (stub)
- [x] `ui/controls/scroll_view.h` / `.cpp` (stub)
- [x] `ui/controls/spacer.h` / `.cpp`
- [x] `ui/controls/spinner.h` / `.cpp` (stub)
- [x] `ui/controls/separator.h` / `.cpp` (stub)

### Greetd
- [x] `greetd/greetd_client.h` / `.cpp` — full JSON protocol

### Greeter
- [x] `greeter/greeter.h` / `.cpp` — main controller
- [x] `greeter/greeter_surface.h` / `.cpp` — login UI scene per output

### Main
- [x] `src/main.cpp` — entry point

### Assets
- [x] `assets/fonts/tabler.ttf` — Tabler Icons font
- [x] `assets/fonts/tabler.json` — 6000+ glyph mappings
- [x] `assets/fonts/tabler-icons-license.txt`

---

## ❌ Still Needed

### Greeter surface improvements
- [ ] Session command resolution (read from greetd environments file)
- [ ] VT switch on session start
- [ ] Proper cursor shape management
- [ ] Wallpaper/background image support

### Testing
- [ ] Test under greetd with actual DRM/KMS output
- [ ] Verify text rendering with Tabler Icons
- [ ] Verify image loading (WebP/PNG)
- [ ] Verify input handling (keyboard/pointer)

---

## Architecture

### Embedded Compositor
```
wlr_backend (auto: DRM/KMS + libinput)
  ├── wlr_renderer (GLES2)
  ├── wlr_output × N → CompositorOutput → GreeterSurface scene
  ├── wlr_output_layout
  ├── wlr_seat (keyboard + pointer)
  └── wlr_cursor / wlr_xcursor_manager
```

### Main Loop
```
wl_event_loop (wlroots)
  ├── wlr_backend events (new output)
  ├── wlr_output frame events → render
  ├── wlr_seat keyboard/pointer → InputDispatcher → scene nodes
  └── greetd socket poll source
```

### Greetd Flow
1. Connect to `/run/greetd/server.sock`
2. `create_session(username)` → auth message
3. `post_auth_message_response(password)` → success/error
4. On success: `start_session(command)` → VT switch → exec session
