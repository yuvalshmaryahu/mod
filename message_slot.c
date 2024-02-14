#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

#include "message_slot.h"

message_slot_file* search_message_slot(unsigned int minor);
void insert_message_slot(message_slot_file* msg_slot);
void delete_message_slot(message_slot_file* msg_slot);
channel* search_channel(unsigned long num, message_slot_file* msg_slot);
void insert_channel(message_slot_file* msg_slot, channel* chan);
void delete_channels(message_slot_file* msg_slot);
void buff_to_chan(ssize_t len, const char* buff, channel* chan);
static message_slot_file* head;



//searches and return message slot struct, null if not found
message_slot_file* search_message_slot(unsigned int minor){
    message_slot_file* curr = head;
    while(!(curr == NULL)){
        if((curr->minor) == minor){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}


//insert messages slots to the linked list
void insert_message_slot(message_slot_file* msg_slot){
    message_slot_file* curr_head = head;
    msg_slot->next = curr_head;
    head = msg_slot;
    return;
}

//delete message slot and its channels, knowing a message slot with this minor exists
void delete_message_slot(message_slot_file* msg_slot){
    delete_channels(msg_slot);
    kfree(msg_slot);
    return;
}

channel* search_channel(unsigned long num, message_slot_file* msg_slot){/*returns a channel struct, NULL if not initialized yet*/
    channel* curr = msg_slot->channels;
    while(!(curr == NULL)){
        if((curr->channel_num) == num){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void insert_channel(message_slot_file* msg_slot, channel* chan){/*insert a new channel to channels of msg_slot*/
    channel* curr_head = msg_slot->channels;
    chan->next = curr_head;
    msg_slot->channels = chan;
    return;
}

void delete_channels(message_slot_file* msg_slot){
    channel* chan = msg_slot->channels;
    channel* next = NULL;
    while(!(chan == NULL)){/*free all open channels*/
        next = chan->next;
        kfree(chan->data);
        kfree(chan);
        chan = next;
    }
    return;
}

void buff_to_chan(ssize_t len, const char* buff, channel* chan){
    ssize_t i;
    for(i=0; i<len; i++){
        (chan->data)[i] = buff[i];
    }
}

/****************-Device functions-***********************/
static int device_open( struct inode* inode,
                        struct file*  file )
{
    unsigned int minor = iminor(inode);
    message_slot_file* msg_slot = search_message_slot(minor);

    if(msg_slot == NULL){/*first time seeing this msg_slot so create new one*/
        msg_slot = (message_slot_file*) kmalloc(sizeof(message_slot_file), GFP_KERNEL);
        if (msg_slot == NULL){/*failed allocating memory*/
            return -ENOMEM;
        }
        msg_slot->minor = minor;
        msg_slot->channels = NULL;
        msg_slot->owners = 0;
        insert_message_slot(msg_slot);
    }

    msg_slot->owners = msg_slot->owners + 1; /*one more owner added to the file*/

    return 0;
}

//---------------------------------------------------------------------
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset )
{
  char* buff;
  ssize_t i;
  if(file->private_data == NULL || buffer == NULL){/*no open channel for this file or buffer is NULL*/
    return -EINVAL;
  }
  
  if(length == 0 || length > BUF_LEN){ //message lenght invalid
    return -EMSGSIZE;
  }

  buff = (char*) kmalloc(BUF_LEN, GFP_KERNEL);/*allocate a buffer for atomic write*/
    if(buff == NULL){/*failed allocating memory*/
        return -ENOMEM;
    }

  for( i = 0; i < length; ++i) {
    if(!(get_user(buff[i], &buffer[i]) == 0)){
        kfree(buff);
        return -EFAULT;
    };
  }
  buff_to_chan(i, buff, (channel*)file->private_data); /*copy from buff to the channel*/
  ((channel*)(file->private_data))->word_len = (unsigned long) i; /*update to length of current word of the channel*/

  // free buff and return the number of input characters used
  kfree(buff);
  return i;
}


//---------------------------------------------------------------------
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    ssize_t i;
    int j;
    unsigned long n;
    char* word;
    char old_message[BUF_LEN];

    if(((channel*)file->private_data) == NULL || buffer == NULL){/*no open channel for this file or buffer is NULL*/
        return -EINVAL;
    }

    n = ((channel*)(file->private_data))->word_len;
    if(n == 0){/*no word in the channel*/
        return -EWOULDBLOCK;
    }

    if(length < n){/*buffer is too short for the word saved in the channel*/
        return -ENOSPC;
    }

    word = ((channel*)(file->private_data))->data;

    for(i=0; i<n; i++){ /*save current buffer content to keep read atomic*/
        if(!(get_user(old_message[i], &buffer[i]) == 0)){
            /*as said in forum - if we failed here we will fail trying writing to the user's buffer*/
            return -EFAULT;
        }
    }

    for(i=0; i<n; ++i){
        if(!(put_user(word[i], &buffer[i]) == 0)){ /*error while trying to write message to user buffer*/
            for(j=0; j<i; j++){
                /*recover original data to user's buffer,
                as said in the forum: we know for sure put_user will work fine here*/
                put_user(old_message[j], &buffer[j]);
            }
            return -EFAULT;
        }
    }

    return i;
}

//----------------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
  unsigned int minor;
  message_slot_file* msg_slot;
  channel* chan;

  // got wrong parameter - error
  if(!(MSG_SLOT_CHANNEL == ioctl_command_id) || ioctl_param == 0){
    return -EINVAL;
  }

  minor = iminor(file->f_inode);
  msg_slot = search_message_slot(minor);
  chan = search_channel(ioctl_param, msg_slot);

  if(chan == NULL){ /*no channel with this number opened yet, create new one*/
    chan = (channel*) kmalloc(sizeof(channel), GFP_KERNEL);
    if(chan == NULL){/*error when allocating memory*/
        return -ENOMEM;
    }

    chan->channel_num = ioctl_param;
    chan->data = (char*) kmalloc(BUF_LEN, GFP_KERNEL);
    if(chan->data == NULL){/*failed allocating memory*/
        kfree(chan);
        return -ENOMEM;
    }
    chan->word_len = 0;
    insert_channel(msg_slot, chan);
  }

  file->private_data = chan; /*set the channel to the message slot file*/
  
  return 0;
}


//==================== DEVICE SETUP ============================= /*code skeleton taken from 6th recitation and modified*/

// The File Operations
struct file_operations Fops = {
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
};

//---------------------------------------------------------------
// Initialize module
static int __init simple_init(void)
{
  int rc = -1;

  // Register driver capabilities
  rc = register_chrdev( MAJOR_NUM, DEVICE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 ) {
    printk( KERN_ERR "registraion failed \n");
    return -1;
  }
  head = NULL;
  return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    message_slot_file* curr = head;
    message_slot_file* next = NULL;
    while(!(curr == NULL)){/*delete all open message slots*/
        next = curr->next;
        delete_message_slot(curr);
        curr = next;
    }

    // Should always succeed
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================