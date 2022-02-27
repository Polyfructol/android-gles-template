#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <dlfcn.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <android/log.h>

#include "sound_device_opensl.h"

#define DEBUG 1
#define LOG_TAG "EmptyApp"

#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif

static void AudioPlayerCallback(SLAndroidSimpleBufferQueueItf BufferQueue, void *Context)
{
    sound_device_impl_t *SoundDevice = (sound_device_impl_t*)Context;
    sound_device_t *SoundDeviceInterface = (sound_device_t*)SoundDevice;

    if (SoundDevice->Callback)
        SoundDevice->Callback((float*)SoundDevice->Buffer,
                SoundDeviceInterface->PeriodSize,
                SoundDeviceInterface->NbChannels,
                SoundDeviceInterface->SampleRate,
                SoundDevice->UserData);
    
    SLresult Result;
    Result = (*BufferQueue)->Enqueue(BufferQueue, SoundDevice->Buffer, SoundDevice->BufferSize);
    assert(Result == SL_RESULT_SUCCESS);
}

sound_device_t *SoundDevice_Create(int FramesPerBuffer, int SampleRate)
{
    sound_device_impl_t *SoundDevice = (sound_device_impl_t *)calloc(1, sizeof(sound_device_impl_t));
    sound_device_t *SoundDeviceInterface = (sound_device_t*)SoundDevice;

    ALOGV("FramesPerBuffer: %d\n", FramesPerBuffer);
    ALOGV("SampleRate: %d\n", SampleRate);

    SoundDeviceInterface->Periods = 1;
    SoundDeviceInterface->NbChannels = 2;
    SoundDeviceInterface->PeriodSize = FramesPerBuffer;
    SoundDeviceInterface->SampleRate = SampleRate;

    SoundDevice->BufferSize = SoundDeviceInterface->PeriodSize * SoundDeviceInterface->NbChannels * sizeof(float);
    SoundDevice->Buffer = (unsigned char*)calloc(SoundDevice->BufferSize, 1);

    SLresult Result;

    // Create Engine
    SLObjectItf EngineObject;
    Result = slCreateEngine(&EngineObject, 0, NULL, 0, NULL, NULL);
    assert(Result == SL_RESULT_SUCCESS);

    Result = (*EngineObject)->Realize(EngineObject, SL_BOOLEAN_FALSE);
    assert(Result == SL_RESULT_SUCCESS);

    SoundDevice->Engine = EngineObject;

    SLEngineItf Engine;    
    Result = (*EngineObject)->GetInterface(EngineObject, SL_IID_ENGINE, &Engine);
    assert(Result == SL_RESULT_SUCCESS);
    
    // Create OutputMix
    SLObjectItf OutputMix;
    Result = (*Engine)->CreateOutputMix(Engine, &OutputMix, 0, 0, 0);
    assert(Result == SL_RESULT_SUCCESS);
    Result = (*OutputMix)->Realize(OutputMix, SL_BOOLEAN_FALSE);
    assert(Result == SL_RESULT_SUCCESS);

    SoundDevice->OutputMix = OutputMix;

    // Create Audio Player
    SLDataLocator_AndroidSimpleBufferQueue LocatorAndroid;
    LocatorAndroid.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    LocatorAndroid.numBuffers = (SLuint32)SoundDeviceInterface->Periods;

    SLAndroidDataFormat_PCM_EX DataFormat;
    DataFormat.formatType     = SL_ANDROID_DATAFORMAT_PCM_EX;
    DataFormat.numChannels    = 2;
    DataFormat.sampleRate     = SoundDeviceInterface->SampleRate * 1000;
    DataFormat.bitsPerSample  = 32;
    DataFormat.containerSize  = 32;
    DataFormat.channelMask    = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    DataFormat.endianness     = SL_BYTEORDER_LITTLEENDIAN;
    DataFormat.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
    
    SLDataSource DataSource;
    DataSource.pLocator = &LocatorAndroid;
    DataSource.pFormat = &DataFormat;
    
    SLDataLocator_OutputMix Locator_OutputMix;
    Locator_OutputMix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    Locator_OutputMix.outputMix = OutputMix;
    
    SLDataSink DataSink;
    DataSink.pLocator = &Locator_OutputMix;
    DataSink.pFormat = NULL;

    #define NB_INTERFACES 1
    SLInterfaceID InterfacesIDs[NB_INTERFACES] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    SLboolean Required[NB_INTERFACES] = { SL_BOOLEAN_TRUE };

    // https://developer.android.com/ndk/guides/audio/opensl/opensl-prog-notes
    // TODO: Destroy audio player on pause
    SLObjectItf AudioPlayerObject;
    Result = (*Engine)->CreateAudioPlayer(Engine, &AudioPlayerObject, &DataSource, &DataSink, NB_INTERFACES, InterfacesIDs, Required);
    assert(Result == SL_RESULT_SUCCESS);
    Result = (*AudioPlayerObject)->Realize(AudioPlayerObject, SL_BOOLEAN_FALSE);
    assert(Result == SL_RESULT_SUCCESS);

    SoundDevice->AudioPlayerObject = AudioPlayerObject;

    // Create BufferQueue
    SLAndroidSimpleBufferQueueItf BufferQueue;
    Result = (*AudioPlayerObject)->GetInterface(AudioPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &BufferQueue);
    assert(Result == SL_RESULT_SUCCESS);

    Result = (*BufferQueue)->RegisterCallback(BufferQueue, AudioPlayerCallback, SoundDevice);
    assert(Result == SL_RESULT_SUCCESS);

    // Start audio player
    SLPlayItf AudioPlayer;
    Result = (*AudioPlayerObject)->GetInterface(AudioPlayerObject, SL_IID_PLAY, &AudioPlayer);
    assert(Result == SL_RESULT_SUCCESS);
    
    SoundDevice->AudioPlayer = AudioPlayer;

    // Enqueue buffers
    for (unsigned int i = 0; i < SoundDeviceInterface->Periods; ++i)
    {
        Result = (*BufferQueue)->Enqueue(BufferQueue,
                SoundDevice->Buffer,
                SoundDevice->BufferSize);
        assert(Result == SL_RESULT_SUCCESS);
    }

    Result = (*AudioPlayer)->SetPlayState(AudioPlayer, SL_PLAYSTATE_PLAYING);
    assert(Result == SL_RESULT_SUCCESS);

    return SoundDeviceInterface;
}

void SoundDevice_Pause(sound_device_t *SoundDeviceInterface)
{
    if (SoundDeviceInterface == NULL)
        return;
    
    sound_device_impl_t *SoundDevice = (sound_device_impl_t*)SoundDeviceInterface;
    
    SLresult Result = (*SoundDevice->AudioPlayer)->SetPlayState(SoundDevice->AudioPlayer, SL_PLAYSTATE_PAUSED);
    assert(Result == SL_RESULT_SUCCESS);
}

void SoundDevice_Resume(sound_device_t *SoundDeviceInterface)
{
    if (SoundDeviceInterface == NULL)
        return;
    
    sound_device_impl_t *SoundDevice = (sound_device_impl_t*)SoundDeviceInterface;
    
    SLresult Result = (*SoundDevice->AudioPlayer)->SetPlayState(SoundDevice->AudioPlayer, SL_PLAYSTATE_PLAYING);
    assert(Result == SL_RESULT_SUCCESS);
}

void SoundDevice_Destroy(sound_device_t *SoundDeviceInterface)
{
    if (SoundDeviceInterface == NULL)
        return;
    
    sound_device_impl_t *SoundDevice = (sound_device_impl_t*)SoundDeviceInterface;

    (*SoundDevice->AudioPlayerObject)->Destroy(SoundDevice->AudioPlayerObject);
    (*SoundDevice->OutputMix)->Destroy(SoundDevice->OutputMix);
    (*SoundDevice->Engine)->Destroy(SoundDevice->Engine);

    free(SoundDevice->Buffer);
    free(SoundDevice);
}

void SoundDevice_SetCallback(sound_device_t *SoundDeviceInterface, sound_device_read_callback Callback, void *UserData)
{
    sound_device_impl_t *SoundDevice = (sound_device_impl_t*)SoundDeviceInterface;

    ALOGV("Set audio callback: %p\n", (void*)Callback);
    SoundDevice->Callback = Callback;
    SoundDevice->UserData = UserData;
}
