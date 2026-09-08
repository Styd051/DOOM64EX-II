#include <fstream>
#include <sstream>
#include "../idevice.hh"
#include "../wad_loaders.hh"

using namespace imp::wad;
Vector<String> iwad_textures;

namespace {
  template <class T>
  void read_into(std::istream& s, T& x)
  {
      s.read(reinterpret_cast<char*>(&x), sizeof(T));
  }

  struct Header {
      char id[4];
      uint32 numlumps;
      uint32 infotableofs;
  };

  struct Directory {
      uint32 filepos;
      uint32 size;
      char name[8];
  };

  static_assert(sizeof(Header) == 12, "WAD Header must have a sizeof of 12 bytes");
  static_assert(sizeof(Directory) == 16, "WAD Directory must have a sizeof of 16 bytes");

  struct Info {
      String name;
      Section section;
      size_t filepos;
      size_t size;
  };

  class DoomDevice;

  class DoomLump : public ILump {
      DoomDevice& device_;
      Info info_;

  public:
      DoomLump(DoomDevice& device, Info info):
          device_(device),
          info_(info) {}

      String name() const override
      { return info_.name; }

      String real_name() const override
      { return info_.name; }

      Section section() const override
      { return info_.section; }

      UniquePtr<std::istream> stream() override;

      IDevice& device() override;
  };

  class DoomDevice : public IDevice {
      std::ifstream stream_;

  public:
      DoomDevice(StringView path):
          stream_(path.to_string(), std::ios::binary)
      {
          stream_.exceptions(stream_.failbit | stream_.badbit);
      }

      Vector<ILumpPtr> read_all() override
      {
          Vector<ILumpPtr> lumps;
          Section section {};
          bool after_textures {};
          Header header;
          read_into(stream_, header);

          stream_.seekg(header.infotableofs);
          size_t numlumps = header.numlumps;

          for (size_t i = 0; i < numlumps; ++i) {
              Directory dir;
              read_into(stream_, dir);

              std::size_t size {};
              while (size < 8 && dir.name[size]) ++size;
              String name { dir.name, size };

              if (dir.size == 0) {
                  if (name == "T_START") {
                      section = wad::Section::textures;
                  } else if (name == "G_START") {
                      section = wad::Section::graphics;
                  } else if (name == "S_START") {
                      section = wad::Section::sprites;
                  } else if (name == "DS_START") {
                      section = wad::Section::sounds;
                      after_textures = false;
                  } else if (name == "DM_START") {
                      //
                      // Music. There is no section of its own for it, and it
                      // does not need one: the cartridge reader files its
                      // MUSAMB01..MUSTITLE under sounds too, so both IWADs
                      // answer the same lookup.
                      //
                      section = wad::Section::sounds;
                  } else if (name == "T_END") {
                      section = wad::Section::normal;
                      //
                      // The remaster's WAD puts 21 graphics between T_END and
                      // DS_START and wraps them in no section at all -- there
                      // is no G_START in the file. Its own engine only insists
                      // on four sections (textures, sprites, music, sounds),
                      // and reaches everything else by name.
                      //
                      // We cannot: the graphics section is how the engine finds
                      // TITLE, SFONT, STATUS and the rest. So the run between
                      // the two markers is claimed for it.
                      //
                      after_textures = true;
                  } else if (name == "G_END") {
                      section = wad::Section::normal;
                  } else if (name == "S_END") {
                      section = wad::Section::normal;
                  } else if (name == "DS_END") {
                      section = wad::Section::normal;
                  } else if (name == "DM_END") {
                      section = wad::Section::normal;
                  } else if (name == "ENDOFWAD") {
                      break;
                  } else {
                      log::warn("Unknown WAD directory '{}'", name);
                  }
                  continue;
              }

              //
              // Kept local: the run between the markers is claimed for
              // graphics without disturbing the section the markers set, so
              // DS_START still takes over from a clean state.
              //
              auto lump_section = section;

              if (lump_section == Section::normal && after_textures)
                  lump_section = Section::graphics;

              if (lump_section == Section::textures)
                  iwad_textures.emplace_back(name);

              auto lump_info = Info { name, lump_section, dir.filepos, dir.size };
              auto lump_ptr = std::make_unique<DoomLump>(*this, lump_info);
              lumps.emplace_back(std::move(lump_ptr));
          }

          return lumps;
      }

      std::istream& stream()
      { return stream_; }
  };
}

UniquePtr<std::istream> DoomLump::stream()
{
    auto iss = std::make_unique<std::istringstream>();

    if (info_.size) {
        auto& s = device_.stream();
        s.seekg(info_.filepos);
        String buff(info_.size, 0);
        s.read(&buff[0], info_.size);
        iss->str(buff);
    }

    return iss;
}

IDevice& DoomLump::device()
{ return device_; }

IDevicePtr wad::doom_loader(StringView path)
{
    std::ifstream file(path.to_string(), std::ios::binary);
    Header header;
    read_into(file, header);
    if (memcmp(header.id, "IWAD", 4) == 0 || memcmp(header.id, "PWAD", 4) == 0) {
        return std::make_unique<DoomDevice>(path);
    } else {
        return nullptr;
    }
}
