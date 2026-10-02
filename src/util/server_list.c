#include "server_list.h"

#include <stdio.h>
#include <string.h>

int server_address_parse(const char *address,char *host,size_t capacity,uint16_t *port)
{
    const char *start,*end,*suffix=NULL,*p;size_t n;unsigned value=25565,colons=0;
    if(!address||!address[0]||!host||!capacity||!port)return 0;
    for(p=address;*p;++p){if((unsigned char)*p<=32||(unsigned char)*p==127)return 0;if(*p==':')++colons;}
    start=address;end=address+strlen(address);
    if(*start=='['){start++;end=strchr(start,']');if(!end||end==start)return 0;
        if(end[1]){if(end[1]!=':')return 0;suffix=end+2;}if(!memchr(start,':',(size_t)(end-start)))return 0;
    }else {if(strchr(start,'[')||strchr(start,']'))return 0;if(colons==1){end=strchr(start,':');suffix=end+1;}}
    n=(size_t)(end-start);if(!n||n>=capacity)return 0;
    if(suffix){value=0;if(!*suffix)return 0;for(p=suffix;*p;++p){if(*p<'0'||*p>'9'||value>6553)return 0;value=value*10+(unsigned)(*p-'0');if(value>65535)return 0;}if(!value)return 0;}
    memcpy(host,start,n);host[n]=0;*port=(uint16_t)value;return 1;
}

#ifdef _WIN32
#include <windows.h>
#endif

static int field_safe(const char *text, size_t capacity)
{
    size_t i;
    if (!text || !text[0]) return 0;
    for (i = 0; i < capacity && text[i]; ++i)
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] == 127) return 0;
    return i < capacity;
}

void server_list_load(ServerList *list, const char *path)
{
    FILE *file;
    char line[512];
    memset(list, 0, sizeof(*list));
    file = fopen(path, "rb");
    if (!file) return;
    while (list->count < RECRAFT_MAX_SERVERS && fgets(line, sizeof(line), file)) {
        SavedServer entry;
        char *tab1, *tab2, *tab3, *end;
        if (!strchr(line, '\n') && !feof(file)) {
            int ch;
            while ((ch = fgetc(file)) != '\n' && ch != EOF) { }
            continue;
        }
        tab1 = strchr(line, '\t');
        if (!tab1) continue;
        *tab1++ = 0;
        tab2 = strchr(tab1, '\t');
        if (!tab2) continue;
        *tab2++ = 0;
        tab3 = strchr(tab2, '\t');
        if(tab3) *tab3++ = 0;
        end = strpbrk(tab2, "\r\n");
        if (end) *end = 0;
        if(tab3) {end=strpbrk(tab3,"\r\n");if(end)*end=0;}
        if (!field_safe(line, sizeof(entry.name)) ||
            !field_safe(tab1, sizeof(entry.address)) ||
            (strcmp(tab2, "0") != 0 && strcmp(tab2, "1") != 0) ||
            (tab3 && strcmp(tab3,"14") && strcmp(tab3,"47"))) continue;
        memset(&entry, 0, sizeof(entry));
        strcpy(entry.name, line);
        strcpy(entry.address, tab1);
        entry.hide_address = tab2[0] == '1';
        entry.protocol = tab3 && !strcmp(tab3,"47") ? 47 : 14;
        list->entries[list->count++] = entry;
    }
    fclose(file);
}

int server_list_save(const ServerList *list, const char *path)
{
    FILE *file;
    char temp[600];
    size_t i;
    if (!list || !path || strlen(path) + 5 >= sizeof(temp)) return 0;
    strcpy(temp, path);
    strcat(temp, ".tmp");
    file = fopen(temp, "wb");
    if (!file) return 0;
    for (i = 0; i < list->count && i < RECRAFT_MAX_SERVERS; ++i) {
        const SavedServer *entry = &list->entries[i];
        if (!field_safe(entry->name, sizeof(entry->name)) ||
            !field_safe(entry->address, sizeof(entry->address))) continue;
        if (fprintf(file, "%s\t%s\t%d\t%d\n", entry->name, entry->address,
                    entry->hide_address != 0,entry->protocol==47?47:14) < 0) { fclose(file); remove(temp); return 0; }
    }
    if (fclose(file) != 0) { remove(temp); return 0; }
#ifdef _WIN32
    if (!MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        remove(temp);
        return 0;
    }
#else
    if (rename(temp, path) != 0) { remove(temp); return 0; }
#endif
    return 1;
}
