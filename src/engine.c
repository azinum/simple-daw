// engine.c

#include "mixer.c"
#include "audio_engine.c"
#include "window.c"

static i32 EngineStart() {
  if (WindowOpen(G_WindowWidth, G_WindowHeight, TITLE, G_Vsync, G_FullScreen) == NoError) {
    while (WindowPollEvents() == 0) {
      if (KeyPressed[GLFW_KEY_P]) {
        AudioEngine.IsPlaying = !AudioEngine.IsPlaying;
      }
      if (KeyPressed[GLFW_KEY_E]) {
        AudioEngine.Tick += AudioEngine.SampleRate;
      }
      if (KeyPressed[GLFW_KEY_W]) {
        AudioEngine.Tick -= AudioEngine.SampleRate;
        if (AudioEngine.Tick < 0)
          AudioEngine.Tick = 0;
      }
      WindowSwapBuffers();
      WindowClear(0, 0, 0);
    }
  }
  WindowClose();
  return NoError;
}

i32 EngineInit() {
  AudioEngineInit(SAMPLE_RATE, FRAMES_PER_BUFFER);
  AudioEngineStart(EngineStart);
  return NoError;
}

void EngineFree() {
  AudioEngineTerminate();
}