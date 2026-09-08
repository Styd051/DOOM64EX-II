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
// DESCRIPTION: Recorded sound effects. See sfx.hh.
//
//-----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstring>

#include <AL/al.h>

#include "sfx.hh"
#include "oal.hh"
#include "sounds.h"
#include "i_system.h"
#include "con_console.h"
#include <wad.hh>

namespace {
  //
  // Thirty-two at once. The sequencer has sixty-four channels but spends
  // several of them on one sound -- a sequence is a set of tracks -- whereas a
  // recording is one voice. Doom 64 never has thirty-two distinct things making
  // a noise in the same tic.
  //
  constexpr int NUM_VOICES = 32;

  struct Voice {
      ALuint   source {};
      int      sfx_id { -1 };
      void*    origin {};
      bool     playing {};
      bool     looping {};
      uint64_t age {};      //!< when it started, for stealing the oldest
  };

  Voice          voices_[NUM_VOICES];
  Vector<ALuint> buffers_;          //!< by sound id; 0 where there is no recording
  float          volume_ { 80.0f }; //!< the menu's 0..100
  uint64_t       clock_ {};
  bool           ready_ {};

  //
  // The two sounds the game starts once and stops by hand. Everything else is
  // fired and forgotten.
  //
  // It has to be said here because the files cannot say it: not one of the
  // remaster's 93 recordings carries a smpl chunk, so there is no loop point in
  // any of them to read. The cartridge had no such problem -- a sequence loops
  // because its own data says to.
  //
  bool loops_(int sfx_id)
  { return sfx_id == sfx_electric || sfx_id == sfx_quake; }

  //
  // A RIFF/WAVE reader, walked chunk by chunk rather than assuming the usual
  // 44-byte preamble. All 93 of the remaster's are in fact plain 22050 Hz mono
  // PCM laid out exactly that way, but a WAD is a thing users replace lumps in.
  //
  struct Wave {
      ALenum       format {};
      ALsizei      rate {};
      const char*  data {};
      ALsizei      size {};
  };

  uint32_t le32_(const unsigned char* p)
  { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

  uint16_t le16_(const unsigned char* p)
  { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

  bool parse_wave_(const String& bytes, Wave& out)
  {
      const auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
      const size_t len = bytes.size();

      if (len < 12 || std::memcmp(p, "RIFF", 4) || std::memcmp(p + 8, "WAVE", 4))
          return false;

      uint16_t channels {}, bits {}, encoding {};
      bool have_fmt {}, have_data {};
      size_t pos = 12;

      while (pos + 8 <= len) {
          const unsigned char* id = p + pos;
          size_t size = le32_(p + pos + 4);
          size_t body = pos + 8;

          if (size > len - body)
              size = len - body;

          if (!std::memcmp(id, "fmt ", 4) && size >= 16) {
              encoding = le16_(p + body);
              channels = le16_(p + body + 2);
              out.rate = static_cast<ALsizei>(le32_(p + body + 4));
              bits     = le16_(p + body + 14);
              have_fmt = true;
          } else if (!std::memcmp(id, "data", 4)) {
              out.data  = bytes.data() + body;
              out.size  = static_cast<ALsizei>(size);
              have_data = true;
          }

          // Chunks are padded to an even length; the pad byte is not counted.
          pos = body + size + (size & 1);
      }

      if (!have_fmt || !have_data || !out.size)
          return false;

      if (encoding != 1)  // WAVE_FORMAT_PCM
          return false;

      if (channels == 1 && bits == 8)       out.format = AL_FORMAT_MONO8;
      else if (channels == 1 && bits == 16) out.format = AL_FORMAT_MONO16;
      else if (channels == 2 && bits == 8)  out.format = AL_FORMAT_STEREO8;
      else if (channels == 2 && bits == 16) out.format = AL_FORMAT_STEREO16;
      else return false;

      return true;
  }

  //
  // The sequencer sends a sound's volume as MIDI controller 7, and General MIDI
  // reads that controller as an attenuation of 40*log10(cc/127) dB -- an
  // amplitude of (cc/127) squared. A recording handed the plain linear number
  // would sit well above everything the synthesiser plays, so it goes through
  // the same curve. Whether it lands at exactly the level FluidSynth's own
  // volume handling produces is a question only the ear can settle.
  //
  float gain_(int volume)
  {
      //
      // The 0.925 is the scaling I_SetSoundVolume applies before handing the
      // sequencer its volume. The two have to agree, or the WAD's effects would
      // sit at a different level from the cartridge's.
      //
      float cc = volume * (volume_ * 0.925f) / 127.0f;

      cc = std::max(0.0f, std::min(127.0f, cc)) / 127.0f;

      return cc * cc;
  }

  //
  // S_AdjustSoundParams gives a separation of 32..224 around a centre of 128,
  // and the sequencer halves it into a MIDI pan of 16..112 around 64 -- three
  // quarters of the way to either side, never hard left or right. Placing the
  // source at the same three quarters keeps a sound as wide as it has always
  // been.
  //
  // The source is relative to the listener with its rolloff switched off, so
  // its position moves the sound between the speakers and does nothing else.
  // Distance attenuation stays where it belongs, in S_AdjustSoundParams.
  //
  void set_pan_(ALuint source, int pan)
  {
      float x = (pan - 128) / 128.0f;

      x = std::max(-1.0f, std::min(1.0f, x));

      alSource3f(source, AL_POSITION, x, 0.0f,
                 -std::sqrt(std::max(0.0f, 1.0f - x * x)));
  }

  void release_(Voice& v)
  {
      alSourcei(v.source, AL_BUFFER, 0);
      v.playing = false;
      v.sfx_id  = -1;
      v.origin  = nullptr;
      v.looping = false;
  }

  //
  // Hand back a voice whose sound has ended. A one-shot source stops on its own
  // and nothing tells us; this is where we notice.
  //
  // One voice at a time, because the caller that matters is origin(), which
  // S_UpdateSounds asks about every voice on every tic. Sweeping all
  // thirty-two on each of those would be a thousand driver calls a tic to
  // learn what one call answers.
  //
  void reap_one_(Voice& v)
  {
      if (!v.playing)
          return;

      ALint state = 0;
      alGetSourcei(v.source, AL_SOURCE_STATE, &state);

      if (state == AL_STOPPED)
          release_(v);
  }

  void reap_()
  {
      for (auto& v : voices_)
          reap_one_(v);
  }

  Voice* claim_()
  {
      for (auto& v : voices_)
          if (!v.playing)
              return &v;

      //
      // All busy. Take the oldest one that will end by itself; a looping sound
      // is playing because the game asked for it to keep playing, and stealing
      // it would leave the plasma rifle silent with no way to notice.
      //
      Voice* oldest {};

      for (auto& v : voices_)
          if (!v.looping && (!oldest || v.age < oldest->age))
              oldest = &v;

      if (oldest) {
          alSourceStop(oldest->source);
          alSourcei(oldest->source, AL_BUFFER, 0);
      }

      return oldest;
  }
}

void imp::sfx::init()
{
    if (ready_ || !oal::ready())
        return;

    auto section = wad::list_section(wad::Section::sounds);

    if (section.empty())
        return;

    buffers_.assign(section.size(), 0);

    size_t found {};
    size_t bytes {};

    for (auto& lump_ptr : section) {
        auto& lump = *lump_ptr;

        //
        // The index is what sounds.h will ask for, so a lump that somehow sits
        // outside the section it was listed in is dropped rather than written
        // past the end of the table.
        //
        if (lump.section_index() >= buffers_.size())
            continue;

        auto data = lump.read_bytes();

        Wave wave;

        //
        // Anything that is not a recording is left for the sequencer, and that
        // is the whole of the cartridge: its sound effects are sequences and
        // its music is MIDI, so none of them parse here and none of them are
        // taken away from it.
        //
        if (!parse_wave_(data, wave))
            continue;

        ALuint buffer = 0;

        alGetError();
        alGenBuffers(1, &buffer);
        alBufferData(buffer, wave.format, wave.data, wave.size, wave.rate);

        if (alGetError() != AL_NO_ERROR) {
            if (buffer)
                alDeleteBuffers(1, &buffer);
            continue;
        }

        buffers_[lump.section_index()] = buffer;
        found++;
        bytes += static_cast<size_t>(wave.size);
    }

    if (!found)
        return;

    alGetError();

    for (auto& v : voices_) {
        alGenSources(1, &v.source);
        alSourcei(v.source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(v.source, AL_ROLLOFF_FACTOR, 0.0f);
        alSource3f(v.source, AL_POSITION, 0.0f, 0.0f, -1.0f);
    }

    if (alGetError() != AL_NO_ERROR) {
        CON_Warnf("sfx: could not create the sound-effect voices\n");
        return;
    }

    ready_ = true;

    I_Printf("%d recorded sound effects, %d KB, on %d voices\n",
             (int)found, (int)(bytes / 1024), NUM_VOICES);
}

bool imp::sfx::have(int sfx_id)
{
    return ready_
        && sfx_id >= 0
        && static_cast<size_t>(sfx_id) < buffers_.size()
        && buffers_[sfx_id] != 0;
}

bool imp::sfx::play(int sfx_id, void* origin, int volume, int pan)
{
    if (!have(sfx_id))
        return false;

    reap_();

    Voice* v = claim_();

    if (!v)
        return false;

    v->sfx_id  = sfx_id;
    v->origin  = origin;
    v->looping = loops_(sfx_id);
    v->playing = true;
    v->age     = clock_++;

    alSourcei(v->source, AL_BUFFER, static_cast<ALint>(buffers_[sfx_id]));
    alSourcei(v->source, AL_LOOPING, v->looping ? AL_TRUE : AL_FALSE);
    alSourcef(v->source, AL_GAIN, gain_(volume));
    set_pan_(v->source, pan);
    alSourcePlay(v->source);

    return true;
}

void imp::sfx::stop(void* origin, int sfx_id)
{
    if (!ready_)
        return;

    for (auto& v : voices_) {
        if (!v.playing)
            continue;

        //
        // The same two tests the sequencer's I_StopSound makes: by sound, and
        // -- when there is one -- by the thing making it. P_RemoveMobj stops a
        // thing's sounds by passing its origin with a sound id of zero.
        //
        if (v.sfx_id == sfx_id || (origin && v.origin == origin)) {
            alSourceStop(v.source);
            release_(v);
        }
    }
}

int imp::sfx::channels()
{ return ready_ ? NUM_VOICES : 0; }

void* imp::sfx::origin(int slot)
{
    if (!ready_ || slot < 0 || slot >= NUM_VOICES)
        return nullptr;

    reap_one_(voices_[slot]);

    return voices_[slot].playing ? voices_[slot].origin : nullptr;
}

void imp::sfx::forget_origin(int slot)
{
    if (ready_ && slot >= 0 && slot < NUM_VOICES)
        voices_[slot].origin = nullptr;
}

void imp::sfx::update(int slot, int volume, int pan)
{
    if (!ready_ || slot < 0 || slot >= NUM_VOICES)
        return;

    Voice& v = voices_[slot];

    if (!v.playing)
        return;

    alSourcef(v.source, AL_GAIN, gain_(volume));
    set_pan_(v.source, pan);
}

int imp::sfx::active()
{
    if (!ready_)
        return 0;

    reap_();

    int n {};

    for (auto& v : voices_)
        if (v.playing)
            n++;

    return n;
}

//
// The voices already sounding are left where they are rather than recomputed,
// because the volume S_AdjustSoundParams worked out for each is not kept
// anywhere. The next tic's S_UpdateSounds sets it right for anything with an
// origin, and a sound without one is over before the slider has stopped moving.
//
void imp::sfx::set_volume(float volume)
{
    volume_ = volume;
}

void imp::sfx::pause()
{
    if (!ready_)
        return;

    for (auto& v : voices_)
        if (v.playing)
            alSourcePause(v.source);
}

void imp::sfx::resume()
{
    if (!ready_)
        return;

    for (auto& v : voices_) {
        if (!v.playing)
            continue;

        ALint state = 0;
        alGetSourcei(v.source, AL_SOURCE_STATE, &state);

        if (state == AL_PAUSED)
            alSourcePlay(v.source);
    }
}

void imp::sfx::stop_all()
{
    if (!ready_)
        return;

    for (auto& v : voices_) {
        alSourceStop(v.source);
        release_(v);
    }
}

void imp::sfx::shutdown()
{
    if (!ready_)
        return;

    stop_all();

    for (auto& v : voices_) {
        alDeleteSources(1, &v.source);
        v.source = 0;
    }

    for (auto buffer : buffers_)
        if (buffer)
            alDeleteBuffers(1, &buffer);

    buffers_.clear();
    ready_ = false;
}
