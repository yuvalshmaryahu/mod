#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

#define BUF_LEN 128
#define MAJOR_NUM 235

#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)

#define DEVICE_NAME "message_slot_device"

typedef struct slot_file
{
    unsigned int minor;
    unsigned long owners;
    struct slot_file* next;  
    struct channel* channels;    
} slot_file;

typedef struct channel
{
    unsigned long channel_num;
    unsigned long word_len;
    char* data;
    struct channel* next;
} channel;

#endif
