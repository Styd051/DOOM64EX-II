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
// DESCRIPTION: Recorded sound effects.
//
// The cartridge has no such thing: every one of its sound effects is an N64
// sequence, played by the synthesiser like a piece of music. The 2020
// remaster's WAD keeps the same 93 effects as WAV instead, and a synthesiser
// has no notion of "play this recording". This is what plays them.
//
// The two live side by side rather than one replacing the other. A sound is
// played from here if a recording was found for it and by the sequencer if not,
// so the cartridge keeps its sequences untouched -- nothing in it is a RIFF, so
// nothing in it ever registers here.
//
// What is deliberately NOT done here is positioning. OpenAL can place a sound
// in the world and attenuate it by distance itself, and doing so would sound
// nothing like DOOM 64. S_AdjustSoundParams already works out a volume and a
// stereo separation the way the game always has; this module is handed those
// two numbers and does as it is told.
//
//-----------------------------------------------------------------------------

#ifndef __SOUND_SFX__33619784
#define __SOUND_SFX__33619784

#include <prelude.hh>

namespace imp {
  namespace sfx {
    /*!
     * Read every recorded sound effect the loaded IWAD holds.
     *
     * Walks the sounds section and keeps whatever parses as a RIFF/WAVE, filed
     * under its section index -- which is the same number sounds.h uses, in
     * both IWADs. Anything else is left alone for the sequencer.
     *
     * Call after oal::init() and before the first sound is played.
     */
    void init();

    /*! Whether a recording was found for this sound. */
    bool have(int sfx_id);

    /*!
     * Play a recorded sound.
     *
     * @param sfx_id  Index into sounds.h
     * @param origin  The thing the sound comes from, or null for the player.
     *                Held only to be compared with itself -- never read
     *                through, because the thing can be freed while its sound
     *                is still playing.
     * @param volume  0..127, as S_AdjustSoundParams computed it
     * @param pan     32..224, centre 128, as S_AdjustSoundParams computed it
     * @return false if there is no recording for this sound, or no free voice
     */
    bool play(int sfx_id, void* origin, int volume, int pan);

    /*!
     * Silence sounds matching either an id or an origin, exactly as the
     * sequencer's I_StopSound does: every voice playing sfx_id, plus -- when
     * origin is not null -- every voice belonging to it.
     */
    void stop(void* origin, int sfx_id);

    /*! How many voices this module owns. Channel indices run 0..channels()-1. */
    int channels();

    /*! The origin of the voice in this slot, or null if it is idle. */
    void* origin(int slot);

    /*! Forget a voice's origin without silencing it. */
    void forget_origin(int slot);

    /*! Give a playing voice a new volume and pan, in the units play() takes. */
    void update(int slot, int volume, int pan);

    /*! How many voices are sounding. */
    int active();

    /*!
     * The master sound-effect volume, on the same 0..100 scale the menu and
     * s_SfxVol use.
     */
    void set_volume(float volume);

    void pause();
    void resume();

    /*! Silence every voice. */
    void stop_all();

    /*! Release the voices and the recordings. Must run before oal::shutdown. */
    void shutdown();
  }
}

#endif //__SOUND_SFX__33619784
