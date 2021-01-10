#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include <alsa/asoundlib.h>
#include <quiet.h>

#define PCM_DEVICE "default"
#define PROFILE_FILE "quiet-profiles.json"
#define SAMPLE_RATE 44100

typedef unsigned char tinyint;

bool verbose = false;
int err = 0;
char pcm_name[32] = PCM_DEVICE;
char config_file[255] = PROFILE_FILE;
char exec_file[255] = "";

static snd_pcm_t *handle;
static snd_pcm_info_t *params;
static snd_pcm_uframes_t chunk_size = 0;
static u_char *audiobuf = NULL;

int prepare_alsa() {
    snd_pcm_info_alloca(&params);

    log_debug("opening PCM handle %s", pcm_name);
    err = snd_pcm_open(&handle, pcm_name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if(err < 0){
        log_error("audio open error: %s", snd_strerror(err));
        return 1;
    }

    log_debug("setting default info");
    err = snd_pcm_info(handle, params);
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

    if(snd_pcm_state(handle) == SND_PCM_STATE_PREPARED){
        log_debug("starting PCM");
        err = snd_pcm_start(handle);
        if(err < 0){
            log_error("failed to start PCM: %s", snd_strerror(err));
            return 1;
        }
    }

    chunk_size = 1024;

    log_debug("allocating memory");
    audiobuf = (u_char *)malloc(chunk_size);
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

    return 0;
}

int run() {
    err = prepare_alsa();
    if(err > 0){ return err; }

    err = prepare_quiet();

    if(handle){
        log_debug("closing PCM handle");
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
        handle = NULL;
    }

    log_debug("finished run");
    return 0;
}

void show_help(char *prog_name) {
    printf("Usage: %s [-v] [-D default] [-f %s] [-x script.sh]\n",
          prog_name, PROFILE_FILE);
}

int main(int argc, char *argv[]) {
    tinyint i;
    log_set_level(LOG_INFO);

    for(i = 1; i < argc; i++){
        if(strcmp("-v", argv[i]) == 0){
            verbose = true;
            log_set_level(LOG_TRACE);
            continue;
        } else if(strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0){
            show_help(argv[0]);
            exit(0);
        } else if(strcmp("-D", argv[i]) == 0){
            if(argc <= i+1){
                log_error("Specify the PCM record device.");
                exit(1);
            }
            strcpy(pcm_name, argv[++i]);
            continue;
        } else if(strcmp("-f", argv[i]) == 0){
            if(argc <= i+1){
                log_error("Specify the config file to use.");
                exit(1);
            }
            i++; // skip to file name arg
            if(access(argv[i], F_OK|R_OK) != 0){
                log_error("Config file does not exist or is not readable");
                exit(1);
            }
            strcpy(config_file, argv[i]);
            continue;
        } else if(strcmp("-x", argv[i]) == 0){
            if(argc <= i+1){
                log_error("Specify the executable script or program to use.");
                exit(1);
            }
        } else {
            log_error("Parameter %s not recognized.", argv[i]);
            exit(1);
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
