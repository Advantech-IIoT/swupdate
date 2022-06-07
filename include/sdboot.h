#ifndef _SDBOOT_H_
#define _SDBOOT_H_

#define EX_SDCARD_ROOT  "/mnt/sdcard"
#define EX_SDCARD_FILE     "/swupdate-image.swu" 


#ifdef  __cplusplus
extern "C" {
#endif

void sdcard_mount(const char *mntpoint);
void sdcard_umount(const char *mntpoint);
bool is_boot_from_SD(void);
bool is_image_exits(void);

#ifdef  __cplusplus
}
#endif

#endif  //_SDBOOT_H_

