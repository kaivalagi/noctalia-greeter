Multi-Monitor NVIDIA Proprietary Driver Fix
=============================================

## Root Cause

The greeter compositor (`noctalia_compositor.c`) required **all** connected outputs to be
successfully enabled before it would launch the greeter client. The output-readiness check
in `schedule_launch()` and `try_launch_greeter()` used `all_outputs_active()`, which returns
true only when every `wlr_output` in the list has `active == true`.

On systems using the NVIDIA proprietary driver (nvidia-drm), the driver's EGL implementation
cannot create swapchains for multiple DRM outputs simultaneously once one output is already
active. This manifests as:

```
[ERROR] [types/output/swapchain.c:109] Swapchain for output 'DP-1' failed test
[ERROR] [../src/compositor/noctalia_compositor.c:666] failed to enable output DP-1
```

When the first output (e.g. HDMI-A-1) initialises render and becomes active, subsequent
outputs (DP-1, DP-2) fail their swapchain allocation because NVIDIA's EGL/GLES does not
support multi-output context sharing in the same way Mesa does. The compositor output
remains inactive.

Because the launch gate required `all_outputs_active()` to be true, the greeter **never
launched**. greetd would eventually time out and kill the session — no login screen
appeared.

On Nouveau (open-source NVIDIA) and Mesa drivers (Intel/AMD), swapchain allocation
succeeds on all outputs, so all outputs become active and the greeter launches normally.

The existing workaround — setting `[output].name = "DP-1"` in `greeter.toml` — bypassed
the issue because `choose_outputs()` then enables only the named output, and the launch
gate checks `any_output_active()` instead of `all_outputs_active()` for the pinned-output
code path.

## Fix

File: `src/compositor/noctalia_compositor.c`

### Change 1 — `schedule_launch()` (was line 808)

Replaced the branched check (`all_outputs_active` for the multi-output path,
`any_output_active` for the pinned-output path) with a single `any_output_active` check:

```c
// Before:
  if (use_all_outputs(server)) {
    if (!all_outputs_active(server)) {
      return;
    }
  } else if (!any_output_active(server)) {
    return;
  }

// After:
  if (!any_output_active(server)) {
    return;
  }
```

### Change 2 — `try_launch_greeter()` (was line 1256)

Same pattern — flatten the branched check:

```c
// Before:
  if (use_all_outputs(server)) {
    if (!all_outputs_active(server)) {
      schedule_launch(server);
      return;
    }
  } else if (!any_output_active(server)) {
    return;
  }

// After:
  if (!any_output_active(server)) {
    schedule_launch(server);
    return;
  }
```

### Change 3 — Removed `all_outputs_active()` (was line 787)

The function became dead code after changes 1 and 2. Removed to avoid compiler warnings.

## Reasoning

The compositor should not require every connected output to be operational before
launching the greeter. A display that fails to initialise for driver-specific reasons
should be silently skipped — the greeter client connects to the compositor and only sees
the outputs that are actually active. Outputs that failed remain disabled and the KMS
state handles them gracefully on their own.

This matches the principle that a login greeter should appear on *whatever works* rather
than failing entirely when one output has driver-specific issues. Users who want to pin
to a specific connector can still use `[output].name = "..."` in `greeter.toml`.

## Tested

- Builds and links with GCC 15.2.0, wlroots 0.20, no warnings or errors.
- Functional validation needed on the affected hardware (NVIDIA proprietary driver with
  multiple monitors, no `[output].name` configured). The greeter should now appear on
  whatever outputs initialise successfully.
