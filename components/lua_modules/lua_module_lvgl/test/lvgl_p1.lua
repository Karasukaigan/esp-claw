local board_manager = require("board_manager")
local delay = require("delay")
local lvgl = require("lvgl")

local panel_handle, io_handle, width, height, panel_if =
    board_manager.get_display_lcd_params("display_lcd")

lvgl.init(panel_handle, io_handle, width, height, panel_if, {
    buffer_lines = 40,
    tick_ms = 5,
    task_period_ms = 10,
})

local ok, err = pcall(function()
    local scr = lvgl.create_screen()
    scr:set_style({ bg_color = "#0f172a" })

    local root = lvgl.container(scr, {
        align = "top_mid",
        x = 0,
        y = 8,
        w = width - 12,
        h = height - 16,
        bg_color = "#182235",
        bg_opa = 255,
        border_color = "#334155",
        border_width = 1,
        radius = 6,
        pad = 6,
        pad_row = 6,
        pad_column = 6,
    })
    root:set_grid({
        cols = { "fr", "fr", "fr" },
        rows = { 54, 74, 74, "fr" },
        col_align = "stretch",
        row_align = "stretch",
    })

    local dd = lvgl.dropdown(root, {
        options = { "One", "Two", "Three" },
        selected = 2,
        text = "Pick",
        dir = "bottom",
    })
    dd:set_grid_cell({ col = 1, row = 1 })
    dd:set_value(3)

    local roller = lvgl.roller(root, {
        options = "A\nB\nC\nD",
        selected = 2,
        visible_rows = 3,
        mode = "normal",
    })
    roller:set_grid_cell({ col = 2, row = 1 })

    local textarea = lvgl.textarea(root, {
        text = "Lua",
        placeholder = "input",
        one_line = true,
        max_length = 16,
        accepted_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ",
    })
    textarea:set_grid_cell({ col = 3, row = 1 })

    local line = lvgl.line(root, {
        points = {
            { x = 0, y = 0 },
            { x = 50, y = 24 },
            { x = 100, y = 0 },
        },
        line_color = "#38bdf8",
        line_width = 3,
    })
    line:set_grid_cell({ col = 1, row = 2 })

    local arc = lvgl.arc(root, {
        min = 0,
        max = 100,
        value = 45,
        start_angle = 135,
        end_angle = 45,
        rotation = 0,
        mode = "normal",
        arc_width = 8,
        line_color = "#f97316",
    })
    arc:set_grid_cell({ col = 2, row = 2 })
    arc:set_value(70)

    local spinner = lvgl.spinner(root, {
        anim_ms = 1000,
        arc_sweep = 75,
    })
    spinner:set_grid_cell({ col = 3, row = 2 })

    local scale = lvgl.scale(root, {
        mode = "round_inner",
        min = 0,
        max = 100,
        total_ticks = 11,
        major_tick_every = 5,
        label_show = true,
        angle_range = 240,
        rotation = 150,
    })
    scale:set_grid_cell({ col = 1, row = 3 })
    scale:set_value(55)

    local list = lvgl.list(root, {
        bg_color = "#111827",
        border_width = 0,
        radius = 4,
    })
    list:set_grid_cell({ col = 2, row = 3, row_span = 2 })
    list:add_text("List")
    list:add_button("Item 1")
    list:add_button("Item 2")

    local table_obj = lvgl.table(root, {
        rows = 2,
        cols = 2,
        cells = {
            { "A", "B" },
            { "1", "2" },
        },
        column_widths = { 48, 48 },
    })
    table_obj:set_cell(2, 2, "ok")
    local cell = table_obj:get_cell(2, 2)
    table_obj:set_grid_cell({ col = 3, row = 3 })

    local keyboard = lvgl.keyboard(root, {
        mode = "text_lower",
        popovers = false,
        textarea = textarea,
        h = 84,
    })
    keyboard:set_grid_cell({ col = 1, row = 4, col_span = 3 })

    lvgl.checkbox(root, {
        text = "cell " .. cell,
        checked = true,
        text_color = "#e2e8f0",
    })

    scr:load()
    delay.delay_ms(5000)
end)

if not ok then
    error(err)
end
