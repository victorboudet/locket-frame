#pragma once

#include <stdint.h>

// Text overlays drawn into the framebuffer via GUI_Paint. Both functions
// require display_init() to have run (it selects the framebuffer and sets
// scale 6); call them before display_refresh().

// Fill the right 320x480 strip (the part image_render_from_url leaves
// untouched) with when the moment was sent: "sent" / "HH:MM" / "DD Mon YYYY",
// in local time per TZ_OFFSET_S.
void panel_ui_draw_moment_info(uint64_t date_seconds);

// Replace the whole framebuffer with a white error screen showing `msg`,
// so a headless frame doesn't fail silently on a blank panel.
void panel_ui_draw_error(const char* msg);
