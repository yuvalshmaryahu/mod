#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "message_slot.h"
/*
gcc -O3 -Wall -std=c11 -o message_reader message_reader.c 
gcc -O3 -Wall -std=c11 -o message_sender message_sender.c
make
sudo insmod message_slot.ko
sudo mknod /dev/slot0 c 235 0
sudo chmod 666 /dev/slot0 
./message_sender /dev/slot0 1 "Hello world"
./message_reader /dev/slot0 1
./message_sender /dev/slot0 1 "Dores"
./message_reader /dev/slot0 1
./message_sender /dev/slot0 3 "another channel"
./message_reader /dev/slot0 3
*/

int main(int argc, char* argv[]){
    if(!(argc == 3)){
        perror("Error Reading - Too many arguments");
        exit(1);
    }

    char* path_to_file = argv[1];
    int channel = atoi(argv[2]);
    int fd;
    char buff[BUF_LEN];
    int msg_len;

    fd = open(path_to_file, O_RDONLY); 
    if(fd < 0){
        perror("Error Reading - file not opened");
        exit(1);
    }

    if(ioctl(fd, MSG_SLOT_CHANNEL, channel) < 0){ 
        perror("Error Reading - IOCTL"); 
        exit(1);
    }

    if((msg_len = read(fd, buff, BUF_LEN)) < 0){
        perror("Error Reading - reading failed");
        exit(1);
    }

    if(close(fd) < 0){
        perror("Error Reading - cannot close file");
        exit(1);
    }

    if(write(STDOUT_FILENO, buff, msg_len) < 0){ 
        perror("Error Reading - printing message not succesfull");
        exit(1);
    }

    exit(0);

}
