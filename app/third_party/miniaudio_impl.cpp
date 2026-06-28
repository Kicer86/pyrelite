// Single translation unit that compiles the miniaudio implementation.
//
// We only use the low-level device (ma_device) playback API: Pyrelite renders
// its own PCM in SoundPlayer, so every higher-level subsystem is disabled to
// keep both compile time and the WebAssembly binary small.
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
