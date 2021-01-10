#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <math.h>

#include <alsa/asoundlib.h>
#include <quiet.h>

#define PCM_DEVICE "default"
#define PROFILE_FILE "quiet-profiles.json"

bool verbose = false;
char device[32] = PCM_DEVICE;
char config_file[255] = PROFILE_FILE;
char exec_file[255] = "";

int main(int argc, char *argv[]) {
    unsigned char i;

    if(argc > 1){
        for(i = 1; i < argc; i++){
            if(strcmp("-v", argv[i]) == 0){
                verbose = true;
                continue;
            } else if(strcmp("-D", argv[i]) == 0){
                if(argc <= i+1){
                    printf("[!] Specify the PCM record device.\n");
                    exit(1);
                }
                strcpy(device, argv[++i]);
                continue;
            } else if(strcmp("-c", argv[i]) == 0){
                if(argc <= i+1){
                    printf("[!] Specify the config file to use.\n");
                    exit(1);
                }
                strcpy(config_file, argv[++i]);
                continue;
            } else if(strcmp("-r", argv[i]) == 0){
                if(argc <= i+1){
                    printf("[!] Specify the config file to use.\n");
                    exit(1);
                }
            } else {
                printf("[!] Parameter %s not recognized.\n", argv[i]);
                exit(1);
            }
        }
    }

    // setting default config
    if(device == NULL){
        strcpy(device, PCM_DEVICE);
    }
    if(config_file == NULL){
        strcpy(config_file, PROFILE_FILE);
    }

    printf("verbose: %d\n", verbose);
    printf("device: %s\n", device);
    printf("config: %s\n", config_file);
    return 0;
}

/* vim: set tabstop=4 expandtab shiftwidth=4 smarttab */
