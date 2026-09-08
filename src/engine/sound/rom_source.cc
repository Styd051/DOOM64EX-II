#include <algorithm>
#include <fstream>
#include <fluidsynth.h>
#include <ostream>
#include <numeric>

#include "prelude.hh"

#include "system/n64_rom.hh"
#include "utility/endian.hh"
#include "utility/binary_reader.hh"

namespace {
  sys::N64Rom g_rom;

  struct Sn64Header {
      // char id[4];
      uint32 game_id; //< Must be 2
      // uint32 _pad0;
      // uint32 _version_id; //< Always 100. Not read or used in game.
      // uint32 _pad1;
      // uint32 _pad2;
      uint32 len1; //< Length of file minus the header size
      // uint32 _pad3;
      uint32 num_inst; //< Number of instruments (31)
      uint16 num_patches; //< Number of patches
      uint16 patch_size; //< sizeof patch struct
      uint16 num_subpatches; //< Number of subpatches
      uint16 subpatch_size; //< sizeof subpatch struct
      uint16 num_sounds; //< Number of sounds
      uint16 sound_size; //< sizeof sound struct
      // uint32 _pad4;
      // uint32 _pad5;

      static constexpr size_t size = 56;

      static Sn64Header from_istream(std::istream& stream)
      {
          Sn64Header d {};

          BinaryReader(stream)
              .magic("SN64")
              .big_endian(d.game_id)
              .ignore_t<uint32>(4)
              .big_endian(d.len1)
              .ignore_t<uint32>()
              .big_endian(d.num_inst,
                          d.num_patches,
                          d.patch_size,
                          d.num_subpatches,
                          d.subpatch_size,
                          d.num_sounds,
                          d.sound_size)
              .ignore_t<uint32>(2)
              .assert_size(size);

          return d;
      }
  } sn64;

  struct PatchHeader {
      uint16 length; //< Length (little endian)
      uint16 offset; //< Offset in SN64 file

      static PatchHeader from_istream(std::istream& stream)
      {
          PatchHeader d {};
          BinaryReader(stream)
              .little_endian(d.length)
              .big_endian(d.offset)
              .assert_size(4);
          return d;
      }
  };

  struct SubpatchHeader {
      uint8 unity_pitch; //< Unknown; used by N64 library
      uint8 attenuation;
      uint8 pan; //< Left/Right panning
      uint8 instrument; //< (Boolean) Treat this as an instrument?
      uint8 root_key; //< Instrument's root key
      uint8 detune; //< Detune key (20120327 villsa - identified)
      uint8 note_min; //< Use this subpatch if note > min_note
      uint8 note_max; //< Use this subpatch if note < max_note
      uint8 pitch_wheel_range_low; //< (20120326 villsa - identified)
      uint8 pitch_wheel_range_high; //< (20120326 villsa - identified)
      uint16 id; //< Sound ID
      int16 attack_time; //< Attack time (fade in)
      // uint8 _unknown0;
      // uint8 _unknown1;
      int16 decay_time; //< Decay time (fade out)
      // uint8 _volume0; //< Volume (unknown purpose)
      // uint8 _volume1; //< Volume (unused?)

      static SubpatchHeader from_istream(std::istream& stream)
      {
          SubpatchHeader d {};
          BinaryReader(stream)
              .big_endian(d.unity_pitch,
                          d.attenuation,
                          d.pan,
                          d.instrument,
                          d.root_key,
                          d.detune,
                          d.note_min,
                          d.note_max,
                          d.pitch_wheel_range_low,
                          d.pitch_wheel_range_high,
                          d.id,
                          d.attack_time)
              .ignore_t<uint8>(2)
              .big_endian(d.decay_time)
              .ignore_t<uint8>(2)
              .assert_size(20);

          return d;
      }
  };

  struct WaveTable {
      uint32 start; //< Start of ROM offset
      uint32 size; //< Size of sound
      // uint32 _pad0;
      uint32 pitch; //< Pitch correction
      uint32 loop_id; //< Index ID for the loop table
      // uint32 _pad1;

      static WaveTable from_istream(std::istream& stream)
      {
          WaveTable d {};
          BinaryReader(stream)
              .big_endian(d.start, d.size)
              .ignore_t<uint32>()
              .big_endian(d.pitch, d.loop_id)
              .ignore_t<uint32>()
              .assert_size(24);
          return d;
      }
  };

  struct LoopInfo {
      uint16 num_sounds; //< Sound count (124)
      // uint16 _pad0;
      uint16 num_loops; //< Loop data count (23)
      // uint16 _num_sounds2; //< Sound count again? (124)

      static constexpr size_t size = 8;

      static LoopInfo from_istream(std::istream& stream)
      {
          LoopInfo d {};
          BinaryReader(stream)
              .big_endian(d.num_sounds)
              .ignore(2)
              .big_endian(d.num_loops)
              .ignore(2)
              .assert_size(size);
          return d;
      }
  };

  struct LoopTable {
      uint32 loop_start;
      uint32 loop_end;
      // char _pad0[40]; //< Garbage in rom, but changed/set in game

      static LoopTable from_istream(std::istream& stream)
      {
          LoopTable d {};
          BinaryReader(stream)
              .big_endian(d.loop_start, d.loop_end)
              .ignore(40)
              .assert_size(48);
          return d;
      }
  };

  struct PredictorTable {
      uint32 order; //< Order ID
      uint32 num_predictors; //< Number of predictors
      int16 predictors[128];

      static PredictorTable from_istream(std::istream& stream)
      {
          PredictorTable d {};
          BinaryReader(stream)
              .big_endian(d.order,
                          d.num_predictors,
                          d.predictors)
              .assert_size(264);
          return d;
      }
  };

  struct SseqHeader {
      // char id[4];
      uint32 game_id; //< Must be 2
      // uint32 _pad0;
      uint32 num_entries; //< Number of entries
      // uint32 _pad1;
      // uint32 _pad2;
      uint32 entry_size; //< sizeof(entry) * num_entries
      // uint32 _pad3;

      static constexpr size_t size = 32;

      static SseqHeader from_istream(std::istream& stream)
      {
          SseqHeader d {};
          BinaryReader(stream)
              .magic("SSEQ")
              .big_endian(d.game_id)
              .ignore(4)
              .big_endian(d.num_entries)
              .ignore(8)
              .big_endian(d.entry_size)
              .ignore(4)
              .assert_size(size);
          return d;
      }
  };

  struct EntryHeader {
      uint16 num_tracks; //< Number of tracks
      // uint16 _pad0;
      uint32 length; //< Length of entry
      uint32 offset; //< Offset in SSeq file
      // uint32 _pad1;

      static constexpr size_t binary_size = 16;

      static EntryHeader from_istream(std::istream& stream)
      {
          EntryHeader d {};
          BinaryReader(stream)
              .big_endian(d.num_tracks)
              .ignore(2)
              .big_endian(d.length,
                          d.offset)
              .ignore(4)
              .assert_size(binary_size);
          return d;
      }
  };

  struct TrackHeader {
      uint16 flag; //< Usually0 on sounds, 0x100 on music
      uint16 id; //< Subpatch ID
      // uint16 _pad0;
      uint8 volume; //< Default volume
      uint8 pan; //< Default pan
      // uint16 _pad1;
      uint16 bpm; //< Beats per minute
      uint16 timediv; //< Time division
      uint16 loop; //< (Boolean) 0 if no loop, 1 if yes
      // uint16 _pad2;
      uint16 size; //< Size of MIDI data

      static constexpr size_t binary_size = 20;

      static TrackHeader from_istream(std::istream& stream)
      {
          TrackHeader d {};
          BinaryReader(stream)
              .big_endian(d.flag,
                          d.id)
              .ignore(2)
              .big_endian(d.volume,
                          d.pan)
              .ignore(2)
              .big_endian(d.bpm,
                          d.timediv,
                          d.loop)
              .ignore(2)
              .big_endian(d.size)
              .assert_size(binary_size);
          return d;
      }
  };

  struct MidiTrackHeader {
      char id[4];
      int length;

      static MidiTrackHeader from_istream(std::istream& stream)
      {
          MidiTrackHeader d {};
          BinaryReader(stream)
              .magic("MTrk")
              .big_endian(d.length);
          return d;
      }
  };

  struct MidiHeader {
      char id[4];
      int32 chunk_size;
      int16 type;
      uint16 num_tracks;
      uint16 delta;
      uint32 size;
  };

  struct Generator {
      int type;
      int ival;

      explicit Generator(int type, int value):
          type(type), ival(value) {}

      explicit Generator(int type, uint8 lo, uint8 hi):
          type(type), ival((hi << 8) | lo) {}
  };

  struct Instrument {
      size_t sample_id;
      int note_min;
      int note_max;
      Vector<Generator> generators;
  };

  struct Preset {
      String name;
      int bank;
      int prog;
      Vector<Instrument> instruments;
  };

  //
  // One decoded cartridge sound, and the handle FluidSynth knows it by.
  //
  // This used to be a fluid_sample_t filled in by hand. From FluidSynth 2.0 on
  // that struct is opaque, so the placement is kept here -- where the witness
  // can still read it -- and pushed across the boundary through the setters.
  //
  // start, end and the loop points are indices into sample_data_, which holds
  // every sample end to end with 16 zero frames between them. Those zeroes are
  // not padding for tidiness: FluidSynth reads a few frames either side of a
  // loop to interpolate, and asks for at least 8 when it is given a buffer it
  // does not own.
  //
  struct RomSample {
      char            name[24] {};
      unsigned        start {};
      unsigned        end {};
      unsigned        loopstart {};
      unsigned        loopend {};
      unsigned        samplerate {};
      int             origpitch {};
      int             pitchadj {};
      fluid_sample_t* handle {};
  };

  std::vector<Preset> presets_;
  std::vector<RomSample> samples_;
  std::vector<short> sample_data_;

  std::unique_ptr<PatchHeader[]> patches_;
  std::unique_ptr<SubpatchHeader[]> subpatches_;
  std::vector<std::string> midis_;
  size_t new_bank_offset_ {};

  void decode8(std::istream& in, short* out, int index, const short* pred1, short* last_sample)
  {
      static const short itable[16] = {
          0, 1, 2, 3, 4, 5, 6, 7,
          -8, -7, -6, -5, -4, -3, -2, -1,
      };

      std::fill_n(out, 8, 0);

      auto pred2 = pred1 + 8;

      short tmp[8];
      for (size_t i {}; i + 1 < 8; i += 2) {
          auto c = in.get();
          tmp[i] = itable[c >> 4] << index;
          tmp[i + 1] = itable[c & 0xf] << index;
      }

      for (int i {}; i < 8; ++i) {
          int64 total = pred1[i] * last_sample[6] + pred2[i] * last_sample[7];

          if (i > 0) {
              for (int j { i - 1 }; j >= 0; --j) {
                  total += tmp[(i - 1) - j] * pred2[j];
              }
          }

          int64 result = ((tmp[i] << 0xb) + total) >> 0xb;
          int16 sample {};

          if (result > 0x7fff) {
              sample = 0x7fff;
          } else if (result < -0x8000) {
              sample = -0x8000;
          } else {
              sample = static_cast<int16>(result);
          }

          out[i] = sample;
      }

      std::copy_n(out, 8, last_sample);
  }

  void decode_vadpcm(std::istream& in, short* out, size_t len, const PredictorTable& book)
  {
      short last_sample[8] {};

      for (size_t i {}; i + 8 < len; i += 9) {
          int c = in.get();

          auto index = (c >> 4) & 0xf;
          auto pred = &book.predictors[(c & 0xf) * 16];

          decode8(in, out,     index, pred, last_sample);
          decode8(in, out + 8, index, pred, last_sample);
          out += 16;
      }
  }

  const SubpatchHeader& get_subpatch_by_note(const PatchHeader& patch, int note)
  {
      if (note >= 0) {
          for (size_t i {}; i < patch.length; ++i) {
              auto& s = subpatches_[patch.offset + i];

              if (note >= s.note_min && note <= s.note_max)
                  return s;
          }
      }

      return subpatches_[patch.offset];
  }

  double s_usec_to_timecents(int usec)
  {
      auto t = static_cast<double>(usec) / 1000.0;
      return 1200 * log2(t);
  }

  double s_pan_to_percent(int pan)
  {
      auto p = static_cast<double>((pan - 64) * 25) / 32.0;
      return p / 0.1;
  }

  int s_get_original_pitch(int key, int pitch)
  {
      return key - (pitch / 100);
  }

  double s_attenuation_to_percent(int attenuation)
  {
      double a = (double)((127 - attenuation) * 45) / 63.5f;
      return a / 0.1;
  }

  UniquePtr<WaveTable[]> wavtables;
  UniquePtr<PredictorTable[]> predictors;
  UniquePtr<LoopTable[]> loop_table;
  void load_sn64_()
  {
      auto s = g_rom.sn64();
      auto start = static_cast<size_t>(s.tellg());

      /* read header */
      sn64 = Sn64Header::from_istream(s);
      auto pos = s.tellg();

      /* read patches */
      patches_ = array_from_istream<PatchHeader>(s, sn64.num_patches);

      pos += sn64.patch_size * sn64.num_patches + sizeof(int);
      s.seekg(pos);

      /* read subpatches */
      subpatches_ = array_from_istream<SubpatchHeader>(s, sn64.num_subpatches);

      pos += sn64.subpatch_size * sn64.num_subpatches + sizeof(int);
      s.seekg(pos);

      /* find where non-instruments begin */
      for (size_t i {}; i < sn64.num_patches; ++i) {
          if (!subpatches_[patches_[i].offset].instrument) {
              new_bank_offset_ = i;
              break;
          }
      }

      /* read waves */
      wavtables = array_from_istream<WaveTable>(s, sn64.num_sounds);
      pos += sn64.sound_size * sn64.num_sounds;
      s.seekg(pos);

      /* read loop info */
      auto loop_info = LoopInfo::from_istream(s);
      pos += LoopInfo::size;

      /* read loop table */
      loop_table = array_from_istream<LoopTable>(s, loop_info.num_loops);
      pos += 2 * (loop_info.num_loops + 1) * loop_info.num_loops;
      s.seekg(pos);

      /* read predictors */
      predictors = array_from_istream<PredictorTable>(s, sn64.num_sounds);

      /* create presets */
      int bank {};
      int prog {};
      presets_.resize(sn64.num_patches);
      for (size_t i {}; i < sn64.num_patches; ++i) {
          auto& preset = presets_[i];
          auto& patch = patches_[i];

          if (!subpatches_[patch.offset].instrument && bank == 0) {
              prog = 0;
              bank = 1;
          }

          preset.prog = prog++;
          preset.bank = bank;

          for (size_t j {}; j < patch.length; ++j) {
              preset.instruments.emplace_back();
              auto& inst = preset.instruments.back();
              auto& gens = inst.generators;
              auto& subpatch = subpatches_[patch.offset + j];
              auto& wavtable = wavtables[subpatch.id];

              inst.note_min = subpatch.note_min;
              inst.note_max = subpatch.note_max;
              gens.emplace_back(GEN_KEYRANGE, subpatch.note_min, subpatch.note_max);

              if (subpatch.attenuation < 127) {
                  int val = s_attenuation_to_percent(subpatch.attenuation);
                  gens.emplace_back(GEN_ATTENUATION, val);
              }

              if (subpatch.pan != 64) {
                  int val = s_pan_to_percent(subpatch.pan);
                  gens.emplace_back(GEN_PAN, val);
              }

              if (subpatch.attack_time > 1) {
                  int val = s_usec_to_timecents(subpatch.attack_time);
                  gens.emplace_back(GEN_VOLENVATTACK, val);
              }

              if (subpatch.decay_time > 1) {
                  int val = s_usec_to_timecents(subpatch.decay_time);
                  gens.emplace_back(GEN_VOLENVRELEASE, val);
              }

              if (wavtable.loop_id != ~0U) {
                  gens.emplace_back(GEN_SAMPLEMODE, 1_u16);
              }

              // // root key override
              int val = s_get_original_pitch(subpatch.root_key, wavtable.pitch);
              if (val < 0)
                  val = 0;
              gens.emplace_back(GEN_OVERRIDEROOTKEY, val);

              // sample id
              inst.sample_id = subpatch.id;
              gens.emplace_back(GEN_SAMPLEID, inst.sample_id);
          }
      }

      fmt::print("SN64 Length: {}\n", static_cast<size_t>(s.tellg()) - start);
  }

  enum struct midi {
      program_change = 0x07,
      pitch_bend     = 0x09,
      unknown1       = 0x0b,
      global_volume  = 0x0c,
      global_panning = 0x0d,
      sustain_pedal  = 0x0e,
      play_note      = 0x11,
      stop_note      = 0x12,
      goto_loop      = 0x20,
      end_marker     = 0x22,
      set_loop       = 0x23
  };

  class MidiWriter {
      std::ostream& out_;
      char chan_ {};

      void chan_prefix()
      {
          out_.put(0xb0 | chan_);
      }

  public:
      MidiWriter(std::ostream& out, char chan):
          out_(out),
          chan_(chan) {}

      void bank_select(char bank)
      {
          chan_prefix();
          out_.put(0);
          out_.put(bank);
      }

      void program_change(char program)
      {
          out_.put(0xc0 | chan_);
          out_.put(program);
      }

      void set_volume(char volume)
      {
          chan_prefix();
          out_.put(0x07);
          out_.put(volume);
      }

      void set_pan(char pan)
      {
          chan_prefix();
          out_.put(0x0a);
          out_.put(pan);
      }

      void set_loop_position(char loop)
      {
          out_.write("\xff\x7f\x02\x00", 4);
          out_.put(loop);
      }

      void jump_loop_position(int loop)
      {
          out_.write("\xff\x7f\x04\x00", 4);
      }

      void registered_parameter_number(uint16_t num)
      {
          chan_prefix();
          out_.put(0x65);
          out_.write(reinterpret_cast<char*>(&num), sizeof(num));

          chan_prefix();
          out_.put(0x64);
          out_.write(reinterpret_cast<char*>(&num), sizeof(num));
      }

      void data_entry(char value)
      {
          chan_prefix();
          out_.put(0x06);
          out_.put(value);
          out_.put(0);

          chan_prefix();
          out_.put(0x26);
          out_.put(0);
          out_.put(0);
      }
  };

  namespace midi_meta_events {
    /* Format: FF 51 03 tttttt */
    void set_tempo(std::ostream& s, int tempo)
    {
        const char *tempo_str = reinterpret_cast<char*>(&tempo);
        s.write("\x00\xff\x51\x03", 4);
        s.put(tempo_str[2]);
        s.put(tempo_str[1]);
        s.put(tempo_str[0]);
    }

    void end_marker(std::ostream& s)
    {
        s.write("\xff\x2f", 2);
    }
  }

  void read_midi_(std::string& midi_data, const TrackHeader& track, const std::string& s, char chan, size_t index,
                  const PatchHeader &patch)
  {
      char tmp[4];
      int pitchbend {};
      int bendrange {};
      int note {};

      auto& inst = subpatches_[patch.offset];

      //header.delta = big_endian(track.timediv);
      //header.type = big_endian(inst.instrument);

      /* Avoid percussion channels (default as 9) */
      if (chan >= 9)
          chan++;

      std::ostringstream ss;
      MidiWriter w { ss, chan };

      /* Set initial tempo */
      if (!chan) {
          midi_meta_events::set_tempo(ss, 60'000'000 / track.bpm);
      }

      /* Set initial bank */
      if (!inst.instrument) {
          ss.put(0);
          w.bank_select(1);
      }

      ss.put(0);
      w.program_change(track.id - (track.id >= new_bank_offset_ ? new_bank_offset_ : 0));
      ss.put(0);
      w.set_volume(track.volume);
      ss.put(0);
      w.set_pan(track.pan);

      auto s2 = s;
      s2.resize(s2.size() + 1000);
      const char* m = s2.c_str();
      for (;;) {
          do {
              ss.put(*m);
          } while (*m++ < 0);

          if (*m == 0x22) {
              ss.write("\xff\x2f", 2);
              break;
          }

          switch (static_cast<midi>(*m)) {
          case midi::end_marker: // 0x22
              midi_meta_events::end_marker(ss);
              break;

          case midi::program_change: // 0x07
              m++;
              tmp[0] = *m++;
              w.program_change(tmp[0] - (tmp[0] >= new_bank_offset_ ? new_bank_offset_ : 0));
              m++;
              break;

          case midi::pitch_bend: // 0x09
              if (inst.instrument && inst.pitch_wheel_range_high != bendrange) {
                  bendrange = inst.pitch_wheel_range_high;

                  w.registered_parameter_number(0);
                  w.data_entry(bendrange);
                  w.registered_parameter_number(0x7f00);
              }

              ss.put(0xe0 | chan);
              m++;

              tmp[0] = *m++;
              tmp[1] = *m++;
              pitchbend = tmp[0] | (tmp[1] << 8);
              pitchbend += 0x2000;

              if (pitchbend > 0x3fff)
                  pitchbend = 0x3fff;

              if (pitchbend < 0)
                  pitchbend = 0;

              pitchbend *= 2;

              ss.put(pitchbend & 0xff);
              ss.put(pitchbend >> 8);
              break;

          case midi::unknown1: {
              m += 2;
              ss.write("\xff\x07\x13", 3);
              ss.write("UNKNOWN EVENT: 0x11", 20);
          }
              break;

          case midi::global_volume: // 0x0c
              m++;
              w.set_volume(*m++);
              break;

          case midi::global_panning: // 0x0d
              /* We're skipping the first byte for whatever reason */
              m++;
              ss.put(0xb0 | chan);
              ss.put(0x0a_u8);
              ss.put(*m++);
              break;

          case midi::sustain_pedal: // 0x0e
              /* We're skipping the first byte for whatever reason */
              m++;
              ss.put(0xb0 | chan);
              ss.put(0x40);
              ss.put(*m++);
              break;

          case midi::play_note:
              /* We're skipping the first byte for whatever reason */
              m++;
              ss.put(0x90 | chan);
              tmp[1] = *m++;
              ss.put(tmp[1]);
              ss.put(*m++);
              note = tmp[1];
              break;

          case midi::stop_note:
              /* We're skipping the first byte for whatever reason */
              m++;
              ss.put(0x90 | chan);
              ss.put(*m++);
              ss.put(0);
              break;

          case midi::goto_loop:
              ss.write("\xff\x7f\x04\x00", 4);
              ss.put(*m++);
              ss.put(*m++);
              ss.put(*m++);
              break;

          case midi::set_loop:
              ss.write("\xff\x7f\x02\x00", 4);
              ss.put(*m++);
              break;

          default:
              log::fatal("Unknown MIDI event '{}'", static_cast<int>(*m));
              break;
          }

          inst = get_subpatch_by_note(patch, note);
      }

      ss.put(0);

      const auto &data = ss.str();
      size_t size = data.size();
      midi_data.append("MTrk", 4);
      midi_data.push_back((size >> 24) & 0xff);
      midi_data.push_back((size >> 16) & 0xff);
      midi_data.push_back((size >> 8) & 0xff);
      midi_data.push_back(size & 0xff);

      midi_data.append(data);
  }

  auto load_sseq_()
  {
      auto s = g_rom.sseq();
      auto start = static_cast<size_t>(s.tellg());

      /* process header */
      auto sseq = SseqHeader::from_istream(s);

      /* process entries */
      auto entries = array_from_istream<EntryHeader>(s, sseq.num_entries);
      assert(EntryHeader::binary_size * sseq.num_entries == sseq.entry_size);

      auto track_table = static_cast<size_t>(s.tellg());
      for (size_t i {}; i < sseq.num_entries; ++i) {
          auto& entry = entries[i];
          s.seekg(track_table + entry.offset);

          midis_.emplace_back();
          auto& midi_data = midis_.back();
          midi_data.append("MThd\0\0\0\x06\0\0\0\0\0\0", 14);

          constexpr size_t type_pos = 8;
          constexpr size_t num_tracks_pos = 10;
          constexpr size_t timediv_pos = 12;

          midi_data[num_tracks_pos] = (entry.num_tracks >> 8) & 0xff;
          midi_data[num_tracks_pos + 1] = entry.num_tracks & 0xff;

          for (size_t j {}; j < entry.num_tracks; ++j) {
              auto track = TrackHeader::from_istream(s);

              const auto &patch = patches_[track.id];
              const auto &inst = subpatches_[patch.offset];
              midi_data[type_pos] = (inst.instrument >> 8) & 0xff;
              midi_data[type_pos + 1] = inst.instrument & 0xff;
              midi_data[timediv_pos] = (track.timediv >> 8) & 0xff;
              midi_data[timediv_pos + 1] = track.timediv & 0xff;

              if (track.loop) {
                  s.ignore(4);
              }

              std::string str;
              str.resize(track.size);
              s.read(&str[0], track.size);

              std::istringstream ss(str);

              if (!(track.flag & 0x100) && entry.num_tracks > 1)
                  log::fatal("Bad track {} offset [entry {:03d}], {}", j, i, static_cast<size_t>(s.tellg()));

              read_midi_(midi_data, track, str, j, i, patches_[track.id]);
          }
      }

      return s;
  }
}

std::string get_midi(size_t midi)
{
    //
    // The cartridge's sequences are read by rom_sfont(), which only runs when
    // the cartridge is also the soundfont. Point s_SoundFont at anything else --
    // a DLS, an SF2 -- and this was reached with midis_ empty, where .at() threw
    // and took the engine down before the audio device was even open.
    //
    // A missing sequence is not a reason to stop: the song simply fails to
    // register and the rest of the game runs. Said once, because it would
    // otherwise be said 117 times.
    //
    if (midi >= midis_.size()) {
        static bool warned = false;

        if (!warned) {
            warned = true;
            log::warn("The cartridge's music and sound effects are sequences that live "
                      "beside its instruments. Choosing another soundfont leaves them "
                      "unreadable, so this game will be silent. Unset s_SoundFont to "
                      "hear it.");
        }

        return {};
    }

    return midis_[midi];
}

//
// The five callbacks every preset of this soundfont shares. Only the Preset
// they are pointed at differs, and that rides in the preset's user data.
//
namespace {
  const char* preset_get_name(fluid_preset_t* preset)
  {
      //
      // The 1.x version read the Preset out of the fluid_preset_t pointer
      // itself rather than out of its data field, which was reading a
      // std::string from the wrong object. It went unnoticed because nothing
      // but a log line ever asked.
      //
      return reinterpret_cast<Preset*>(fluid_preset_get_data(preset))->name.c_str();
  }

  int preset_get_banknum(fluid_preset_t* preset)
  {
      return reinterpret_cast<Preset*>(fluid_preset_get_data(preset))->bank;
  }

  int preset_get_num(fluid_preset_t* preset)
  {
      return reinterpret_cast<Preset*>(fluid_preset_get_data(preset))->prog;
  }

  int preset_noteon(fluid_preset_t* preset, fluid_synth_t* synth,
                    int chan, int key, int vel)
  {
      auto& data = *reinterpret_cast<Preset*>(fluid_preset_get_data(preset));

      for (auto& inst : data.instruments) {
          if (key < inst.note_min || key > inst.note_max)
              continue;

          if (inst.sample_id >= samples_.size())
              continue;

          auto voice = fluid_synth_alloc_voice(synth, samples_[inst.sample_id].handle,
                                               chan, key, vel);

          if (!voice)
              continue;

          for (auto& g : inst.generators) {
              fluid_voice_gen_set(voice, g.type, g.ival);
          }

          fluid_synth_start_voice(synth, voice);
      }

      return FLUID_OK;
  }
}

fluid_sfont_t* rom_sfont()
{
    load_sn64_();
    load_sseq_();

    size_t pcm_size {};
    for (size_t i {}; i < sn64.num_sounds; ++i) {
        pcm_size += wavtables[i].size/9*16 + 32;
    }
    sample_data_.resize(pcm_size);

    /* read samples */
    auto s = g_rom.pcm();
    auto start = static_cast<size_t>(s.tellg());
    samples_.resize(sn64.num_sounds);
    auto pcm_ptr = sample_data_.data();
    for (size_t i {}; i < sn64.num_sounds; ++i) {
        auto& sample = samples_[i];
        sample = RomSample {};

        auto& wavtable = wavtables[i];
        auto& predictor = predictors[i];
        auto name = fmt::format("SFX_{}", i);

        wavtable.size -= wavtable.size % 9;

        std::copy_n(name.data(), name.size(), sample.name);
        sample.start = static_cast<unsigned>(
            std::distance(sample_data_.data(), pcm_ptr) + 16);
        sample.end = sample.start + wavtable.size / 9 * 16;
        sample.samplerate = 22050;
        sample.origpitch = 60;
        sample.pitchadj = 0;

        s.seekg(wavtable.start);
        decode_vadpcm(s, pcm_ptr + 16, wavtable.size, predictor);

        std::fill_n(pcm_ptr, 16, 0);

        pcm_ptr += wavtable.size / 9 * 16 + 32;

        std::fill_n(pcm_ptr - 16, 16, 0);

        sample.loopstart = sample.start;
        sample.loopend = sample.end - 1;

        if (wavtable.loop_id != ~0U) {
            const auto& loop = loop_table[wavtable.loop_id];
            sample.loopstart = sample.start + loop.loop_start;
            sample.loopend = sample.start + loop.loop_end;
        }

        //
        // Hand it over. copy_data is false, so FluidSynth reads straight out of
        // sample_data_ -- which is why the buffer must outlive the synthesiser,
        // and does: it is a file-scope vector.
        //
        // That choice also decides how the loop is expressed. With copy_data
        // true FluidSynth copies the frames behind an 8-frame margin and the
        // sample then starts at index 8, so the loop points would have to carry
        // that 8 with them. With it false the sample starts at 0 and the loop
        // is simply an offset from the first frame, which is what the numbers
        // above already are once the sample's own start is taken off. No magic
        // constant, and nothing to get wrong when FluidSynth changes its
        // margin.
        //
        // It requires at least 48 frames and 8 unused ones either side. The
        // shortest cartridge sound is 928 frames, and the 16 zeroes written
        // above and below cover the margin twice over.
        //
        sample.handle = new_fluid_sample();

        if (sample.handle) {
            fluid_sample_set_name(sample.handle, sample.name);

            fluid_sample_set_sound_data(sample.handle,
                                        sample_data_.data() + sample.start,
                                        nullptr,
                                        sample.end - sample.start,
                                        sample.samplerate,
                                        /* copy_data */ 0);

            fluid_sample_set_loop(sample.handle,
                                  sample.loopstart - sample.start,
                                  sample.loopend - sample.start);

            fluid_sample_set_pitch(sample.handle, sample.origpitch, sample.pitchadj);
        }
    }

    //
    // The presets are built once and handed out by pointer.
    //
    // The 1.x code made a fresh fluid_preset_t on every lookup and never freed
    // one, and its iterator returned the same preset for ever because nothing
    // advanced the index. Neither showed: FluidSynth only iterates a soundfont
    // when something asks it to list one, and nothing here ever did.
    //
    struct Soundfont {
        std::string name = "Doom64EX RomSource";
        std::vector<fluid_preset_t*> presets;
        size_t iter {};
    };

    auto free = [](fluid_sfont_t* sfont) -> int {
        auto data = reinterpret_cast<Soundfont*>(fluid_sfont_get_data(sfont));

        if (data) {
            for (auto p : data->presets)
                delete_fluid_preset(p);

            delete data;
        }

        for (auto& s : samples_) {
            if (s.handle) {
                delete_fluid_sample(s.handle);
                s.handle = nullptr;
            }
        }

        delete_fluid_sfont(sfont);
        return 0;
    };

    auto get_name = [](fluid_sfont_t* sfont) -> const char* {
        return reinterpret_cast<Soundfont*>(fluid_sfont_get_data(sfont))->name.c_str();
    };

    auto get_preset = [](fluid_sfont_t* sfont, int bank, int prog) -> fluid_preset_t* {
        auto& data = *reinterpret_cast<Soundfont*>(fluid_sfont_get_data(sfont));
        size_t id = (bank ? new_bank_offset_ : 0) + prog;

        //
        // Bounds checked, where the 1.x version indexed the vector blind. The
        // synthesiser does ask for combinations that do not exist, and the
        // documented answer to that is a null, not undefined behaviour.
        //
        return id < data.presets.size() ? data.presets[id] : nullptr;
    };

    auto iteration_start = [](fluid_sfont_t* sfont) {
        reinterpret_cast<Soundfont*>(fluid_sfont_get_data(sfont))->iter = 0;
    };

    auto iteration_next = [](fluid_sfont_t* sfont) -> fluid_preset_t* {
        auto& data = *reinterpret_cast<Soundfont*>(fluid_sfont_get_data(sfont));

        if (data.iter >= data.presets.size())
            return nullptr;

        return data.presets[data.iter++];
    };

    auto sfont = new_fluid_sfont(get_name, get_preset, iteration_start,
                                 iteration_next, free);

    if (!sfont)
        return nullptr;

    auto data = new Soundfont;
    fluid_sfont_set_data(sfont, data);

    data->presets.reserve(presets_.size());

    for (auto& preset : presets_) {
        auto p = new_fluid_preset(sfont, preset_get_name, preset_get_banknum,
                                  preset_get_num, preset_noteon,
                                  delete_fluid_preset);

        if (!p)
            break;

        fluid_preset_set_data(p, &preset);
        data->presets.push_back(p);
    }

    return sfont;
}

fluid_sfloader_t* rom_soundfont()
{
    auto load = [](fluid_sfloader_t*, const char *fname) -> fluid_sfont_t* {
        if (g_rom.open(fname)) {
            return rom_sfont();
        }

        return nullptr;
    };

    return new_fluid_sfloader(load, delete_fluid_sfloader);
}

//
// rom_sfont_dump
//
// Writes out everything the cartridge's soundfont hands to FluidSynth: every
// sample's placement and loop, every preset's instruments and generators.
//
// This exists to be a witness across a FluidSynth version change. The rendered
// audio cannot be compared bit for bit -- the synthesiser's interpolation,
// filters and reverb all changed between 1.x and 2.x, so identical output would
// be the surprise, not the goal. What must survive is the *data*: the same
// samples, in the same places, with the same loops, reachable through the same
// presets. That is what this prints, and it is comparable byte for byte.
//
// Written after the lesson of the demo work: a correction without a reference
// to check it against is a bet, and four bets in a row cannot be told apart.
//
void rom_sfont_dump(const char* path, fluid_synth_t* synth, int sfont_id)
{
    std::ofstream out(path);

    if (!out) {
        log::warn("rom_sfont_dump: could not write {}", path);
        return;
    }

    out << "rom soundfont witness\n";
    out << "samples " << samples_.size() << "\n";
    out << "presets " << presets_.size() << "\n";
    out << "pcm " << sample_data_.size() << "\n\n";

    //
    // Which preset each sequence reaches for.
    //
    // A sequence is not tied to the preset of the same number: it carries a
    // program change naming one, and the numbering of the two lists is its own.
    // Nothing in the engine needed to know that until the remaster's WAD turned
    // up with the same sounds recorded in a third order, and the only way to
    // line the three up is to read what the cartridge itself says.
    //
    for (size_t i {}; i < midis_.size(); ++i) {
        const auto& m = midis_[i];
        int prog = -1;

        //
        // Read only far enough to find the first program change. This walks the
        // bytes rather than parsing the MIDI properly, which is sound here only
        // because these sequences put their program change before any event
        // whose data could be mistaken for one.
        //
        for (size_t p {}; p + 1 < m.size(); ++p) {
            if ((static_cast<unsigned char>(m[p]) & 0xf0) == 0xc0) {
                prog = static_cast<unsigned char>(m[p + 1]);
                break;
            }
        }

        out << fmt::format("seq {:4} prog {:4}\n", i, prog);
    }

    out << "\n";

    for (size_t i {}; i < samples_.size(); ++i) {
        const auto& s = samples_[i];

        //
        // A checksum over the sample's own PCM, so that a change in the VADPCM
        // decoder shows up here rather than silently later. Cheap and order
        // dependent, which is all that is wanted.
        //
        uint32 sum = 2166136261u;

        for (auto p = s.start; p < s.end && p < sample_data_.size(); ++p) {
            sum = (sum ^ static_cast<uint16>(sample_data_[p])) * 16777619u;
        }

        out << fmt::format("sample {:4} {:<12} rate {:6} start {:8} end {:8} "
                           "loop {:8}..{:8} pitch {:3} adj {:5} pcm {:08x}\n",
                           i, s.name, s.samplerate, s.start, s.end,
                           s.loopstart, s.loopend, s.origpitch, s.pitchadj, sum);
    }

    out << "\n";

    for (size_t i {}; i < presets_.size(); ++i) {
        const auto& p = presets_[i];

        out << fmt::format("preset {:4} bank {:3} prog {:3} instruments {:3} \"{}\"\n",
                           i, p.bank, p.prog, p.instruments.size(), p.name);

        for (size_t j {}; j < p.instruments.size(); ++j) {
            const auto& inst = p.instruments[j];

            out << fmt::format("   inst {:3} sample {:4} notes {:3}..{:3} gens {:3}",
                               j, inst.sample_id, inst.note_min, inst.note_max,
                               inst.generators.size());

            for (const auto& g : inst.generators) {
                out << fmt::format(" {}={}", static_cast<int>(g.type), g.ival);
            }

            out << "\n";
        }
    }

    //
    // And then the question the listing cannot answer: does any of it actually
    // make a sound?
    //
    // Every preset is selected in turn, given one note, and rendered for a
    // sixteenth of a second. What is recorded is only whether the output moved
    // at all -- the peak sample. Comparing the peaks themselves across
    // FluidSynth versions would be meaningless, since interpolation and filters
    // both changed, but "how many of the 147 presets produce silence" is the
    // same question in both, and it has the same answer or the port is wrong.
    //
    // Safe to do here: the sequencer thread is idle until I_InitSequencer
    // signals it, and the audio device is not open yet, so nothing else is
    // pulling samples out of the synthesiser.
    //
    if (synth) {
        constexpr int frames = 44100 / 16;
        std::vector<short> buf(frames * 2);
        size_t audible {};

        out << "\nsynthesis probe\n";

        for (size_t i {}; i < presets_.size(); ++i) {
            const auto& p = presets_[i];

            fluid_synth_all_sounds_off(synth, 0);
            fluid_synth_program_select(synth, 0, sfont_id, p.bank, p.prog);
            fluid_synth_noteon(synth, 0, 60, 127);

            std::fill(buf.begin(), buf.end(), 0);
            fluid_synth_write_s16(synth, frames, buf.data(), 0, 2, buf.data(), 1, 2);

            int peak {};

            for (auto v : buf) {
                int a = v < 0 ? -v : v;
                if (a > peak) peak = a;
            }

            if (peak)
                ++audible;

            //
            // The peak is printed for every preset, not just the silent ones.
            // Comparing one version's peaks with another's says nothing -- the
            // interpolation changed -- but comparing presets with each other
            // inside one version says a great deal: a soundfont whose peaks run
            // from a few hundred to full scale is one that will sound loud in
            // places and inaudible in others.
            //
            out << fmt::format("peak {:4} bank {:3} prog {:3} {:6} \"{}\"\n",
                               i, p.bank, p.prog, peak, p.name);
        }

        fluid_synth_all_sounds_off(synth, 0);

        out << fmt::format("audible {} of {}\n", audible, presets_.size());

        log::info("rom_sfont_dump: {} of {} presets produced sound",
                  audible, presets_.size());
    }

    log::info("rom_sfont_dump: wrote {} ({} samples, {} presets)",
              path, samples_.size(), presets_.size());
}
