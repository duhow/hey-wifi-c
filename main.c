#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include <alsa/asoundlib.h>
#include <quiet.h>

#define PCM_DEVICE "default"
#define PROFILE_FILE "quiet-profiles.json"
#define MESSAGE_SIZE 1024

typedef unsigned char tinyint;

bool verbose = false;
bool running = false;
int err = 0;
char pcm_name[32] = PCM_DEVICE;
char config_file[255] = PROFILE_FILE;
char exec_file[255] = "";

snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t chunk_size = 0;
unsigned int frame_size = 0;
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
unsigned short rate = 44100;
tinyint channels = 2;
char *audiobuf = NULL;
quiet_decoder *decoder;
uint8_t *writeBuffer;

int prepare_alsa() {
    log_debug("opening PCM handle %s", pcm_name);
    err = snd_pcm_open(&handle, pcm_name, SND_PCM_STREAM_CAPTURE, 0);
    if(err < 0){
        log_error("audio open error: %s", snd_strerror(err));
        return 1;
    }

    log_debug("allocating parameter structure");
    err = snd_pcm_hw_params_malloc(&params);
    if(err < 0){
        log_error("error allocating: %s", snd_strerror(err));
        return 1;
    }

    err = snd_pcm_hw_params_any(handle, params);
    if(err < 0){
        log_error("error initializing: %s", snd_strerror(err));
        return 1;
    }

    log_debug("setting access type interleaved");
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(err < 0){
        log_error("error with access type: %s", snd_strerror(err));
        return 1;
    }

    log_debug("setting audio format");
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if(err < 0){
        log_error("error with format %d: %s", format, snd_strerror(err));
        return 1;
    }

    log_debug("setting audio sample rate to %d", rate);
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    if(err < 0){
        log_error("error with sample rate: %s", snd_strerror(err));
        return 1;
    }

    log_debug("setting channels to %d", channels);
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if(err < 0){
        log_error("error with channels: %s", snd_strerror(err));
        return 1;
    }

    log_debug("setting hardware params");
    err = snd_pcm_hw_params(handle, params);
    if(err < 0){
        log_error("error while setting params: %s", snd_strerror(err));
        return 1;
    }

    snd_pcm_hw_params_free(params);

    log_debug("preparing audio interface");
    err = snd_pcm_prepare(handle);
    if(err < 0){
        log_error("error while preparing: %s", snd_strerror(err));
        return 1;
    }

    chunk_size = (1024 * snd_pcm_format_width(format) / 8 * channels);
    frame_size = (chunk_size / 4);

    log_debug("allocating memory %d", chunk_size);
    audiobuf = malloc(chunk_size);
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
    decoder = quiet_decoder_create(decodeOpt, rate);

    writeBuffer = malloc(MESSAGE_SIZE);
    if(writeBuffer == NULL){
        log_error("not enough memory");
        return 1;
    }

    return 0;
}

int run() {
    err = prepare_alsa();
    if(err > 0){ return err; }

    err = prepare_quiet();
    if(err > 0){ return err; }

    err = 0;
    running = true;
    log_info("starting to listen");
    while(running){
        log_debug("reading data");
        err = snd_pcm_readi(handle, audiobuf, frame_size);
        if(err < 0){
            log_error("ALSA error %d: %s", err, snd_strerror(err));
            err = 1;
            running = false;
        }
        log_debug("consuming to decoder");
        quiet_decoder_consume(decoder, audiobuf, frame_size);

        log_debug("checking if data received");
        ssize_t read = quiet_decoder_recv(decoder, writeBuffer, MESSAGE_SIZE);
        if(read < 0){ continue; }

        log_info("%.*s\n", read, writeBuffer);
        running = false;
    }
    quiet_decoder_destroy(decoder);
    free(audiobuf);

    if(handle){
        log_debug("closing PCM handle");
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
        handle = NULL;
    }

    log_debug("finished run");
    return err;
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
