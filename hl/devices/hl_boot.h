#ifndef HL_BOOT_H
#define HL_BOOT_H
#ifdef __cplusplus
extern "C"
{
#endif

    void hl_bootenv_init(void);
    void hl_bootenv_set(const char *name, const char *value);
    const char *hl_bootenv_get(const char *name);
    int hl_get_chipid(char *buf,int len);
    int hl_get_chiptemp(char *buf, int len);
    
#ifdef __cplusplus
}
#endif
#endif