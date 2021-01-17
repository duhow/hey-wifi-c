#include <stdio.h>
#include <unistd.h>

#include "log.h"
#include <alsa/asoundlib.h>
#include <quiet.h>

#define MESSAGE_SIZE 255

typedef unsigned char BYTE;

bool verbose = false;
bool listening = false;
int err = 0;
char pcm_name[32] = "default";
char config_file[255] = "quiet-profiles.json";
char profile[32] = "wave";
char exec_file[255] = "";
char *ssid, *password;

snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t chunk_size = 0;
unsigned int frame_size = 0;
unsigned int buffer_size = 16384;
snd_pcm_format_t format = SND_PCM_FORMAT_FLOAT_LE;
unsigned int rate = 44100;
BYTE channels = 1;
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

    chunk_size = (buffer_size * snd_pcm_format_width(format) / 8 * channels);
    frame_size = (chunk_size / snd_pcm_format_width(format));

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
    log_debug("loading profile %s from file: %s", profile, config_file);
    quiet_decoder_options *decodeOpt =
        quiet_decoder_profile_filename(config_file, profile);

    log_debug("creating a decoder");
    decoder = quiet_decoder_create(decodeOpt, rate);

    writeBuffer = malloc(MESSAGE_SIZE);
    if(writeBuffer == NULL){
        log_error("not enough memory");
        return 1;
    }

    return 0;
}

int extract_data() {
    BYTE msize1, msize2, x;

    msize1 = writeBuffer[0];
    log_debug("SSID has size %d", msize1);

    ssid = malloc(msize1 + 1);
    for(x = 0; x < msize1; x++){ ssid[x] = writeBuffer[1+x]; }
    ssid[msize1] = '\0';

    log_info("SSID: %s", ssid);

    // ------

    msize2 = writeBuffer[msize1 + 1];
    log_debug("Password has size %d", msize2);

    password = malloc(msize2 + 1);
    for(x = 0; x < msize2; x++){ password[x] = writeBuffer[msize1+1 + 1+x]; }
    password[msize2] = '\0';

    log_debug("Password: %s", password);

    //x = 1 + msize1 + 1 + msize2;
    //log_debug("Check is %d %d", writeBuffer[x], writeBuffer[x+1]);

    return 0;
}

int run() {
    err = prepare_alsa();
    if(err > 0){ return err; }

    err = prepare_quiet();
    if(err > 0){ return err; }

    err = 0;
    listening = true;
    log_info("listening");
    while(listening){
        log_debug("reading data");
        err = snd_pcm_readi(handle, audiobuf, frame_size);
        if(err < 0){
            log_error("ALSA error %d: %s", err, snd_strerror(err));
            err = 1;
            break;
        }
        log_trace("consuming to decoder");
        quiet_decoder_consume(decoder, audiobuf, frame_size);

        log_trace("checking if data received");
        ssize_t read = quiet_decoder_recv(decoder, writeBuffer, MESSAGE_SIZE);
        if(read < 0){ continue; }

        log_trace("%.*s\n", read, writeBuffer);

        err = extract_data();
        if(err > 0){ continue; }

        listening = false;
    }
    quiet_decoder_destroy(decoder);
    free(audiobuf);

    if(handle){
        log_debug("closing PCM handle");
        snd_pcm_close(handle);
        handle = NULL;
    }

    log_debug("finished run");
    return err;
}

void show_help(char *prog_name) {
    printf(
        "Usage: %s [OPTIONS]\n"
        "Listen for Quiet encoded message, decode and setup Wireless access.\n"
        "This is a fork of the original Hey Wifi project based in Python.\n"
        "Use https://hey.voicen.io/ to send the WiFi credentials.\n"
        "\n"
        "  -v         Activate verbose (debug) messages\n"
        "  -D %s Specify ALSA PCM capture device\n"
        "  -r %d   Sample rate to use\n"
        "  -c %d       Audio channels from capture device\n"
        "  -B %d   Buffer size to use\n"
        "  -f q.json  %s file\n"
        "  -p %s    Quiet profile to use\n"
        "  -x run.sh  Custom command or script to run\n",
          prog_name, pcm_name, rate, channels, buffer_size, config_file, profile);
}

int main(int argc, char *argv[]) {
    BYTE i;
    log_set_level(LOG_INFO);

    for(i = 1; i < argc; i++){
        if(strcmp("-v", argv[i]) == 0){
            verbose = true;
            log_set_level(LOG_DEBUG);
            continue;
        } else if(strcmp("-vv", argv[i]) == 0){
            verbose = true;
            log_set_level(LOG_TRACE);
            continue;
        } else if(strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0){
            show_help(argv[0]);
            exit(0);
        } else if(strcmp("-r", argv[i]) == 0){
            if(argc <= ++i){
                log_error("Specify the sample rate to use.");
                exit(1);
            }
            rate = atoi(argv[i]);
            continue;
        } else if(strcmp("-c", argv[i]) == 0){
            if(argc <= ++i){
                log_error("Specify the channels to use.");
                exit(1);
            }
            channels = atoi(argv[i]);
            continue;
        } else if(strcmp("-B", argv[i]) == 0){
            if(argc <= ++i){
                log_error("Specify the frame buffer size to use.");
                exit(1);
            }
            buffer_size = atoi(argv[i]);
            continue;
        } else if(strcmp("-D", argv[i]) == 0){
            if(argc <= ++i){
                log_error("Specify the PCM record device.");
                exit(1);
            }
            strcpy(pcm_name, argv[i]);
            continue;
        } else if(strcmp("-f", argv[i]) == 0){
            if(argc <= ++i){
                log_error("Specify the config file to use.");
                exit(1);
            }
            if(access(argv[i], F_OK|R_OK) != 0){
                log_error("Config file does not exist or is not readable");
                exit(1);
            }
            strcpy(config_file, argv[i]);
            continue;
        } else if(strcmp("-p", argv[i]) == 0){
            if(argc <= ++i){
                log_error("Specify the quiet profile to use.");
                exit(1);
            }
            strcpy(profile, argv[i]);
            continue;
        } else if(strcmp("-x", argv[i]) == 0){
            if(argc <= ++i){
                log_error("Specify the executable script or program to use.");
                exit(1);
            }
        } else {
            log_error("Parameter %s not recognized.", argv[i]);
            exit(1);
        }
    }

    log_info("Using [%s] > %d:%d with %d frames, config %s:%s",
             pcm_name, rate, channels, buffer_size, config_file, profile);
    log_debug("starting program");
    return run();
}

/* vim: set tabstop=4 expandtab shiftwidth=4 smarttab */
