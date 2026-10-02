#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
#endif

typedef struct OptionField { const char *name; size_t offset; int min, max; } OptionField;
#define FIELD(n,a,b) { #n, offsetof(UiOptions,n), a, b }
static const OptionField fields[] = {
    FIELD(difficulty,0,3), FIELD(sound_volume,0,100), FIELD(music_volume,0,100),
    FIELD(fancy_graphics,0,1), FIELD(render_distance,2,12), FIELD(smooth_lighting,0,1),
    FIELD(menu_blur,0,1), FIELD(max_framerate,0,144), FIELD(anaglyph,0,0), FIELD(view_bobbing,0,1),
    FIELD(gui_scale,0,3), FIELD(brightness,0,100), FIELD(clouds,0,0),
    FIELD(particles,0,0), FIELD(fullscreen,0,1), FIELD(vsync,0,1), FIELD(mipmaps,0,4),
    FIELD(alternate_blocks,0,0), FIELD(entity_shadows,0,0), FIELD(use_vbo,0,2),
    FIELD(vbo_budget_mb,4,32),
    FIELD(greedy_mesh,0,1), FIELD(chunk_build_budget,1,8), FIELD(fog,0,1),
    FIELD(fancy_leaves,0,1), FIELD(reduced_transparency,0,1), FIELD(dynamic_updates,1,8), FIELD(debug_statistics,0,1)
};
#undef FIELD

int settings_player_name_valid(const char *name)
{
    size_t i,n=name ? strlen(name) : 0;
    if(n<1 || n>16) return 0;
    for(i=0;i<n;++i) if(!((name[i]>='a' && name[i]<='z') ||
        (name[i]>='A' && name[i]<='Z') || (name[i]>='0' && name[i]<='9') || name[i]=='_')) return 0;
    return 1;
}

void settings_load(UiOptions *options, const char *path)
{
    FILE *file=fopen(path,"rb");
    char line[160], key[64];
    int value;
    size_t i;
    if (!file) return;
    while (fgets(line,sizeof(line),file)) {
        if(!strncmp(line,"player_name=",12)) {
            char *name=line+12; name[strcspn(name,"\r\n")]=0;
            if(settings_player_name_valid(name)) strcpy(options->player_name,name);
            continue;
        }
        if (sscanf(line,"%63[^=]=%d",key,&value)!=2) continue;
        for (i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
            const OptionField *field=&fields[i];
            if (strcmp(key,field->name)!=0) continue;
            if (value<field->min || value>field->max) break;
            if (field->offset==offsetof(UiOptions,max_framerate) &&
                value!=0 && value!=25 && value!=30 && value!=50 && value!=60 &&
                value!=75 && value!=90 && value!=120 && value!=144) break;
            if (field->offset==offsetof(UiOptions,vbo_budget_mb) &&
                value!=4 && value!=8 && value!=16 && value!=32) break;
            *(int *)((char *)options+field->offset)=value;
            break;
        }
    }
    fclose(file);
}

int settings_save(const UiOptions *options, const char *path)
{
    char temp[600];
    FILE *file;
    size_t i;
    if (strlen(path)+5>=sizeof(temp)) return 0;
    strcpy(temp,path); strcat(temp,".tmp");
    file=fopen(temp,"wb");
    if (!file) return 0;
    if(!settings_player_name_valid(options->player_name) ||
       fprintf(file,"player_name=%s\n",options->player_name)<0) {
        fclose(file); remove(temp); return 0;
    }
    for (i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
        int value=*(const int *)((const char *)options+fields[i].offset);
        if (fprintf(file,"%s=%d\n",fields[i].name,value)<0) {
            fclose(file); remove(temp); return 0;
        }
    }
    if (fclose(file)!=0) { remove(temp); return 0; }
#ifdef _WIN32
    if (!MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        remove(temp); return 0;
    }
#else
    if (rename(temp,path)!=0) { remove(temp); return 0; }
#endif
    return 1;
}
