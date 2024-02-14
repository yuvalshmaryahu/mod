#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "message_slot.h"

int main(int argc, char* argv[]){
    if(!(argc == 4)){
        perror("Invalid Number Of Arguments for Sender");
        exit(1);
    }
    int file_descriptor;
    char* path = argv[1];
    unsigned int channel = atoi(argv[2]);
    char* message = argv[3];
    
    file_descriptor = open(path, O_WRONLY); /*open file*/
    if(file_descriptor < 0){/*failed to open file*/
        perror("Sender Failed To Open File");
        exit(1);
    }

    if(ioctl(file_descriptor, MSG_SLOT_CHANNEL, channel) < 0){ /*try ioctl*/
        perror("Sender Failed Using ioctl"); /*ioctl failed*/
        exit(1);
    }

    if(write(file_descriptor, message, strlen(message)) < 0){/*try to write*/
        perror("Sender Failed To Write");
        exit(1);
    }

    if(close(file_descriptor) < 0){/*try to close*/
        perror("Sender Failed To Close File");
        exit(1);
    }

    exit(0);
}