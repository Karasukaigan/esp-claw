/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Method-style API metatables.
 *
 * Each LVGL widget type owns its own metatable named "lvgl.obj.<type>". The
 * metatable carries:
 *   __index      = a per-type "methods" table that itself has a metatable
 *                  whose __index is the shared base methods table; this
 *                  yields a two-level inheritance chain so that all widgets
 *                  inherit the common operations (set_pos / get_pos /
 *                  align / set_style / set_flex / ... / delete / clean /
 *                  is_valid) without duplicating entries.
 *   __gc         = lua_lvgl_obj_gc (shared)
 *   __name       = "lvgl.obj.<type>" (purely informational / debug aid)
 *   __lvgl_obj   = true (sentinel field used by lua_lvgl_check_ud to
 *                  recognize any of the per-type metatables uniformly)
 *
 * Registration is performed once from luaopen_lvgl through
 * lua_lvgl_register_metatables(). After that, lua_lvgl_push_obj() selects
 * the appropriate metatable name via lua_lvgl_metatable_for_type().
 */

#include "lua_lvgl_private.h"

/* --- Base method table: methods every widget gets ---------------------- */

static const luaL_Reg lua_lvgl_base_methods[] = {
    {"set_pos", lua_lvgl_set_pos},
    {"get_pos", lua_lvgl_get_pos},
    {"set_size", lua_lvgl_set_size},
    {"get_size", lua_lvgl_get_size},
    {"align", lua_lvgl_align},
    {"is_valid", lua_lvgl_is_valid},
    {"set_style", lua_lvgl_set_style},
    {"set_flex", lua_lvgl_set_flex},
    {"set_grid", lua_lvgl_set_grid},
    {"set_grid_cell", lua_lvgl_set_grid_cell},
    {"set_scroll", lua_lvgl_set_scroll},
    {"on", lua_lvgl_obj_on},
    {"off", lua_lvgl_obj_off},
    {"delete", lua_lvgl_delete},
    {"clean", lua_lvgl_clean},
    {NULL, NULL},
};

/* --- Per-type extra method tables -------------------------------------- */

static const luaL_Reg lua_lvgl_screen_methods[] = {
    {"load", lua_lvgl_screen_load},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_label_methods[] = {
    {"set_text", lua_lvgl_set_text},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_button_methods[] = {
    {"set_text", lua_lvgl_set_text},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_bar_methods[] = {
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {"set_range", lua_lvgl_set_range},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_slider_methods[] = {
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {"set_range", lua_lvgl_set_range},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_arc_methods[] = {
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {"set_range", lua_lvgl_set_range},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_scale_methods[] = {
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {"set_range", lua_lvgl_set_range},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_checkbox_methods[] = {
    {"set_text", lua_lvgl_set_text},
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_switch_methods[] = {
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_dropdown_methods[] = {
    {"set_text", lua_lvgl_set_text},
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_roller_methods[] = {
    {"set_value", lua_lvgl_set_value},
    {"get_value", lua_lvgl_get_value},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_textarea_methods[] = {
    {"set_text", lua_lvgl_set_text},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_list_methods[] = {
    {"add_text", lua_lvgl_list_add_text},
    {"add_button", lua_lvgl_list_add_button},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_list_text_methods[] = {
    {"set_text", lua_lvgl_set_text},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_list_button_methods[] = {
    {"set_text", lua_lvgl_set_text},
    {NULL, NULL},
};

static const luaL_Reg lua_lvgl_table_methods[] = {
    {"set_cell", lua_lvgl_table_set_cell},
    {"get_cell", lua_lvgl_table_get_cell},
    {NULL, NULL},
};

/* image / line / spinner / keyboard / generic / container expose the base
 * method set only; an empty per-type extra table is enough. */
static const luaL_Reg lua_lvgl_no_extra_methods[] = {
    {NULL, NULL},
};

/* --- Type -> metatable name + extra methods table ---------------------- */

typedef struct {
    lua_lvgl_obj_type_t type;
    const char *mt_name;
    const luaL_Reg *extra_methods;
} lua_lvgl_widget_descriptor_t;

static const lua_lvgl_widget_descriptor_t s_widget_descriptors[] = {
    {LUA_LVGL_OBJ_GENERIC,     "lvgl.obj.generic",     lua_lvgl_no_extra_methods},
    {LUA_LVGL_OBJ_SCREEN,      "lvgl.obj.screen",      lua_lvgl_screen_methods},
    {LUA_LVGL_OBJ_CONTAINER,   "lvgl.obj.container",   lua_lvgl_no_extra_methods},
    {LUA_LVGL_OBJ_LABEL,       "lvgl.obj.label",       lua_lvgl_label_methods},
    {LUA_LVGL_OBJ_BUTTON,      "lvgl.obj.button",      lua_lvgl_button_methods},
    {LUA_LVGL_OBJ_BAR,         "lvgl.obj.bar",         lua_lvgl_bar_methods},
    {LUA_LVGL_OBJ_SLIDER,      "lvgl.obj.slider",      lua_lvgl_slider_methods},
    {LUA_LVGL_OBJ_IMAGE,       "lvgl.obj.image",       lua_lvgl_no_extra_methods},
    {LUA_LVGL_OBJ_LINE,        "lvgl.obj.line",        lua_lvgl_no_extra_methods},
    {LUA_LVGL_OBJ_ARC,         "lvgl.obj.arc",         lua_lvgl_arc_methods},
    {LUA_LVGL_OBJ_SPINNER,     "lvgl.obj.spinner",     lua_lvgl_no_extra_methods},
    {LUA_LVGL_OBJ_SCALE,       "lvgl.obj.scale",       lua_lvgl_scale_methods},
    {LUA_LVGL_OBJ_CHECKBOX,    "lvgl.obj.checkbox",    lua_lvgl_checkbox_methods},
    {LUA_LVGL_OBJ_SWITCH,      "lvgl.obj.switch",      lua_lvgl_switch_methods},
    {LUA_LVGL_OBJ_DROPDOWN,    "lvgl.obj.dropdown",    lua_lvgl_dropdown_methods},
    {LUA_LVGL_OBJ_ROLLER,      "lvgl.obj.roller",      lua_lvgl_roller_methods},
    {LUA_LVGL_OBJ_KEYBOARD,    "lvgl.obj.keyboard",    lua_lvgl_no_extra_methods},
    {LUA_LVGL_OBJ_LIST,        "lvgl.obj.list",        lua_lvgl_list_methods},
    {LUA_LVGL_OBJ_LIST_TEXT,   "lvgl.obj.list_text",   lua_lvgl_list_text_methods},
    {LUA_LVGL_OBJ_LIST_BUTTON, "lvgl.obj.list_button", lua_lvgl_list_button_methods},
    {LUA_LVGL_OBJ_TEXTAREA,    "lvgl.obj.textarea",    lua_lvgl_textarea_methods},
    {LUA_LVGL_OBJ_TABLE,       "lvgl.obj.table",       lua_lvgl_table_methods},
};

#define LUA_LVGL_WIDGET_DESCRIPTOR_COUNT \
    (sizeof(s_widget_descriptors) / sizeof(s_widget_descriptors[0]))

const char *lua_lvgl_metatable_for_type(lua_lvgl_obj_type_t type)
{
    for (size_t i = 0; i < LUA_LVGL_WIDGET_DESCRIPTOR_COUNT; i++) {
        if (s_widget_descriptors[i].type == type) {
            return s_widget_descriptors[i].mt_name;
        }
    }
    /* Defensive fallback: an unknown type still gets a valid metatable so
     * the userdata remains recognizable to lua_lvgl_check_ud. */
    return "lvgl.obj.generic";
}

/* --- Metatable construction helpers ------------------------------------ */

/* Build a per-type "methods" table whose metatable's __index points to the
 * shared base methods table at stack absolute index `base_idx`. The newly
 * built methods table is left at the top of the stack on return.
 *
 * Effect on stack: pushes exactly one new value (the methods table).
 */
static void lua_lvgl_build_methods_table(lua_State *L, int base_idx, const luaL_Reg *extra_methods)
{
    lua_newtable(L);                       /* methods */
    if (extra_methods != NULL) {
        luaL_setfuncs(L, extra_methods, 0);
    }

    lua_newtable(L);                       /* mt_for_methods */
    lua_pushvalue(L, base_idx);            /* base_methods */
    lua_setfield(L, -2, "__index");        /* mt_for_methods.__index = base */
    lua_setmetatable(L, -2);               /* setmetatable(methods, mt_for_methods) */
}

static void lua_lvgl_register_one_metatable(lua_State *L,
                                            int base_idx,
                                            const lua_lvgl_widget_descriptor_t *desc)
{
    if (luaL_newmetatable(L, desc->mt_name) == 0) {
        /* Already exists (e.g. interpreter re-init). Drop the duplicate
         * stack entry and skip rebuilding to avoid trampling on existing
         * Lua references to this metatable. */
        lua_pop(L, 1);
        return;
    }
    /* mt is now at top of stack. */

    lua_lvgl_build_methods_table(L, base_idx, desc->extra_methods);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lua_lvgl_obj_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushstring(L, desc->mt_name);
    lua_setfield(L, -2, "__name");

    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "__lvgl_obj");

    lua_pop(L, 1);
}

void lua_lvgl_register_metatables(lua_State *L)
{
    int base_idx;

    /* Build the base methods table once and keep it on the stack while we
     * register every per-type metatable so each can reference the same
     * shared instance via __index. */
    lua_newtable(L);
    luaL_setfuncs(L, lua_lvgl_base_methods, 0);
    base_idx = lua_gettop(L);

    for (size_t i = 0; i < LUA_LVGL_WIDGET_DESCRIPTOR_COUNT; i++) {
        lua_lvgl_register_one_metatable(L, base_idx, &s_widget_descriptors[i]);
    }

    lua_pop(L, 1);
}
