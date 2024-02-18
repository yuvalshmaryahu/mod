#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

#define BUF_LEN 128
#define MAJOR_NUM 235

#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)

#define DEVICE_NAME "message_slot_device"

#endif
