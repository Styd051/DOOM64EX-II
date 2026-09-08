// -*- mode: c++ -*-
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
// DESCRIPTION: The OpenAL device.
//
// The engine has one synthesiser and no way at all to play a recorded sound.
// That is fine for the cartridge, whose every sound effect is a sequence, and
// useless for the 2020 remaster's WAD, whose 93 are WAV. OpenAL is what will
// play those, and eventually what will place them in the world and put a
// sector's reverb on them through EFX.
//
// This module owns nothing but the device and the context. Whether a sound is
// played through it, and how, belongs elsewhere.
//
//-----------------------------------------------------------------------------

#ifndef __SOUND_OAL__41028756
#define __SOUND_OAL__41028756

#include <prelude.hh>

namespace imp {
  namespace oal {
    /*!
     * Open the default device and make a context current.
     *
     * Failure is not fatal and is not meant to be: a machine with no working
     * audio device should still run the game. The engine reports it and carries
     * on silent.
     *
     * @return true if there is a context to draw sound from
     */
    bool init();

    /*! Whether init() succeeded. */
    bool ready();

    /*! Whether the EFX extension is present -- reverb and filters need it. */
    bool have_efx();

    /*!
     * Asked for the next block of interleaved stereo samples.
     *
     * Called from the streaming thread, never from the game's. It must fill
     * exactly frames * 2 shorts and must not block: everything queued behind it
     * is what stands between the player and a gap in the sound.
     */
    using StreamFill = void (*)(void* user, short* out, int frames);

    /*!
     * Play a continuous stream produced by a callback.
     *
     * This is how the synthesiser reaches the speakers. It used to write into
     * an SDL audio callback; now it fills OpenAL buffers that a small thread
     * keeps cycling, which is what lets sampled sounds share one output with
     * it instead of fighting for the device.
     *
     * @return true if the stream is running
     */
    bool stream_start(StreamFill fill, void* user);

    /*! Stop the stream and release its source. Safe to call twice. */
    void stream_stop();

    /*! Release the context and close the device. Safe to call twice. */
    void shutdown();
  }
}

#endif //__SOUND_OAL__41028756
