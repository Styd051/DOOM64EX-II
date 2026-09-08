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
#include "m_misc.h"
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
  // Which of the remaster's recordings is which of the game's sounds.
  //
  // The WAD lists them as SFX_033..SFX_0124, and those numbers are nothing but
  // a running count: the remaster's order is neither the cartridge's nor
  // sounds.h's. Reading the k-th recording as the k-th sound is what put a
  // monster's death rattle on a door and the pistol on a teleporter.
  //
  // This table was not guessed. It was derived from the cartridge and then
  // checked against it:
  //
  //   - Each of the cartridge's 92 sound-effect sequences carries a program
  //     change naming a patch, and that number is not the sequence's own. The
  //     witness (-romsfdump) prints it.
  //   - Each patch names a sample.
  //   - Each of the remaster's recordings is one of those samples with every
  //     16-bit value repeated -- 22050 Hz carrying 11025 Hz of content, four
  //     bytes where the cartridge has one sample.
  //
  // Decimating each recording and hashing it against the cartridge's own PCM
  // matched all 92, exactly, and the result is a clean permutation of 1..92
  // with no slot unused and none used twice. Only one pair was undecidable and
  // it cannot matter: the cartridge plays sfx_oof and sfx_noway from the same
  // sample, so the remaster carries that sound twice and either copy will do.
  //
  constexpr int WAD_SLOTS = 93;

  const int sfx_for_wad_slot_[WAD_SLOTS] = {
      0,  5,  6,  7,  8,  9, 10, 11, 12, 13,  2, 14,
      4, 15, 16, 17, 18, 19, 20, 21, 30, 41, 40, 32,
     22, 24, 25, 33, 34, 35, 44, 45, 49, 54, 52, 75,
     56, 50, 43, 31, 36, 37, 38, 46, 47, 51, 55, 53,
     76, 39, 48, 42,  3,  1, 77, 78, 27, 28, 29, 57,
     58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
     71, 72, 73, 74, 79, 80, 81, 82, 83, 84, 85, 86,
     87, 88, 89, 90, 91, 92, 23, 70, 26
  };

  //
  // SFX_033 is the first recording, SFX_0124 the last. Read from the name
  // rather than from the lump's position, so that the table above is keyed to
  // something the file actually says.
  //
  int wad_slot_from_name_(const String& name)
  {
      if (name.compare(0, 4, "SFX_") != 0 || name.size() < 5)
          return -1;

      int n = 0;

      for (size_t i = 4; i < name.size(); ++i) {
          if (name[i] < '0' || name[i] > '9')
              return -1;

          n = n * 10 + (name[i] - '0');
      }

      return (n >= 33 && n <= 124) ? n - 32 : -1;
  }

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
  // What the game asked for and nothing else: the volume S_AdjustSoundParams
  // worked out from distance, times the slider.
  //
  // This first went through the squared curve General MIDI defines for
  // controller 7 -- the one the sequencer sends -- on the reasoning that a
  // recording handed a plain linear number would sit above everything the
  // synthesiser plays. Measuring the recordings settled it the other way.
  // They are mastered with no headroom at all: a median peak of 30968 out of
  // 32767, a third of the 92 at full scale, the quietest still above 10000.
  // The curve was not holding them level with the music, it was holding them
  // 5 dB under it, and the music reaches full scale too.
  //
  // So they are given the whole range, and the sliders are left to do what
  // sliders are for. -sfxdump prints the peaks this rests on.
  //
  float gain_(int volume)
  {
      float v = std::max(0, std::min(127, volume)) / 127.0f;
      float slider = std::max(0.0f, std::min(100.0f, volume_)) / 100.0f;

      return v * slider;
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

//
// The sequencer's name table, used here to place a recording that carries a
// cartridge name rather than one of the remaster's. Declared rather than
// included, as p_setup.cc does.
//
size_t Seq_SoundLookup(StringView name);

void imp::sfx::init()
{
    if (ready_ || !oal::ready())
        return;

    auto section = wad::list_section(wad::Section::sounds);

    if (section.empty())
        return;

    buffers_.assign(NUMSFX, 0);

    const bool dump = M_CheckParm("-sfxdump") != 0;
    size_t found {};
    size_t bytes {};

    for (auto& lump_ptr : section) {
        auto& lump = *lump_ptr;
        auto  name = lump.name();

        //
        // Where the sound belongs is read from the name, never from the lump's
        // position. The remaster names its recordings SFX_033..SFX_0124 in an
        // order of its own; anything else is looked up the way the sequencer
        // looks up a cartridge name, so a PWAD that replaces SNDPUNCH with a
        // WAV lands where it means to.
        //
        int id = -1;
        int slot = wad_slot_from_name_(name);

        if (slot >= 0 && slot < WAD_SLOTS) {
            id = sfx_for_wad_slot_[slot];
        } else {
            size_t n = Seq_SoundLookup(name);

            if (n < NUMSFX)
                id = static_cast<int>(n);
        }

        // 0 is NOSOUND, which is silence and has no business holding a buffer.
        if (id <= 0)
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

        if (buffers_[id])
            alDeleteBuffers(1, &buffers_[id]);

        buffers_[id] = buffer;
        found++;
        bytes += static_cast<size_t>(wave.size);

        if (dump) {
            //
            // The peak says how much headroom a recording was mastered with,
            // which is what settles how much gain it can be given before the
            // loud ones clip.
            //
            int peak = 0;
            const auto* s = reinterpret_cast<const short*>(wave.data);

            for (int i = 0; i < wave.size / 2; i++) {
                int a = s[i] < 0 ? -s[i] : s[i];
                if (a > peak) peak = a;
            }

            I_Printf("  %-9s -> sound %3d  %6d bytes  %d Hz  peak %5d\n",
                     name.c_str(), id, (int)wave.size, (int)wave.rate, peak);
        }
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
