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

void freeall(void);
slot_file* search_slot(unsigned int minor);
void delete_slot(slot_file* slot);
channel* search_channel(unsigned long num, slot_file* slot);
void insert_channel(slot_file* slot, channel* chan);
void delete_channels(slot_file* slot);
void copy_buffer_to_channel(ssize_t len, const char* buff, channel* chan);
static slot_file* head;



//searches and return slot , null if not found
slot_file* search_slot(unsigned int minor){
    slot_file* curr = head;
    while(!(curr == NULL)){
        if((curr->minor) == minor){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void freeall(void){
    slot_file* curr = head;
    slot_file* next = NULL;
    while(!(curr == NULL)){
        /*delete all open slots*/
        next = curr->next;
        delete_slot(curr);
        curr = next;
    }
}

//delete message slot and all of its channels
void delete_slot(slot_file* slot){
    delete_channels(slot);
    kfree(slot);
    return;
}
//delete channels of a slot
void delete_channels(slot_file* slot){
    channel* chan = slot->channels;
    channel* next = NULL;
    while(!(chan == NULL)){/*free all open channels*/
        next = chan->next;
        kfree(chan->data);
        kfree(chan);
        chan = next;
    }
    return;
}
//seach channel
channel* search_channel(unsigned long num, slot_file* slot){/*returns a channel struct, NULL if not initialized yet*/
    channel* curr = slot->channels;
    while(!(curr == NULL)){
        if((curr->channel_num) == num){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

//insert channel
void insert_channel(slot_file* slot, channel* chan){/*insert a new channel to channels of slot*/
    channel* curr_head = slot->channels;
    chan->next = curr_head;
    slot->channels = chan;
    return;
}

//copy from user's buffer to kernel's channel
void copy_buffer_to_channel(ssize_t l, const char* buf, channel* c){
    ssize_t i;
    for(i=0; i<l; i++){
        (c->data)[i] = buf[i];
    }
}

/****************-Device functions-***********************/
static int device_open( struct inode* inode,
                        struct file*  file )
{
    //get minor num for the node
    unsigned int curr_minor = iminor(inode);
    slot_file* slot = search_slot(curr_minor);
    slot_file* curr_head ;
    if(slot == NULL){
        /*Create a new slot*/
        slot = (slot_file*) kmalloc(sizeof(slot_file), GFP_KERNEL);
        if (slot == NULL){
            /*kmaloc failed*/
            return -ENOMEM;
        }
        /*Fixing minor num, and count of owners(next it will be increased to 1)*/
        slot->minor = curr_minor;
        slot->channels = NULL;
        slot->owners = 0;
        curr_head = head;
        slot->next = curr_head;
        head = slot;
    }
    /*Increase the counter of owners in this file*/
    slot->owners = slot->owners + 1; 
    return 0;
}

//---------------------------------------------------------------------
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset )
{
  char* buf;
  ssize_t i;

  if(file->private_data == NULL || buffer == NULL){
    return -EINVAL;
  }
  
  if(length == 0 || length > BUF_LEN){ 
    return -EMSGSIZE;
  }

  buf = (char*) kmalloc(BUF_LEN, GFP_KERNEL);
    if(buf == NULL){
        return -ENOMEM;
    }

  for( i = 0; i < length; ++i) {
    if(!(get_user(buf[i], &buffer[i]) == 0)){
        kfree(buf);
        return -EFAULT;
    };
  }

  /*copy from buf to the channel*/
  copy_buffer_to_channel(i, buf, (channel*)file->private_data); 
  /*update to length of current word of the channel*/
  ((channel*)(file->private_data))->word_len = (unsigned long) i; 

  // free buf
  kfree(buf);
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
    unsigned long n ;// length of waord;
    char* text;
    char original_message[BUF_LEN];

    if(((channel*)file->private_data) == NULL || buffer == NULL){/*no open channel for this file or buffer is NULL*/
        return -EINVAL;
    }

    n = ((channel*)(file->private_data))->word_len;
    if(n == 0){
        /*txt is empty*/
        return -EWOULDBLOCK;
    }

    if(length < n){
        /*buffe too short*/
        return -ENOSPC;
    }

    text = ((channel*)(file->private_data))->data;

    for(i=0; i<n; i++){ 
        if(!(get_user(original_message[i], &buffer[i]) == 0)){
            /*Failure reading from buffer to message*/
            return -EFAULT;
        }
    }

    for(i=0; i<n; ++i){
        if(!(put_user(text[i], &buffer[i]) == 0)){
             /*Failue writibng to user's buffer from message*/
            for(j=0; j<i; j++){
                put_user(original_message[j], &buffer[j]);
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
  slot_file* slot;
  channel* ch;


  if(!(MSG_SLOT_CHANNEL == ioctl_command_id) || ioctl_param == 0){
    // wrong argumnets
    return -EINVAL;
  }

  minor = iminor(file->f_inode);
  slot = search_slot(minor);
  ch = search_channel(ioctl_param, slot);

  if(ch == NULL){ 
    /*create new channel*/
    ch = (channel*) kmalloc(sizeof(channel), GFP_KERNEL);
    if(ch == NULL){
        /*Allocating memory Failure*/
        return -ENOMEM;
    }

    ch->channel_num = ioctl_param;
    ch->data = (char*) kmalloc(BUF_LEN, GFP_KERNEL);
    if(ch->data == NULL){
        /*Allocating memory Failure*/
        kfree(ch);
        return -ENOMEM;
    }
    ch->word_len = 0;
    insert_channel(slot, ch);
  }
  /*set the channel to the message slot file*/
  file->private_data = ch; 
  
  return 0;
}


//==================== DEVICE SETUP ============================= //

// The File Operations
struct file_operations Fops = {
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
};


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


static void __exit simple_cleanup(void)
{
    //free the linkedlist
    freeall();
    //unregister as seen in the recitation
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
