# Lua LVGL

This module exposes a small LVGL runtime to Lua scripts.

`lvgl` can:
- Initialize LVGL on an existing `esp_lcd` panel handle
- Create screens, containers, and a P1 set of LVGL widgets
- Update widget position, size, text, value, style, and layout via methods
- Subscribe to LVGL widget events (`obj:on / obj:off`) and dispatch them on
  the script task via `lvgl.process_events / lvgl.run`
- Deinitialize the LVGL runtime and release display ownership

## How to call

- Import it with `local lvgl = require("lvgl")`
- Get LCD parameters from `board_manager.get_display_lcd_params(...)` or `lcd.new(...)`
- Call `lvgl.init(panel_handle, io_handle, width, height, panel_if, options)`
- Create a screen with `lvgl.create_screen()` or use `lvgl.screen()`
- Create widgets with `lvgl.label(...)`, `lvgl.button(...)`, `lvgl.bar(...)`,
  `lvgl.slider(...)`, `lvgl.dropdown(...)`, `lvgl.table(...)`, and other P1 constructors
- Operate on widgets via methods: `obj:set_text(...)`, `obj:set_value(...)`,
  `obj:align(...)`, `obj:set_style(...)`, etc.
- Call `lvgl.deinit()` when finished

## Important rules

- **LVGL is a single-script subsystem.** Only one Lua script may drive LVGL
  at a time. See [RFC-single-script-ui.md](RFC-single-script-ui.md) for the
  full rationale and roadmap. Other capabilities must talk to the UI script
  via events (for example through `event_publisher`) instead of calling
  `lvgl.*` directly.
- `lvgl` uses the same display ownership path as the `display` module.
- Do not use `display.init(...)` and `lvgl.init(...)` at the same time.
- The module owns a single LVGL display runtime at a time.
- The Lua script that successfully calls `lvgl.init(...)` owns the LVGL runtime.
- When that owner script exits, the module automatically deinitializes LVGL,
  stops the LVGL task and tick timer, frees the draw buffer, and releases
  display ownership. Calling `lvgl.deinit()` explicitly is still recommended
  when the script is done with the UI.
- `lvgl.init(...)` raises an error if LVGL is already initialized.
- Lua object handles become invalid after `lvgl.deinit()` or after their
  parent object is deleted.
- All operations on widget objects (setters, getters, lifecycle, layout)
  are exposed **only** as methods on the userdata. There is no
  `lvgl.set_text(obj, ...)` style; use `obj:set_text(...)` instead. Each
  widget metatable advertises only methods that make sense for its type,
  so calling an unsupported method raises `attempt to call a nil value`
  immediately at the call site rather than at runtime inside C.
- LVGL event callbacks are dispatched on the **script task** rather than the
  LVGL task. After registering callbacks with `obj:on(...)`, the script must
  drive the event loop via either `lvgl.run()` (blocks until the cap_lua job
  is asked to stop) or repeated `lvgl.process_events([timeout_ms])` calls.
  This is what makes Lua callbacks safe in a single-script subsystem; see
  RFC §4.2 for the reasoning.
- P3 does not yet support input device binding, custom fonts, advanced style
  parts/states, or image decoder/filesystem setup.
- `lvgl.image(...)` only forwards a string `src` to LVGL. Whether that string
  can load depends on firmware FS and decoder configuration outside this module.
- Chinese or other non-ASCII text may not render unless the firmware LVGL font configuration includes a matching font.

## Typical example

```lua
local board_manager = require("board_manager")
local lvgl = require("lvgl")
local delay = require("delay")

local panel_handle, io_handle, width, height, panel_if =
    board_manager.get_display_lcd_params("display_lcd")

lvgl.init(panel_handle, io_handle, width, height, panel_if, {
    buffer_lines = 40,
    tick_ms = 5,
    task_period_ms = 10,
})

local scr = lvgl.create_screen()
lvgl.label(scr, {
    text = "LVGL from Lua",
    align = "top_mid",
    x = 0,
    y = 20,
})

local btn = lvgl.button(scr, {
    text = "OK",
    align = "center",
    w = 120,
    h = 44,
})
btn:set_text("Updated")

local bar = lvgl.bar(scr, {
    align = "bottom_mid",
    x = 0,
    y = -34,
    w = width - 40,
    h = 18,
    min = 0,
    max = 100,
    value = 65,
})
bar:set_value(80)

scr:load()
delay.delay_ms(5000)
lvgl.deinit()
```

## Module-level API

These are the functions hosted on the `lvgl` module table itself. Every
other operation is invoked as a method on a widget userdata.

### `lvgl.init(panel_handle, io_handle, width, height[, panel_if, options])`

Initializes LVGL for an existing LCD panel.

- `panel_handle`: lightuserdata for an `esp_lcd_panel_handle_t`
- `io_handle`: lightuserdata for an `esp_lcd_panel_io_handle_t`; required for IO panels
- `width`: display width
- `height`: display height
- `panel_if`: optional panel interface constant, default `lvgl.PANEL_IF_IO`
- `options`: optional table

Options:
- `buffer_lines`: number of lines in the partial draw buffer, default `40`
- `tick_ms`: LVGL tick interval, default `5`
- `task_period_ms`: LVGL handler task period, default `10`

Returns `true` on success. Raises a Lua error on failure.

### `lvgl.deinit()`

Stops the LVGL task and tick timer, deletes the LVGL display, frees the draw buffer, and releases display ownership.
The same cleanup is also performed automatically when the owner script exits.

Returns `true`.

### `lvgl.screen()`

Returns the current active screen object.

### `lvgl.create_screen()`

Creates and returns a new screen object.

### `lvgl.process_events([timeout_ms])`

Drains queued LVGL events (callbacks registered with `obj:on(...)`) on the
calling script task.

- `timeout_ms = 0` (default): single non-blocking pass; returns immediately
  after draining whatever is currently queued
- `timeout_ms > 0`: keeps draining for up to `timeout_ms` milliseconds. When
  the queue empties the function sleeps in short slices (~20ms) and
  re-checks until either the deadline expires or the cap_lua job is asked
  to stop

Returns the number of callbacks invoked.

### `lvgl.run([opts])`

Convenience wrapper that loops `process_events(period_ms)` until cap_lua
asks the job to stop. Use this as the last call in a UI script:

```lua
-- ... build UI, register callbacks ...
lvgl.run()
```

`opts.period_ms` (default `200`) controls the slice length; smaller values
react to stop signals faster at the cost of slightly more CPU.

Returns the total number of callbacks invoked across all slices.

### Widget factories

Each call below returns a new widget userdata. Use the `opts` table to
configure visual properties at creation time.

- `lvgl.object(parent, opts)`
- `lvgl.container(parent, opts)`
- `lvgl.label(parent, opts)`
- `lvgl.button(parent, opts)` — if `opts.text` is set, a centered child label is created and tracked so `btn:set_text(text)` can update it later
- `lvgl.bar(parent, opts)` — supports `min`, `max`, `value`
- `lvgl.slider(parent, opts)` — supports `min`, `max`, `value`
- `lvgl.image(parent, { src = "..." })`
- `lvgl.line(parent, { points = {{x=0,y=0}, {x=10,y=10}}, y_invert = false })`
- `lvgl.arc(parent, opts)`
- `lvgl.spinner(parent, { anim_ms = 1000, arc_sweep = 60 })`
- `lvgl.scale(parent, opts)`
- `lvgl.checkbox(parent, { text = "...", checked = true })`
- `lvgl.switch(parent, { checked = true })`
- `lvgl.dropdown(parent, { options = {"A", "B"} or "A\nB", selected = 1 })`
- `lvgl.roller(parent, { options = {"A", "B"} or "A\nB", selected = 1 })`
- `lvgl.keyboard(parent, { mode = "text_lower", popovers = false, textarea = obj })`
- `lvgl.list(parent, opts)`
- `lvgl.textarea(parent, opts)`
- `lvgl.table(parent, opts)`

Rows, columns, selected dropdown/roller indexes, and grid cell indexes are
1-based in Lua.

### Common widget options

Most constructors accept:
- `text`
- `x`, `y`
- `w`, `h`
- `align`
- `min`, `max`, `value` for numeric widgets
- style fields: `bg_color`, `text_color`, `border_color`, `bg_opa`, `opa`,
  `radius`, `border_width`, `pad`, `pad_row`, `pad_column`, `line_color`,
  `line_width`, `arc_width`

Color fields accept `0xRRGGBB` numbers or `"#RRGGBB"` strings. Style uses
LVGL selector `0` only.

## Method API (`obj:method(...)`)

Every widget userdata carries a metatable named `lvgl.obj.<type>` whose
`__index` chain provides:

1. **Methods specific to the widget type** (e.g. `slider:set_value(v)`)
2. **Base methods shared by every widget type** (e.g. `obj:align(...)`)

A method that does not exist on a given type is simply absent from its
metatable, so Lua reports `attempt to call a nil value (method 'set_value')`
immediately rather than dispatching to C and failing later.

### Base methods (available on every widget)

| Method                           | Returns      | Notes                                                                                  |
| -------------------------------- | ------------ | -------------------------------------------------------------------------------------- |
| `obj:set_pos(x, y)`              | `true`       |                                                                                        |
| `obj:get_pos()`                  | `x, y`       |                                                                                        |
| `obj:set_size(w, h)`             | `true`       |                                                                                        |
| `obj:get_size()`                 | `w, h`       |                                                                                        |
| `obj:align(name [, x, y])`       | `true`       | `name` ∈ `top_left top_mid top top_right bottom_left bottom_mid bottom bottom_right left_mid left right_mid right center centre` |
| `obj:is_valid()`                 | `boolean`    | `false` after delete / parent delete / deinit                                          |
| `obj:set_style(opts)`            | `true`       | Same fields as the constructor `opts`                                                  |
| `obj:set_flex(opts)`             | `true`       | See "Layout helpers" below                                                             |
| `obj:set_grid(opts)`             | `true`       | See "Layout helpers" below                                                             |
| `obj:set_grid_cell(opts)`        | `true`       | Fields `col / row / col_span / row_span / col_align / row_align`                       |
| `obj:set_scroll(opts)`           | `true`       | See "Layout helpers" below                                                             |
| `obj:on(event, callback)`        | `handle`     | Subscribes a Lua function to an event; see "Events" below                              |
| `obj:off([handle \| event])`     | `count`      | Cancels one subscription (handle), all subscriptions for an event name, or all if no arg |
| `obj:delete()`                   | `true`       | Invalidates the Lua handle                                                             |
| `obj:clean()`                    | `true`       | Deletes all children but keeps `obj`                                                   |

### Type-specific methods

| Method            | Available on                                                          | Notes                                              |
| ----------------- | --------------------------------------------------------------------- | -------------------------------------------------- |
| `set_text(text)`  | `label`, `button`, `checkbox`, `dropdown`, `textarea`, `list_text`, `list_button` |                                                    |
| `set_value(v[,a])`| `bar`, `slider`, `arc`, `scale`, `dropdown`, `roller`, `checkbox`, `switch` | `a` is a boolean for animation when supported       |
| `get_value()`     | same as `set_value`                                                   |                                                    |
| `set_range(l, h)` | `bar`, `slider`, `arc`, `scale`                                       |                                                    |
| `screen:load()`   | `screen` only                                                         | Replaces the previous module-level `lvgl.load(scr)` |
| `list:add_text(t)`        | `list` only                                                   | Returns the new `list_text` userdata                |
| `list:add_button(t[, s])` | `list` only                                                   | Returns the new `list_button` userdata              |
| `table:set_cell(r, c, t)` | `table` only                                                  | 1-based                                            |
| `table:get_cell(r, c)`    | `table` only                                                  | Returns the cell text or `""`                      |

### Layout helpers (called via base methods)

`obj:set_flex(opts)` fields:
- `flow`: `row`, `column`, `row_wrap`, `row_reverse`,
  `row_wrap_reverse`, `column_wrap`, `column_reverse`, `column_wrap_reverse`
- `main`, `cross`, `track`: `start`, `center`, `end`, `space_between`,
  `space_around`, `space_evenly`

`obj:set_grid(opts)` fields:
- `cols`, `rows`: arrays of integers, `"fr"`, or `"content"`
- `col_align`, `row_align`: `start`, `center`, `end`, `stretch`,
  `space_between`, `space_around`, `space_evenly`

Grid descriptor arrays are copied into C-owned memory and remain valid until
the object is deleted, deinitialized, or `obj:set_grid()` is called again.

`obj:set_scroll(opts)` fields:
- `dir`: `none`, `left`, `right`, `top`, `bottom`, `hor`, `ver`, `all`
- `scrollbar`: `off`, `on`, `active`, `auto`
- `snap_x`, `snap_y`: `none`, `start`, `end`, `center`

## Events

Each widget can subscribe Lua callbacks to LVGL events via `obj:on(event,
callback)`. The first call returns a light userdata handle that can be
passed to `obj:off(handle)` to cancel that subscription specifically.

```lua
local btn = lvgl.button(scr, { text = "OK" })
local handle = btn:on("clicked", function()
    print("clicked, value =", slider:get_value())
end)

-- later
btn:off(handle)        -- cancel one subscription
btn:off("clicked")     -- cancel all subscriptions for that event
btn:off()              -- cancel every subscription on btn
```

Supported event names (first version):

| Lua name          | LVGL constant            |
| ----------------- | ------------------------ |
| `"clicked"`       | `LV_EVENT_CLICKED`       |
| `"pressed"`       | `LV_EVENT_PRESSED`       |
| `"released"`      | `LV_EVENT_RELEASED`      |
| `"long_pressed"`  | `LV_EVENT_LONG_PRESSED`  |
| `"value_changed"` | `LV_EVENT_VALUE_CHANGED` |
| `"focused"`       | `LV_EVENT_FOCUSED`       |
| `"defocused"`     | `LV_EVENT_DEFOCUSED`     |
| `"ready"`         | `LV_EVENT_READY`         |
| `"cancel"`        | `LV_EVENT_CANCEL`        |

Callback signature: `function() ... end` (no arguments). Capture context
via Lua closures; the `lv_event_t *` is intentionally NOT exposed because
it lives only inside the LVGL trampoline and cannot survive the deferred
dispatch.

### Driving the event loop

Events queued from the LVGL task are dispatched only when the script calls
`lvgl.run()` or `lvgl.process_events([timeout_ms])`. A typical UI script
does its setup work at the top, then sits in `lvgl.run()` until the
cap_lua job is stopped:

```lua
local scr = lvgl.create_screen()
local btn = lvgl.button(scr, { text = "Tap" })
btn:on("clicked", function()
    print("tap")
end)
scr:load()
lvgl.run()              -- returns when cap_lua stop is requested
```

If the script needs to do its own periodic work in the same loop:

```lua
while not done do
    lvgl.process_events(50)   -- drain for up to 50ms
    -- update non-LVGL state, push frames over MQTT, etc.
end
```

### Coalescing semantics

If the same callback fires repeatedly before `process_events` drains it,
all but the first fire are coalesced into a single dispatched call. This
matches typical UI expectations ("react to the latest event") and prevents
the queue from growing unbounded under fast input. In a future iteration
this may become opt-out via a per-subscription flag.

### Error handling

If a callback raises a Lua error, it is logged at `ESP_LOGE` (tag
`lua_lvgl_evt`) and the script continues. The subscription is **not**
auto-cancelled.

## Implementation notes

- All `lvgl.obj.<type>` metatables share an `__lvgl_obj = true` sentinel
  field. The internal `lua_lvgl_check_ud()` uses this sentinel to recognize
  any LVGL widget without being bound to a single metatable name. This is
  what makes the per-type metatable layout possible while keeping the
  ud-check logic uniform.
- Method tables inherit from a single shared "base methods" table via
  `__index`, so adding a base method only touches one place in
  `src/lua_lvgl_methods.c`.
- The event subsystem in `src/lua_lvgl_events.c` decouples LVGL-task event
  firing from script-task callback dispatch through two queues on
  `s_lvgl`: an event FIFO of pending subscriptions and a deferred
  `pending_unrefs` list of registry refs. The LVGL trampoline never touches
  the Lua state; only `process_events / run / obj:off / deinit` running on
  the script task do `lua_pcall` and `luaL_unref`.
