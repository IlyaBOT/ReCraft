#include "file_dialog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif
void *game_read_small_file(const char *path,size_t limit,size_t *size)
{
    FILE *file;long length;void *bytes;
    *size=0;
#ifdef _WIN32
    wchar_t wide[1024];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,1024)) return NULL;
    file=_wfopen(wide,L"rb");
#else
    file=fopen(path,"rb");
#endif
    if(!file) return NULL;
    if(fseek(file,0,SEEK_END) || (length=ftell(file))<0 || (size_t)length>limit || fseek(file,0,SEEK_SET)) {fclose(file);return NULL;}
    bytes=malloc((size_t)length+1);
    if(!bytes || fread(bytes,1,(size_t)length,file)!=(size_t)length) {free(bytes);fclose(file);return NULL;}
    fclose(file);((unsigned char *)bytes)[length]=0;*size=(size_t)length;return bytes;
}
int game_choose_skin_file(char *path,size_t capacity)
{
    if(!path || capacity<2) return 0;
    path[0]=0;
#ifdef _WIN32
    wchar_t wide[1024]={0};OPENFILENAMEW dialog;
    memset(&dialog,0,sizeof(dialog));dialog.lStructSize=sizeof(dialog);dialog.lpstrFile=wide;dialog.nMaxFile=1024;
    dialog.lpstrFilter=L"Minecraft PNG skin\0*.png\0\0";dialog.lpstrTitle=L"Choose player skin";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog)) return 0;
    return WideCharToMultiByte(CP_UTF8,0,wide,-1,path,(int)capacity,NULL,NULL)>0;
#else
    FILE *pipe;int status;
#ifdef __APPLE__
    pipe=popen("/usr/bin/osascript -e 'POSIX path of (choose file with prompt \"Choose player skin\" of type {\"public.png\"})' 2>/dev/null","r");
#else
    pipe=popen("if command -v zenity >/dev/null 2>&1; then zenity --file-selection --title='Choose player skin' --file-filter='PNG skins | *.png'; else kdialog --getopenfilename . '*.png'; fi 2>/dev/null","r");
#endif
    if(!pipe) return 0;
    if(!fgets(path,(int)capacity,pipe)) path[0]=0;
    status=pclose(pipe);path[strcspn(path,"\r\n")]=0;
    return status==0 && path[0]=='/';
#endif
}
