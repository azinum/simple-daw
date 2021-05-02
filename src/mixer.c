// mixer.c

#define MASTER_CHANNEL_INDEX 0
#define MASTER_CHANNEL_COUNT 2

i32 MixerInit(mixer* Mixer, i32 SampleRate, i32 FramesPerBuffer) {
  Mixer->SampleRate = SampleRate;
  Mixer->FramesPerBuffer = FramesPerBuffer;

  bus* Master = &Mixer->Buses[0];
  Master->Buffer = NULL;
  Master->ChannelCount = MASTER_CHANNEL_COUNT;
  Master->Pan = V2(1, 1);
  Master->Db = V2(DB_MIN, DB_MIN);
  Master->DbFade = Master->Db;
  Master->Active = 1;
  Master->Disabled = 0;
  Master->InternalBuffer = 0;

  Mixer->BusCount = 1;

  i32 Index = -1;
  MixerAddBus0(Mixer, 2, NULL, &Index);
  instrument* OscTest = InstrumentCreate(OscTestInit, OscTestFree, OscTestProcess);
  MixerAttachInstrumentToBus(Mixer, Index, OscTest);
  return NoError;
}

i32 MixerAddBus(mixer* Mixer, i32 ChannelCount, float* Buffer) {
  if (Mixer->BusCount < MAX_AUDIO_BUS) {
    bus* Bus = &Mixer->Buses[Mixer->BusCount];
    if (!Buffer) {
      Bus->Buffer = M_Calloc(sizeof(float), ChannelCount * Mixer->FramesPerBuffer);
      Bus->InternalBuffer = 1;
    }
    else {
      Bus->Buffer = Buffer;
      Bus->InternalBuffer = 0;
    }
    Bus->ChannelCount = ChannelCount;
    Bus->Pan = V2(1, 1);
    Bus->Db = V2(DB_MIN, DB_MIN);
    Bus->DbFade = Bus->Db;
    Bus->Active = 1;
    Bus->Disabled = 0;

    Mixer->BusCount++;
  }
  else {
    Assert(0); // TODO(lucas): Handle
  }
  return NoError;
}

// NOTE(lucas): Returns a reference to the bus that you added. It should be noted, however, that the reference is
// not persistant, in other words the reference can change (it can point to a bus which you did not
// initially select). It should therefore be used with caution.
bus* MixerAddBus0(mixer* Mixer, i32 ChannelCount, float* Buffer, i32* BusIndex) {
  bus* Bus = NULL;
  i32 Index = Mixer->BusCount;

  if (MixerAddBus(Mixer, ChannelCount, Buffer) == NoError) {
    Bus = &Mixer->Buses[Index];
    if (BusIndex) {
      *BusIndex = Index;
    }
  }
  else {
    if (BusIndex) {
      *BusIndex = -1;
    }
  }
  return Bus;
}

i32 MixerAttachInstrumentToBus(mixer* Mixer, i32 BusIndex, instrument* Ins) {
  if (BusIndex > MASTER_CHANNEL_INDEX && BusIndex < MAX_AUDIO_BUS) {
    bus* Bus = &Mixer->Buses[BusIndex];
    if (Bus->Ins) {
      InstrumentFree(Bus->Ins);
      Bus->Ins = NULL;
    }
    Bus->Ins = Ins;
  }
  else {
    return Error;
  }
  return NoError;
}

i32 MixerToggleActiveBus(mixer* Mixer, i32 BusIndex) {
  if (BusIndex < Mixer->BusCount) {
    bus* Bus = &Mixer->Buses[BusIndex];
    Bus->Active = !Bus->Active;
  }
  return NoError;
}

i32 MixerClearBuffers(mixer* Mixer) {
  TIMER_START();
  for (i32 BusIndex = 1; BusIndex < Mixer->BusCount; ++BusIndex) {
    bus* Bus = &Mixer->Buses[BusIndex];
    // NOTE(lucas): The buffer is cleared elsewhere if it is not internal
    if (Bus->InternalBuffer) {
      i32 BufferSize = sizeof(float) * Bus->ChannelCount * Mixer->FramesPerBuffer;
      ClearFloatBuffer(Bus->Buffer, BufferSize);
    }
  }
  TIMER_END();
  return NoError;
}

i32 MixerSumBuses(mixer* Mixer, u8 IsPlaying, float* OutBuffer, float* InBuffer) {
  TIMER_START();

  bus* Master = &Mixer->Buses[0];
  Master->Buffer = OutBuffer;
  i32 MasterBufferSize = sizeof(float) * Master->ChannelCount * Mixer->FramesPerBuffer;
  ClearFloatBuffer(Master->Buffer, MasterBufferSize);
  if (!IsPlaying || !Master->Active) {
    return NoError;
  }

  if (InBuffer) {
    float* Iter = &Master->Buffer[0];
    for (i32 FrameIndex = 0; FrameIndex < Mixer->FramesPerBuffer; ++FrameIndex) {
      float Frame0 = *InBuffer++; InBuffer++;
      float Frame1 = Frame0;
      *Iter++ = Frame0;
      *Iter++ = Frame1;
    }
  }

  // CopyFloatBuffer(Master->Buffer, InBuffer, sizeof(float) * Master->ChannelCount * Mixer->FramesPerBuffer);

  // Process all buses
  for (i32 BusIndex = 1; BusIndex < Mixer->BusCount; ++BusIndex) {
    bus* Bus = &Mixer->Buses[BusIndex];
    instrument* Ins = Bus->Ins;
    if (Bus->Active && !Bus->Disabled && Bus->Buffer) {
      if (Ins) {
        if (Ins->Process) {
          Ins->Process(Ins, Bus, Mixer->FramesPerBuffer, Mixer->SampleRate);
        }
      }
    }
  }

  // Sum all buses into the master bus
  for (i32 BusIndex = 1; BusIndex < Mixer->BusCount; ++BusIndex) {
    bus* Bus = &Mixer->Buses[BusIndex];
    instrument* Ins = Bus->Ins;
    (void)Ins;
    if (!Bus->Disabled && Bus->Buffer && Bus->InternalBuffer) {
      float* Iter = &Master->Buffer[0];
      float Frame0 = 0.0f;
      float Frame1 = 0.0f;
      v2 Db = V2(0.0f, 0.0f);
      i32 WindowSize = Mixer->FramesPerBuffer;
      for (i32 FrameIndex = 0; FrameIndex < Mixer->FramesPerBuffer; ++FrameIndex) {
        if (Bus->ChannelCount == 2) {
          Frame0 = Bus->Buffer[FrameIndex * Bus->ChannelCount];
          Frame1 = Bus->Buffer[FrameIndex * Bus->ChannelCount + 1];
        }
        else {
          Frame0 = Frame1 = Bus->Buffer[FrameIndex * Bus->ChannelCount];
        }
        if (Bus->Active) {
          *(Iter++) += Frame0 * Bus->Pan.X * Master->Pan.X;
          *(Iter++) += Frame1 * Bus->Pan.Y * Master->Pan.Y;
        }
        Db.L += 20.0f * Log10(Abs(Frame0));
        Db.R += 20.0f * Log10(Abs(Frame1));
      }
      Db.L /= WindowSize;
      Db.R /= WindowSize;
      Bus->Db = Db;
    }
  }
  TIMER_END();
  return NoError;
}

// TODO(lucas): This is temporary. Replace all of this when proper ui code is implemented.
i32 MixerRender(mixer* Mixer) {
  const i32 TileSize = 32;
  const i32 Gap = 8;

  for (i32 BusIndex = 0; BusIndex < Mixer->BusCount; ++BusIndex) {
    bus* Bus = &Mixer->Buses[BusIndex];
    if (Bus->Active) {
      Bus->DbFade = LerpV2t(Bus->DbFade, Bus->Db, 30 * AudioEngine.DeltaTime);
    }
    { // Draw bus
      v3 P = V3((1 + BusIndex) * (TileSize + Gap), TileSize, 0);
      v2 Size = V2(TileSize, TileSize);
      v3 Color = V3(0, 0, 0);
      Color = (Bus->Active ? V3(0.25f, 0.25f, 0.90f) : V3(0.30f, 0.30f, 0.30f));
      DrawRect(P, Size, Color);
    }
    { // Draw bus volume
      float DbFactorL = 1.0f / (1 + (-Bus->DbFade.L));
      float DbFactorR = 1.0f / (1 + (-Bus->DbFade.R));
      float VolumeBarMaxHeight = 200;
      v3 P = V3((1 + BusIndex) * (TileSize + Gap), TileSize * 2, 0);
      v3 OrigP = P;
      v2 Size = V2(TileSize, 1 + (VolumeBarMaxHeight * DbFactorL));
      v2 OrigSize = Size;
      v2 FullSize = Size;
      Size.W *= 0.4f;
      FullSize.Y = 1 + VolumeBarMaxHeight;
      v3 Color = (Bus->Active ? V3(0.1f, 0.92, 0.1f) : V3(0.10f, 0.5f, 0.10f));
      DrawRect(P, Size, Color);

      P.X += OrigSize.W - Size.W;
      Size.H = 1 + (VolumeBarMaxHeight * DbFactorR);
      DrawRect(P, Size, Color);
      DrawRect(OrigP, FullSize, V3(0.15f, 0.15f, 0.15f));
    }
  }
  return NoError;
}

void MixerFree(mixer* Mixer) {
  for (i32 BusIndex = 1; BusIndex < Mixer->BusCount; ++BusIndex) {
    bus* Bus = &Mixer->Buses[BusIndex];
    if (Bus->InternalBuffer) {
      M_Free(Bus->Buffer, sizeof(float) * Bus->ChannelCount * Mixer->FramesPerBuffer);
    }
    if (Bus->Ins) {
      InstrumentFree(Bus->Ins);
    }
    memset(Bus, 0, sizeof(bus));
  }
}
