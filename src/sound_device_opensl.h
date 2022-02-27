
#include "sound_device.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Forward declaration
struct SLObjectItf_;
typedef const struct SLObjectItf_ * const * SLObjectItf;

struct SLPlayItf_;
typedef const struct SLPlayItf_ * const * SLPlayItf;

typedef struct sound_device_impl_t
{
    sound_device_t Interface;

    volatile sound_device_read_callback  Callback;
    void *UserData;

	SLObjectItf Engine;
	SLObjectItf OutputMix;
	SLObjectItf AudioPlayerObject;
    SLPlayItf AudioPlayer;

	int BufferSize;
	unsigned char *Buffer;
} sound_device_impl_t;

#ifdef __cplusplus
}
#endif
