#include "gui_button.h"
#include "pixel_font.h"
#include "../assets/assets.h"
#include "raylib.h"

static void part(Texture2D texture, int sx, int sy, int sw,
                 int dx, int dy, int dw, int dh, Color tint)
{
    Rectangle src = {(float)sx,(float)sy,(float)sw,20};
    Rectangle dst = {(float)dx,(float)dy,(float)dw,(float)dh};
    Vector2 origin = {0,0};
    DrawTexturePro(texture,src,dst,origin,0,tint);
}

void gui_button_draw(int x, int y, int width, int height,
                     const char *caption, GuiButtonState state)
{
    Texture2D texture = assets_get_texture(ASSET_GUI_WIDGETS);
    Color white = {255,255,255,255};
    Color label = state == GUI_BUTTON_DISABLED ? (Color){160,160,160,255} :
                  state == GUI_BUTTON_HOVERED ? (Color){255,255,160,255} :
                  (Color){240,240,240,255};
    int row = state == GUI_BUTTON_DISABLED ? 46 : state == GUI_BUTTON_NORMAL ? 66 : 86;
    int at = 4;
    if (width < 8 || height <= 0) return;
    part(texture,0,row,4,x,y,4,height,white);
    while (at < width-4) {
        int n = width-4-at;
        if (n > 192) n = 192;
        part(texture,4,row,n,x+at,y,n,height,white);
        at += n;
    }
    part(texture,196,row,4,x+width-4,y,4,height,white);
    DrawMinecraftTextCentered(caption,x+width/2,
        y+(height-12)/2+(state==GUI_BUTTON_PRESSED),12,label,1);
}
