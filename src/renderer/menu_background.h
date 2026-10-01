#ifndef RECRAFT_MENU_BACKGROUND_H
#define RECRAFT_MENU_BACKGROUND_H
typedef struct MenuBackground {
    unsigned texture;
    int width,height,texture_width,texture_height;
    int screen_width,screen_height,blur,valid;
} MenuBackground;
/* Returns whether a fresh scene is needed, with its viewport dimensions. */
int menu_background_prepare(MenuBackground *bg,int width,int height,int blur,
                            int live,int *scene_width,int *scene_height);
void menu_background_capture(MenuBackground *bg);
void menu_background_draw(const MenuBackground *bg);
void menu_background_clear(MenuBackground *bg);
#endif
