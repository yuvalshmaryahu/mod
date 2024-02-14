#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "message_slot.h"

int main(int argc, char* argv[]){
    if(!(argc == 3)){
        
        perror("Invalid Number Of Arguments for Reader");
        exit(1);
    }

    char* path = argv[1];
    int channel = atoi(argv[2]);
    int file_descriptor;
    char buff[BUF_LEN];
    int message_len;

    file_descriptor = open(path, O_RDONLY); /*open file*/
    if(file_descriptor < 0){/*failed to open file*/
        perror("Reader Failed To Open File");
        exit(1);
    }

    if(ioctl(file_descriptor, MSG_SLOT_CHANNEL, channel) < 0){ /*try ioctl*/
        perror("Reader Failed Using ioctl"); /*ioctl failed*/
        exit(1);
    }

    if((message_len = read(file_descriptor, buff, BUF_LEN)) < 0){/*try reading*/
        perror("Reader Failed To Read From File");
        exit(1);
    }

    if(close(file_descriptor) < 0){/*try to close*/
        perror("Reader Failed To Close File");
        exit(1);
    }

    if(write(STDOUT_FILENO, buff, message_len) < 0){ /*try print to stdout*/
        perror("Reader Failed Printing To STDOUT");
        exit(1);
    }

    exit(0);

}