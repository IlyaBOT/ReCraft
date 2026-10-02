#ifndef RECRAFT_MUSIC_STREAM_H
#define RECRAFT_MUSIC_STREAM_H
typedef struct MusicStream {
    void *decoder;
    unsigned source,buffers[4];
    int channels,rate,eof;
} MusicStream;
int music_stream_open(MusicStream *stream,const char *path);
void music_stream_update(MusicStream *stream,float volume);
void music_stream_close(MusicStream *stream);
#endif
