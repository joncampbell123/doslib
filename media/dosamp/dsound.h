
#if defined(HAS_DSOUND)
extern HMODULE             dsound_dll;
extern unsigned char       dsound_tried;

extern HRESULT (WINAPI *__DirectSoundCreate)(LPGUID lpGuid,LPDIRECTSOUND* ppDS,LPUNKNOWN pUnkOuter);

void free_dsound(void);
void dsound_atexit(void);
int init_dsound(void);
#endif


