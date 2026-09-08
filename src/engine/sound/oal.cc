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

#include <AL/al.h>
#include <AL/alc.h>

#include "oal.hh"
#include "m_misc.h"

namespace {
  ALCdevice*  device_ {};
  ALCcontext* context_ {};
  bool        efx_ {};

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

void imp::oal::shutdown()
{
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
