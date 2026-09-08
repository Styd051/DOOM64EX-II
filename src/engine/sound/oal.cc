// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2026 Dylan (Styd051)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION: The OpenAL device. See oal.hh.
//
//-----------------------------------------------------------------------------

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

#include "oal.hh"
#include "m_misc.h"

namespace {
  ALCdevice*  device_ {};
  ALCcontext* context_ {};
  bool        efx_ {};

  //
  // Eight buffers of 256 frames: about 46 ms of sound in hand, refilled every
  // 6 ms or so.
  //
  // The size is a compromise and worth stating. Sound effects still go through
  // the synthesiser, so the whole queue is latency between pulling a trigger
  // and hearing it -- too long and the game feels loose. Too short and the
  // thread cannot refill before the queue empties, which is heard as clicking.
  // The old SDL callback ran on about 10 ms of buffer; this holds four times
  // that in reserve while handing over a block every 6.
  //
  constexpr int STREAM_BUFFERS = 8;
  constexpr int STREAM_FRAMES  = 256;
  constexpr int STREAM_RATE    = 44100;

  ALuint             stream_source_ {};
  ALuint             stream_buffers_[STREAM_BUFFERS] {};
  std::thread        stream_thread_;
  std::atomic<bool>  stream_run_ { false };
  imp::oal::StreamFill stream_fill_ {};
  void*              stream_user_ {};
  std::vector<short> stream_pcm_;

  void stream_write_(ALuint buffer)
  {
      stream_fill_(stream_user_, stream_pcm_.data(), STREAM_FRAMES);

      alBufferData(buffer, AL_FORMAT_STEREO16, stream_pcm_.data(),
                   static_cast<ALsizei>(stream_pcm_.size() * sizeof(short)),
                   STREAM_RATE);
  }

  void stream_thread_fn()
  {
      while (stream_run_.load(std::memory_order_relaxed)) {
          ALint processed = 0;
          alGetSourcei(stream_source_, AL_BUFFERS_PROCESSED, &processed);

          while (processed-- > 0) {
              ALuint buffer = 0;
              alSourceUnqueueBuffers(stream_source_, 1, &buffer);
              stream_write_(buffer);
              alSourceQueueBuffers(stream_source_, 1, &buffer);
          }

          //
          // A source that ran dry stops, and stays stopped even once it has
          // buffers again. Nothing restarts it but this.
          //
          ALint state = 0;
          alGetSourcei(stream_source_, AL_SOURCE_STATE, &state);

          if (state != AL_PLAYING) {
              ALint queued = 0;
              alGetSourcei(stream_source_, AL_BUFFERS_QUEUED, &queued);

              if (queued > 0)
                  alSourcePlay(stream_source_);
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
  }

  //
  // Names an alc error rather than printing a number. There are only six, and a
  // reader should not have to look them up.
  //
  const char* alc_error_(ALCdevice* d)
  {
      switch (alcGetError(d)) {
      case ALC_NO_ERROR:          return "no error";
      case ALC_INVALID_DEVICE:    return "invalid device";
      case ALC_INVALID_CONTEXT:   return "invalid context";
      case ALC_INVALID_ENUM:      return "invalid enum";
      case ALC_INVALID_VALUE:     return "invalid value";
      case ALC_OUT_OF_MEMORY:     return "out of memory";
      default:                    return "unknown error";
      }
  }
}

bool imp::oal::init()
{
    if (context_)
        return true;

    if (M_CheckParm("-nosound")) {
        return false;
    }

    device_ = alcOpenDevice(nullptr);

    if (!device_) {
        log::warn("OpenAL: no audio device ({}). The game will be silent.",
                  alc_error_(nullptr));
        return false;
    }

    context_ = alcCreateContext(device_, nullptr);

    if (!context_ || !alcMakeContextCurrent(context_)) {
        log::warn("OpenAL: could not make a context current ({})",
                  alc_error_(device_));

        if (context_) {
            alcDestroyContext(context_);
            context_ = nullptr;
        }

        alcCloseDevice(device_);
        device_ = nullptr;
        return false;
    }

    //
    // EFX is what a sector's reverb and the low-pass filter will be built on --
    // KEX's snd_hardwarereverb and snd_lowpassfilter. Absent on some drivers,
    // so it is reported now rather than discovered later.
    //
    efx_ = alcIsExtensionPresent(device_, "ALC_EXT_EFX") == ALC_TRUE;

    //
    // ALC_ALL_DEVICES_SPECIFIER names the actual output; the older
    // ALC_DEVICE_SPECIFIER only names the driver.
    //
    const char* name = nullptr;

    if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") == ALC_TRUE)
        name = alcGetString(device_, ALC_ALL_DEVICES_SPECIFIER);

    if (!name)
        name = alcGetString(device_, ALC_DEVICE_SPECIFIER);

    int major {}, minor {};
    alcGetIntegerv(device_, ALC_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(device_, ALC_MINOR_VERSION, 1, &minor);

    log::info("OpenAL {}.{} on {}", major, minor, name ? name : "an unnamed device");
    log::info("OpenAL renderer: {}, EFX {}",
              alGetString(AL_RENDERER) ? alGetString(AL_RENDERER) : "?",
              efx_ ? "present" : "absent -- no reverb or filtering");

    return true;
}

bool imp::oal::ready()
{ return context_ != nullptr; }

bool imp::oal::have_efx()
{ return efx_; }

bool imp::oal::stream_start(StreamFill fill, void* user)
{
    if (!context_ || stream_source_ || !fill)
        return false;

    stream_fill_ = fill;
    stream_user_ = user;
    stream_pcm_.assign(static_cast<size_t>(STREAM_FRAMES) * 2, 0);

    alGetError();
    alGenSources(1, &stream_source_);
    alGenBuffers(STREAM_BUFFERS, stream_buffers_);

    if (alGetError() != AL_NO_ERROR) {
        log::warn("OpenAL: could not create the synthesiser's stream");
        stream_source_ = 0;
        return false;
    }

    //
    // The stream is not a thing standing somewhere in the level: it is the
    // synthesiser itself. Marked relative to the listener and placed on top of
    // it, so that distance attenuation and panning -- which sampled sounds will
    // want -- never touch it.
    //
    alSourcei(stream_source_, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(stream_source_, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSourcef(stream_source_, AL_GAIN, 1.0f);

    for (auto buffer : stream_buffers_)
        stream_write_(buffer);

    alSourceQueueBuffers(stream_source_, STREAM_BUFFERS, stream_buffers_);
    alSourcePlay(stream_source_);

    stream_run_.store(true, std::memory_order_relaxed);
    stream_thread_ = std::thread(stream_thread_fn);

    log::info("OpenAL: synthesiser stream running, {} buffers of {} frames ({} ms)",
              STREAM_BUFFERS, STREAM_FRAMES,
              STREAM_BUFFERS * STREAM_FRAMES * 1000 / STREAM_RATE);

    return true;
}

void imp::oal::stream_stop()
{
    if (!stream_source_)
        return;

    stream_run_.store(false, std::memory_order_relaxed);

    if (stream_thread_.joinable())
        stream_thread_.join();

    alSourceStop(stream_source_);

    //
    // Detach every buffer before deleting: a buffer still queued on a source
    // belongs to it, and alDeleteBuffers will refuse.
    //
    alSourcei(stream_source_, AL_BUFFER, 0);
    alDeleteSources(1, &stream_source_);
    alDeleteBuffers(STREAM_BUFFERS, stream_buffers_);

    stream_source_ = 0;
    stream_fill_ = nullptr;
    stream_user_ = nullptr;
}

void imp::oal::shutdown()
{
    stream_stop();

    if (context_) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context_);
        context_ = nullptr;
    }

    if (device_) {
        alcCloseDevice(device_);
        device_ = nullptr;
    }

    efx_ = false;
}
