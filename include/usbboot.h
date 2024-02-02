#ifndef _USBBOOT_H_
#define _USBBOOT_H_

#define EX_USB_ROOT  "/run/media/sda1"
#define EX_USB_FILE     "/swupdate-image.swu" 


#ifdef  __cplusplus
extern "C" {
#endif

int usb_mount(const char *mntpoint);
void usb_umount(const char *mntpoint);
bool is_usb_image_exits(const char *fname);

#ifdef  __cplusplus
}
#endif

#endif  //_SDBOOT_H_

