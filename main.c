#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <getopt.h>
#include <time.h>

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alure.h>
#else
#include <AL/al.h>
#include <AL/alure.h>
#endif

#include "buckle.h"

#define MAX_SOURCES 32
#define DEFAULT_MUTE_KEYCODE 0x46 /* Scroll Lock */
#define MAX_NUM_SAMPLES 3 /* Maximum number of samples per key */


static void usage(char *exe);
static void list_devices(void);
static void test_openal_error(const char *msg);
static double find_key_loc(int code);


/* 
 * Horizontal position on keyboard for each key as they are located on my
 * model-M. I do not like this at all, this whould probably reworked to a
 * proper table or read from a config file or so
 */

static int keyloc[][32] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0x66, 0x68, 0x1c, 0x45, 0x62, 0x37, 0x4a, -1 },
	{ 0x01, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58, 0x6f, 0x6b, 0x6d, 0x47, 0x48, 0x49, 0x4e, -1 },
	{ 0x29, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x4b, 0x4c, 0x4d, -1 },
	{ 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x2b, 0x4f, 0x50, 0x51, 0x60, -1 },
	{ 0x3a, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x1c, 0x52, 0x53, -1 },
	{ 0x2a, 0x56, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, -1 },
	{ 0x1d, 0x7d, 0x5b, 0x38, 0x39, 0x64, 0x61, 0x67, -1 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x6c, 0x6a, -1 },
};


/* 
 * Horizontal position on keyboard of the pragmatic center of the row, since
 * keys come in different sizes and shapes
 */

static double midloc[] = {
	7.5,
	7.5,
	7.5,
	6.5,
	6.5,
	6.5,
	4.5,
};

static int muted = 0;
static ALuint src[MAX_SOURCES] = { 0 };
static ALuint buf[512 * MAX_NUM_SAMPLES] = { 0 };


/*
 * Cmdline options
 */

static int opt_verbose = 0;
static int opt_stereo_width = 50;
static int opt_gain = 100;
static int opt_fallback_sound = 0;
static int opt_mute_keycode = DEFAULT_MUTE_KEYCODE;
static const char *opt_device = NULL;
static const char *opt_path_audio = PATH_AUDIO;

static const char short_opts[] = "d:fg:hlm:Mp:s:v";

static const struct option long_opts[] = {
	{ "device",         required_argument, NULL, 'd' },
	{ "fallback-sound", no_argument,       NULL, 'f' },
	{ "gain",           required_argument, NULL, 'g' },
	{ "help",           no_argument,       NULL, 'h' },
	{ "list-devices",   no_argument,       NULL, 'l' },
	{ "mute-keycode",   required_argument, NULL, 'm' },
	{ "mute",           no_argument,       NULL, 'M' },
	{ "audio-path",     required_argument, NULL, 'p' },
	{ "stereo-width",   required_argument, NULL, 'w' },
	{ "verbose",        no_argument,       NULL, 'v' },
	{ 0, 0, 0, 0 }
};


int main(int argc, char **argv)
{
	int c;
	int rv = EXIT_SUCCESS;
	int idx;

	while( (c = getopt_long(argc, argv, 
				   short_opts, long_opts, &idx)) != -1) {
		switch(c) {
			case 'd':
				opt_device = optarg;
				break;
			case 'f':
				opt_fallback_sound = 1;
				break;
			case 'g':
				opt_gain = atoi(optarg);
				break;
			case 'h':
				usage(argv[0]);
				return 0;
			case 'l':
				list_devices();
				return 0;
			case 'm':
				opt_mute_keycode = strtol(optarg, NULL, 0);
				break;
			case 'M':
				muted = !muted;
				break;
			case 'p':
				opt_path_audio = optarg;
				break;
			case 's':
				opt_stereo_width = atoi(optarg);
				break;
			case 'v':
				opt_verbose++;
				break;
			default:
				usage(argv[0]);
				return 1;
				break;
		}
	}

	if(opt_verbose) {
		open_console();
	}

	srand(time(NULL));

	/* Create openal context */

	ALCdevice *device = NULL;
	ALCcontext *context = NULL;
	ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

	if (!opt_device) {
		opt_device = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
	}

	printd("Opening OpenAL audio device \"%s\"", opt_device);

	device = alcOpenDevice(opt_device);
	if (!device) {
		fprintf(stderr, "unable to open default device\n");
		rv = EXIT_FAILURE;
		goto out;
	}

	context = alcCreateContext(device, NULL);
	if (!alcMakeContextCurrent(context)) {
		fprintf(stderr, "failed to make default context\n");
		return -1;
	}
	test_openal_error("make default context");

	alListener3f(AL_POSITION, 0, 0, 0);
	alListener3f(AL_VELOCITY, 0, 0, 0);
	alListenerfv(AL_ORIENTATION, listenerOri);

	/* Generate sample sources */

	alGenSources(MAX_SOURCES, src);
	test_openal_error("source generation");

	/* Path to data files can also be specified by environment, this is
	 * used by the snap package */

	const char *env_path = getenv("BUCKLESPRING_WAV_DIR");
	if (env_path) {
		opt_path_audio = env_path;
	}

	printd("Using wav dir: \"%s\"\n", opt_path_audio);

	scan(opt_verbose);

out:
	device = alcGetContextsDevice(context);
	alcMakeContextCurrent(NULL);
	if(context) alcDestroyContext(context);
	if(device) alcCloseDevice(device);

	return rv;
}


static void test_openal_error(const char *msg)
{
	ALCenum error = alGetError();
	if (error != AL_NO_ERROR) {
		fprintf(stderr, "OpenAL error at %s\n", msg);
		exit(1);
	}
}


static void usage(char *exe)
{
	fprintf(stderr, 
		"bucklespring version " VERSION "\n"
		"usage: %s [options]\n"
		"\n"
		"options:\n"
		"\n"
		"  -d, --device=DEVICE       use OpenAL audio device DEVICE\n"
		"  -f, --fallback-sound      use a fallback sound for unknown keys\n"
		"  -g, --gain=GAIN           set playback gain [0..100]\n"
		"  -m, --mute-keycode=CODE   use CODE as mute key (default 0x46 for scroll lock)\n"
		"  -M, --mute                start the program muted\n"
		"  -h, --help                show help\n"
		"  -l, --list-devices        list available openAL audio devices\n"
		"  -p, --audio-path=PATH     load .wav files from directory PATH\n"
		"  -s, --stereo-width=WIDTH  set stereo width [0..100]\n"
		"  -v, --verbose             increase verbosity / debugging\n",
		exe
	   );
}


static void list_devices(void)
{
	const ALCchar *devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
	const ALCchar *device = devices, *next = devices + 1;
	size_t len = 0;

	printf("Available audio devices:");
	while (device && *device != '\0' && next && *next != '\0') {
		fprintf(stdout, " \"%s\"", device);
		len = strlen(device);
		device += (len + 1);
		next += (len + 2);
	}
	printf("\n");
}


void printd(const char *fmt, ...)
{
	if(opt_verbose) {

		char buf[256];
		va_list va;

		va_start(va, fmt);
		vsnprintf(buf, sizeof(buf), fmt, va);
		va_end(va);

		fprintf(stderr, "%s\n", buf);
	}
}


/*
 * Find horizontal position of the given key on the keyboard. returns -1.0 for
 * left to 1.0 for right
 */

static double find_key_loc(int code)
{
	int row;
	int col, keycol = 0;

	for(row=0; row<8; row++) {
		for(col=0; col<32; col++) {
			if(keyloc[row][col] == code) keycol = col+1;
			if(keyloc[row][col] == -1) break;
		}
		if(keycol) {
			return ((double) keycol-midloc[row])/(col-midloc[row]);
		}
	}
	return 0;
}


/*
 * To silence play temporarily, press mute key (default ScrollLock) within 2
 * seconds, same to unmute
 */


static void handle_mute_key(int mute_key)
{
	static time_t t_prev;
	static int count = 0;

	if(mute_key) {
		time_t t_now = time(NULL);
		if(t_now - t_prev < 2) {
			count ++;
			if(count == 2) {
				muted = !muted;
				printd("Mute %s", muted ? "enabled" : "disabled");
				count = 0;
			}
		} else {
			count = 1;
		}
		t_prev = t_now;
	} else {
		count = 0;
	}
}


/*
 * Play audio file for given keycode. Wav files are loaded on demand
 */

int play(int code, int press)
{
	static int src_idx = 0;

	printd("key %d/0x%x %sed", code, code, press ? "press" : "release");


	/* Check for mute sequence: ScrollLock down+up+down */

	if(press) {
		handle_mute_key(code == opt_mute_keycode);
	}

	if(muted) {
		return 0;
	}


	/* 0 <= "code" <= 255, which means that 0 <= "idx" <= 511. We could
	 * then add "$rand * 512" to jump to one of the possible samples for
	 * this keycode. */

	int buf_idx = code + press * 256;


	/* Find a valid sample for this key. Assumes that a key either has
	 * MAX_NUM_SAMPLES samples or just one sample. */

	char fname[256];
	int rand_off = rand() % MAX_NUM_SAMPLES;

	snprintf(fname, sizeof fname, "%s/%02x-%d-%d.wav", opt_path_audio, code, press, rand_off);

	if (access(fname, R_OK) != 0) {
		printd("Sample #%d not found, falling back to #0");
		rand_off = 0;
		snprintf(fname, sizeof fname, "%s/%02x-%d-%d.wav", opt_path_audio, code, press, rand_off);
	}


	/* Load sample if not yet loaded */
	
	buf_idx += rand_off * 512;

	if(buf[buf_idx] == 0) {

		printd("Loading audio file \"%s\"", fname);
		buf[buf_idx] = alureCreateBufferFromFile(fname);

		if(buf[buf_idx] == 0) {

			if(opt_fallback_sound) {
				snprintf(fname, sizeof(fname), "%s/%02x-%d-0.wav", opt_path_audio, 0x31, press);
				buf[buf_idx] = alureCreateBufferFromFile(fname);
			} else {
				fprintf(stderr, "Error opening audio file \"%s\": %s\n", fname, alureGetErrorString());
			}
		}
	}


	/* Bind sample buffer to the next available source */

	double x = find_key_loc(code);
	if (opt_stereo_width > 0) {
		alSource3f(src[src_idx], AL_POSITION, -x, 0, (100 - opt_stereo_width) / 100.0);
	}
	alSourcef(src[src_idx], AL_GAIN, opt_gain / 100.0);


	/* Stop playing and bind a new buffer to the source */

	alSourceStop(src[src_idx]);
	test_openal_error("stop");

	alSourcei(src[src_idx], AL_BUFFER, buf[buf_idx]);
	test_openal_error("buffer binding");

	alSourcePlay(src[src_idx]);
	test_openal_error("play");


	/* Round robin to the next source */

	src_idx = (src_idx + 1) % MAX_SOURCES;

	return 0;
}


/*
 * End
 */
