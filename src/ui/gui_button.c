#include "gui_button.h"
#include "pixel_font.h"
#include "../assets/assets.h"
#include "raylib.h"
#include <math.h>

static void part(Texture2D texture, float sx, int sy, float sw,
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
                  state == GUI_BUTTON_HOVERED || state == GUI_BUTTON_PRESSED ? (Color){255,255,160,255} :
                  (Color){224,224,224,255};
    int row = state == GUI_BUTTON_DISABLED ? 46 : state == GUI_BUTTON_NORMAL ? 66 : 86;
    int left=width/2, font_height=(height*8+10)/20;
    float source_width;
    if (width < 2 || height <= 0) return;
    /* Beta GuiButton: the two outer halves, in logical 20-pixel units.
     * Repeating the atlas center duplicates resource-pack ornaments. */
    source_width=fminf(100,(float)width*10/height);
    part(texture,0,row,source_width,x,y,left,height,white);
    part(texture,200-source_width,row,source_width,x+left,y,width-left,height,white);
    if(font_height<1) font_height=1;
    if(!caption) return;
    DrawMinecraftTextCentered(caption,x+width/2,
        y+(height-font_height)/2,font_height,label,1);
}
