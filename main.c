#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include <alsa/asoundlib.h>
#include <quiet.h>

#define PCM_DEVICE "default"
#define PROFILE_FILE "quiet-profiles.json"
#define SAMPLE_RATE 44100

bool verbose = false;
int err = 0;
char pcm_name[32] = PCM_DEVICE;
char config_file[255] = PROFILE_FILE;
char exec_file[255] = "";

static snd_pcm_t *handle;
static snd_pcm_uframes_t chunk_size = 0;
static u_char *audiobuf = NULL;

int prepare_alsa() {
    snd_pcm_info_t *info;
    snd_pcm_info_alloca(&info);

    log_debug("opening PCM handle %s", pcm_name);
    err = snd_pcm_open(&handle, pcm_name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if(err < 0){
        log_error("audio open error: %s", snd_strerror(err));
        return 1;
    }

    log_debug("setting default info");
    err = snd_pcm_info(handle, info);
    if(err < 0){
        log_error("info error: %s", snd_strerror(err));
        return 1;
    }

    log_debug("setting PCM as nonblock");
    err = snd_pcm_nonblock(handle, 1);
    if(err < 0){
        log_error("nonblock setting error: %s", snd_strerror(err));
        return 1;
    }

    chunk_size = 1024;

    log_debug("allocating memory");
    audiobuf = (u_char *)malloc(1024);
    if(audiobuf == NULL){
        log_error("not enough memory");
        return 1;
    }

    log_debug("all done");
    return 0;
}

int prepare_quiet() {
    log_debug("loading options from file: %s", config_file);
    quiet_decoder_options *decodeOpt =
        quiet_decoder_profile_filename(config_file, "audible");

    log_debug("creating a decoder");
    quiet_decoder *decoder = quiet_decoder_create(decodeOpt, SAMPLE_RATE);
}

int run() {
    err = prepare_alsa();
    if(err > 0){ return err; }

    err = prepare_quiet();

    if(handle){
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
        handle = NULL;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    unsigned char i;
    log_set_level(LOG_INFO);

    if(argc > 1){
        for(i = 1; i < argc; i++){
            if(strcmp("-v", argv[i]) == 0){
                verbose = true;
                log_set_level(LOG_TRACE);
                continue;
            } else if(strcmp("-D", argv[i]) == 0){
                if(argc <= i+1){
                    log_error("Specify the PCM record device.");
                    exit(1);
                }
                strcpy(pcm_name, argv[++i]);
                continue;
            } else if(strcmp("-c", argv[i]) == 0){
                if(argc <= i+1){
                    log_error("Specify the config file to use.");
                    exit(1);
                }
                strcpy(config_file, argv[++i]);
                continue;
            } else if(strcmp("-r", argv[i]) == 0){
                if(argc <= i+1){
                    log_error("Specify the executable script or program to use.");
                    exit(1);
                }
            } else {
                log_error("Parameter %s not recognized.", argv[i]);
                exit(1);
            }
        }
    }

    // setting default config
    if(pcm_name == NULL){
        strcpy(pcm_name, PCM_DEVICE);
    }
    if(config_file == NULL){
        strcpy(config_file, PROFILE_FILE);
    }

    printf("verbose: %d\n", verbose);
    printf("device: %s\n", pcm_name);
    printf("config: %s\n", config_file);

    log_debug("starting program");
    return run();
}

/* vim: set tabstop=4 expandtab shiftwidth=4 smarttab */
