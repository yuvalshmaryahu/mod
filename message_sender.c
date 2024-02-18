#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "message_slot.h"

int main(int argc, char* argv[]){
    if(!(argc == 4)){
        perror("Error Sender - Invalid Number Of Arguments");
        exit(1);
    }
    int fd;
    char* path_to_file = argv[1];
    unsigned int channel = atoi(argv[2]);
    char* message = argv[3];
    
    fd = open(path_to_file, O_WRONLY); 
    if(fd < 0){
        perror("Error Sender -cannot open file");
        exit(1);
    }

    if(ioctl(fd, MSG_SLOT_CHANNEL, channel) < 0){ 
        perror("Error Sender - IOCTL"); 
        exit(1);
    }

    if(write(fd, message, strlen(message)) < 0){
        perror("Error Sender - cannot write");
        exit(1);
    }

    if(close(fd) < 0){
        perror("Error Sender - cannot close File");
        exit(1);
    }

    exit(0);
}
