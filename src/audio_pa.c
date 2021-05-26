// audio_pa.c
// portaudio audio backend

#include <portaudio.h>

static PaStream* Stream = NULL;
static PaStreamParameters OutPort;
static PaStreamParameters InPort;

static i32 StereoCallback(const void* InBuffer, void* OutBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo* TimeInfo, PaStreamCallbackFlags Flags, void* UserData) {
  (void)InBuffer; (void)TimeInfo; (void)Flags; (void)UserData;

  TIMER_START();

  audio_engine* Engine = &AudioEngine;
  mixer* Mixer = &Engine->Mixer;

  Engine->Out = (float*)OutBuffer;
  Engine->In = (float*)InBuffer;

  if (Mixer->Active) {
    MixerClearBuffers(Mixer);
    MixerSumBuses(Mixer, Engine->IsPlaying, Engine->Out, Engine->In);
  }
  else {
    ClearFloatBuffer(Engine->Out, sizeof(float) * MASTER_CHANNEL_COUNT * FramesPerBuffer);
  }
  if (AudioEngine.IsPlaying) {
    const float DeltaTime = (1.0f / Engine->SampleRate) * FramesPerBuffer;
    Engine->DeltaTime = DeltaTime;
    Engine->Time += DeltaTime;
    Engine->Tick += FramesPerBuffer;
  }
  TIMER_END();
  return paContinue;
}

static i32 OpenStream() {
  PaError Error = Pa_OpenStream(
    &Stream,
#if 0
    &InPort,
#else
    NULL,
#endif
    &OutPort,
    AudioEngine.SampleRate,
    AudioEngine.FramesPerBuffer,
    paNoFlag,
    StereoCallback,
    NULL
  );

  if (Error != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "[PortAudio Error]: %s\n", Pa_GetErrorText(Error));
    return Error;
  }
  Pa_StartStream(Stream);
  return NoError;
}

i32 AudioEngineInit(i32 SampleRate, i32 FramesPerBuffer) {
  PaError Err = Pa_Initialize();
  if (Err != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "[PortAudio Error]: %s\n", Pa_GetErrorText(Err));
    return -1;
  }

  AudioEngine.SampleRate = SampleRate;
  AudioEngine.FramesPerBuffer = FramesPerBuffer;
  AudioEngine.Tick = 0;
  AudioEngine.Out = NULL;
  AudioEngine.In = NULL;
  AudioEngine.Time = 0.0f;
  AudioEngine.DeltaTime = 0.0f;
  AudioEngine.IsPlaying = 1;
  AudioEngine.Initialized = 1;

  i32 InputDevice = Pa_GetDefaultInputDevice();
  InPort.device = InputDevice;
  InPort.channelCount = 2;
  InPort.sampleFormat = paFloat32;
  InPort.suggestedLatency = Pa_GetDeviceInfo(InPort.device)->defaultLowInputLatency;
  InPort.hostApiSpecificStreamInfo = NULL;

  i32 OutputDevice = Pa_GetDefaultOutputDevice();
  OutPort.device = OutputDevice;
  OutPort.channelCount = 2;
  OutPort.sampleFormat = paFloat32;
  OutPort.suggestedLatency = Pa_GetDeviceInfo(OutPort.device)->defaultLowOutputLatency;
  OutPort.hostApiSpecificStreamInfo = NULL;

  for (i32 Device = 0; Device < Pa_GetDeviceCount(); ++Device) {
    const PaDeviceInfo* Info = Pa_GetDeviceInfo(Device);
    fprintf(stdout, "%-2i | %s", Device, Info->name);
    if (OutputDevice == Device) {
      fprintf(stdout, " [SELECTED]");
    }
    fprintf(stdout, "\n");
  }

  if ((Err = Pa_IsFormatSupported(&InPort, &OutPort, SampleRate)) != paFormatIsSupported) {
    Assert(0);
  }
  fprintf(stdout, "Using portaudio audio backend\n");
  return NoError;
}

i32 AudioEngineStart(callback Callback) {
  i32 Result = OpenStream();
  if (Result != NoError) {
    return Result;
  }

  if (!AudioEngine.Initialized) {
    return Error;
  }
  if (Callback) {
    Callback(&AudioEngine);
  }

  return NoError;
}

void AudioEngineTerminate() {
  Pa_CloseStream(Stream);
  Pa_Terminate();
}