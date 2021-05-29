// audio_pa.c

#include <portaudio.h>

static PaStream* Stream = NULL;
static PaStreamParameters OutPort;
static PaStreamParameters InPort;

static i32 StereoCallback(const void* InBuffer, void* OutBuffer, unsigned long FramesPerBuffer, const PaStreamCallbackTimeInfo* TimeInfo, PaStreamCallbackFlags Flags, void* UserData) {
  (void)InBuffer; (void)TimeInfo; (void)Flags; (void)UserData;

  AudioEngineProcess(InBuffer, OutBuffer);

  return paContinue;
}

static i32 OpenStream() {
  PaError Err = Pa_OpenStream(
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

  if (Err != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "[PortAudio Error]: %s\n", Pa_GetErrorText(Error));
    return Error;
  }
  Err = Pa_StartStream(Stream);
  if (Err != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "[PortAudio Error]: %s\n", Pa_GetErrorText(Error));
    return Error;
  }
  return NoError;
}

i32 AudioEngineInit(audio_engine* Engine, i32 SampleRate, i32 FramesPerBuffer) {
  PaError Err = Pa_Initialize();
  if (Err != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "[PortAudio Error]: %s\n", Pa_GetErrorText(Err));
    return -1;
  }

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

  fprintf(stdout, "ID | INPUTS | OUTPUTS | SAMPLE RATE | DEVICE NAME\n");
  for (i32 Device = 0; Device < Pa_GetDeviceCount(); ++Device) {
    const PaDeviceInfo* Info = Pa_GetDeviceInfo(Device);
    fprintf(stdout, "%-2i", Device);
    fprintf(stdout, " | %6i | %7i | %11g", Info->maxInputChannels, Info->maxOutputChannels, Info->defaultSampleRate);
    fprintf(stdout, " | %s", Info->name);
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
