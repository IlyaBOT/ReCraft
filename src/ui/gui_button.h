#ifndef RECRAFT_GUI_BUTTON_H
#define RECRAFT_GUI_BUTTON_H

typedef enum GuiButtonState {
    GUI_BUTTON_NORMAL,
    GUI_BUTTON_HOVERED,
    GUI_BUTTON_PRESSED,
    GUI_BUTTON_DISABLED
} GuiButtonState;

void gui_button_draw(int x, int y, int width, int height,
                     const char *caption, GuiButtonState state);

#endif
