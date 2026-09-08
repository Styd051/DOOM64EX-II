#include <sstream>
#include <map>

#include "wad/wad.hh"
#include "wad/lump_hash.hh"
#include "map.hh"

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

  struct MapLump {
      String data;
  };

  std::vector<MapLump> lumps_;

  std::map<int, int> texturehashlist_;
}

void* W_GetMapLump(int lump)
{
    return &lumps_[lump].data[0];
}

void W_CacheMapLump(int map)
{
    auto file = wad::open(fmt::format("MAP{:02d}", map));

    if (!file) {
        log::fatal("Could not find MAP{:02d}", map);
    }

    lumps_.clear();

    auto& s = file->stream();

    Header header;
    read_into(s, header);

    if (memcmp(header.id, "IWAD", 4) != 0 && memcmp(header.id, "PWAD", 4)) {
        log::fatal("MAP{:02d} is an invalid WAD", map);
    }

    std::size_t numlumps = header.numlumps;
    s.seekg(header.infotableofs);
    for (std::size_t i = 0; !s.eof() && i < numlumps; ++i) {
        Directory dir;
        read_into(s, dir);

        auto pos = s.tellg();
        s.seekg(dir.filepos);
        String str(dir.size, 0);
        s.read(&str[0], dir.size);
        s.seekg(pos);

        lumps_.push_back({ std::move(str) });
    }
}

void W_FreeMapLump()
{
    // nop
}

int W_MapLumpLength(int lump)
{
    return lumps_[lump].data.size();
}

//
// P_InitTextureHashTable
//

extern Vector<String> rom_textures;

//
// What a sidedef's texture field means, and it is not the same in the two IWADs.
//
// The cartridge stores an index straight into the texture section, so the table
// is the identity and this costs nothing. The remaster's WAD stores a 16-bit
// hash of the texture's name instead -- verified against the file: all 4902
// references in its MAP01 resolve, and the 503 textures produce 502 distinct
// hashes, the one collision being the two both named "?".
//
// The two are kept apart rather than merged. A hash is a 16-bit number and an
// index is a small one, so a merged table would let a hash land on an index and
// silently hand back the wrong texture; and only one IWAD is ever loaded, so
// there is nothing to gain by mixing them.
//
void P_InitTextureHashTable(void) {
    texturehashlist_.clear();

    if (wad::iwad_kind() == wad::Iwad::wad) {
        for(auto& lump : wad::list_section(wad::Section::textures)) {
            texturehashlist_.emplace(wad::LumpHash(lump->name()).get(),
                                     lump->section_index());
        }
    }
    else {
        for (size_t i = 0; i < rom_textures.size(); ++i) {
            texturehashlist_.emplace(i, i);
        }
    }
}

//
// P_GetTextureHashKey
//

uint32 P_GetTextureHashKey(int hash) {
    auto it = texturehashlist_.find(hash);
    if (it == texturehashlist_.end()) {
        return 0;
    } else {
        return it->second;
    }
}
