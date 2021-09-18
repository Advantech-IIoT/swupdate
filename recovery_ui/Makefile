obj-$(CONFIG_RECOVERY_UI)	+= events.o \
	graphics.o graphics_drm.o \
	graphics_fbdev.o default_recovery_ui.o \
	recovery_ui.o resources.o ui.o

EXTRA_LDFLAGS := -lpng -ldrm -lpthread -lc 
EXTRA_CFLAGS := -Irecovery_ui
