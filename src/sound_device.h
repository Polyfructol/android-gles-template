
#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*sound_device_read_callback)(float* /* Buf */, int /* nbFrames */, int /* NbChannels */, int /* SamplingRate */, void* /* UserData */);

typedef struct sound_device_t
{
    int Periods;
    int PeriodSize;
    int NbChannels;
    int SampleRate;
} sound_device_t;

sound_device_t *SoundDevice_Create(int FramesPerBuffer, int SampleRate);
void SoundDevice_Destroy(sound_device_t *SoundDevice);
void SoundDevice_Pause(sound_device_t *SoundDevice);
void SoundDevice_Resume(sound_device_t *SoundDevice);
void SoundDevice_SetCallback(sound_device_t *SoundDevice, sound_device_read_callback Callback, void *UserData);

#ifdef __cplusplus
}
#endif