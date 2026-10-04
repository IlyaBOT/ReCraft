#ifndef RECRAFT_LANGUAGE_H
#define RECRAFT_LANGUAGE_H
typedef struct LanguageEntry { char code[16],name[160]; } LanguageEntry;
void language_init(void);
void language_shutdown(void);
int language_count(void);
const LanguageEntry *language_at(int index);
int language_select(const char *code);
const char *language_text(const char *key,const char *fallback);
const char *language_caption(const char *english);
const char *language_item(int id,int damage);
#endif
