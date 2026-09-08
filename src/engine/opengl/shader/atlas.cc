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
// DESCRIPTION: The texture array KEX's world shader reads from.
//
// progs/doomSceneMain.shader does not bind textures. Every texture lives in one
// sampler2DArray whose layers are 1024x1024 pages, with each texture written
// into a page as a flat run of width*height texels. The shader turns a texture
// coordinate back into a page coordinate:
//
//   tOffset = offset + ((iTC.y % height) * width) + (iTC.x % width);
//   page    = ivec2(tOffset & 1023, (tOffset / 1024) & 1023);
//
// which is how the N64 addressed TMEM. Two consequences fall out of it:
//
//   - a texture must sit entirely inside one layer, since the row index wraps
//     at 1024 (one layer is exactly 1024*1024 texels);
//   - the layer index travels in a vertex attribute as a byte
//     (out_flash.a * 255), so there is a hard ceiling of 256 layers.
//
// This file starts with the measurement that decides whether the engine's
// textures fit inside those limits at all.
//
//-----------------------------------------------------------------------------

#include <cstring>
#include <fstream>
#include <algorithm>

#include "doomtype.h"
#include "doomdef.h"
#include "d_event.h"
#include "g_actions.h"
#include "m_misc.h"
#include "i_png.h"
#include "z_zone.h"

#include "image/image.hh"
#include "wad/wad.hh"
#include "gl_texture.h"
#include "shader/atlas.hh"
#include "shader/gl33.hh"

using namespace imp;
using namespace imp::shader;

namespace {
  constexpr size_t PAGE_SIZE = 1024;
  constexpr size_t PAGE_TEXELS = PAGE_SIZE * PAGE_SIZE;
  constexpr size_t MAX_LAYERS = 256;

// GL_TEXTURE_2D_ARRAY, absent from the 1.4 glad header.
#define GL_TEXTURE_2D_ARRAY 0x8C1A

  struct Entry {
      String name;
      size_t width {};
      size_t height {};

      size_t texels() const
      { return width * height; }
  };

  /*!
   * Measure one wad section by reading every image in it.
   * @param section Section to walk
   * @param out Entries appended here
   * @param failed Count of lumps that could not be read as an image
   */
  void measure_(wad::Section section, Vector<Entry>& out, size_t& failed)
  {
      for (auto& lump_ptr : wad::list_section(section)) {
          auto& lump = *lump_ptr;

          try {
              auto image = I_ReadImage(static_cast<int>(lump.lump_index()),
                                       false, true, true, 0);
              out.push_back(Entry { lump.name(),
                                    static_cast<size_t>(image.width()),
                                    static_cast<size_t>(image.height()) });
          } catch (...) {
              ++failed;
          }
      }
  }

  /*!
   * Pack entries into pages the way the shader expects: flat runs, no entry
   * straddling a page boundary.
   *
   * @param entries What to pack, in the order given
   * @param waste Texels lost to the gap at the end of each page
   * @return Number of layers used
   */
  size_t pack_(const Vector<Entry>& entries, size_t& waste)
  {
      size_t layers = 1;
      size_t offset = 0;
      waste = 0;

      for (const auto& e : entries) {
          size_t n = e.texels();

          if (n > PAGE_TEXELS) {
              // Cannot be represented at all: no offset would keep it inside
              // one layer.
              continue;
          }

          if (offset + n > PAGE_TEXELS) {
              waste += PAGE_TEXELS - offset;
              ++layers;
              offset = 0;
          }

          offset += n;
      }

      return layers;
  }

  void report_(const char* what, Vector<Entry>& entries, size_t failed)
  {
      if (entries.empty()) {
          I_Printf("  %s: none\n", what);
          return;
      }

      size_t total = 0;
      size_t maxw = 0, maxh = 0, maxtexels = 0;
      size_t oversized = 0;

      for (const auto& e : entries) {
          total += e.texels();
          maxw = std::max(maxw, e.width);
          maxh = std::max(maxh, e.height);
          maxtexels = std::max(maxtexels, e.texels());
          if (e.texels() > PAGE_TEXELS)
              ++oversized;
      }

      size_t waste = 0;
      size_t layers = pack_(entries, waste);

      I_Printf("  %s: %i entries, %i unreadable\n",
               what, (int)entries.size(), (int)failed);
      I_Printf("    largest      : %ix%i (%i texels)\n",
               (int)maxw, (int)maxh, (int)maxtexels);
      I_Printf("    total        : %i texels, %i MB as RGBA8\n",
               (int)total, (int)((total * 4) / (1024 * 1024)));
      I_Printf("    layers needed: %i of %i  (%.1f%% of a layer wasted on average)\n",
               (int)layers, (int)MAX_LAYERS,
               layers ? (100.0 * waste) / (layers * PAGE_TEXELS) : 0.0);

      if (oversized) {
          I_Printf("    WARNING: %i entries exceed one 1024x1024 page and cannot be packed\n",
                   (int)oversized);
      }
  }
}

void shader::atlas_stats()
{
    Vector<Entry> textures;
    Vector<Entry> sprites;
    size_t tex_failed = 0;
    size_t spr_failed = 0;

    measure_(wad::Section::textures, textures, tex_failed);
    measure_(wad::Section::sprites, sprites, spr_failed);

    I_Printf("Texture array sizing (pages of %ix%i, %i layers max):\n",
             (int)PAGE_SIZE, (int)PAGE_SIZE, (int)MAX_LAYERS);

    report_("world textures", textures, tex_failed);
    report_("sprites", sprites, spr_failed);

    Vector<Entry> both = textures;
    both.insert(both.end(), sprites.begin(), sprites.end());
    report_("both together", both, tex_failed + spr_failed);
}

//
// Building
//

namespace {
  bool built_ {};
  GLuint texture_ {};
  size_t layers_ {};

  Vector<AtlasEntry> world_;       // flattened: world_base_[n] + palette
  Vector<size_t> world_base_;
  Vector<size_t> world_pals_;
  Vector<AtlasEntry> sprite_;      // flattened: sprite_base_[n] + palette
  Vector<size_t> sprite_base_;
  Vector<size_t> sprite_pals_;

  const AtlasEntry invalid_ {};

  /*! One layer being assembled in RAM. */
  using Page = Vector<uint32>;

  /*!
   * Reserve room for one image and return where it goes, starting a new layer
   * if it would straddle the end of the current one.
   */
  bool place_(size_t texels, size_t& layer, size_t& offset, AtlasEntry& out)
  {
      if (texels == 0 || texels > PAGE_TEXELS)
          return false;

      if (offset + texels > PAGE_TEXELS) {
          ++layer;
          offset = 0;
      }

      out.layer = static_cast<uint16>(layer);
      out.offset = static_cast<uint32>(offset);

      offset += texels;
      return true;
  }

  /*!
   * Copy an image into its page. The shader addresses a texture as its rows
   * laid end to end, which is exactly the source layout, so this is a straight
   * run of texels rather than a rectangular blit.
   */
  bool blit_(Vector<Page>& pages, const AtlasEntry& e, const Image& image)
  {
      size_t texels = static_cast<size_t>(e.width) * e.height;

      // Tightly packed RGBA8 is what the rest of the engine already assumes
      // when it hands data_ptr() to glTexImage2D.
      if (image.pitch() != image.width() * 4)
          return false;

      while (pages.size() <= e.layer)
          pages.emplace_back(PAGE_TEXELS, 0u);

      std::memcpy(pages[e.layer].data() + e.offset, image.data_ptr(), texels * 4);
      return true;
  }
}

bool shader::atlas_ready()
{
    return built_;
}

unsigned shader::atlas_texture()
{
    return texture_;
}

size_t shader::atlas_layers()
{
    return layers_;
}

const AtlasEntry& shader::atlas_world(int texnum)
{
    if (texnum < 0 || static_cast<size_t>(texnum) >= world_base_.size())
        return invalid_;

    // The same two indirections GL_BindWorldTexture applies, and for the same
    // reason: ANIMDEFS animates a texture either by walking to the next lump
    // (texturetranslation) or by swapping the palette inside the one lump
    // (palettetranslation, set by GL_SetNewPalette). Skipping them left every
    // animated texture frozen -- and for the palette-driven ones, frozen on
    // palette 0, which for the computer screens and the demon faces is the
    // blank state. That is what the flat coloured panels were.
    if (texturetranslation) {
        int t = texturetranslation[texnum];
        if (t >= 0 && static_cast<size_t>(t) < world_base_.size())
            texnum = t;
    }

    size_t pal = 0;
    if (palettetranslation)
        pal = palettetranslation[texnum];

    if (pal >= world_pals_[texnum])
        pal = 0;

    return world_[world_base_[texnum] + pal];
}

const AtlasEntry& shader::atlas_sprite(int spritenum, int pal)
{
    if (spritenum < 0 || static_cast<size_t>(spritenum) >= sprite_base_.size())
        return invalid_;

    // Same clamp GL_BindSpriteTexture applies.
    size_t pals = sprite_pals_[spritenum];
    if (pal < 0 || static_cast<size_t>(pal) >= pals)
        pal = 0;

    return sprite_[sprite_base_[spritenum] + pal];
}

namespace {
  /*!
   * How many 16-colour palettes a texture lump carries.
   *
   * The count is not exposed anywhere -- InitWorldTextures allocates one slot
   * per texture and never asks -- so it is read back off the image itself.
   */
  size_t world_palettes_(size_t lump_index)
  {
      try {
          auto lump = wad::open(lump_index);
          if (!lump)
              return 1;

          auto image = lump->read_image();
          if (!image || !image->is_indexed())
              return 1;

          size_t n = image->palette().count() / 16;
          return n ? n : 1;
      } catch (...) {
          return 1;
      }
  }
}

bool shader::atlas_build()
{
    if (built_)
        return true;

    if (!gl33::loaded()) {
        log::warn("Texture array needs GL 3.3 entry points");
        return false;
    }

    auto textures = wad::list_section(wad::Section::textures);
    auto sprites = wad::list_section(wad::Section::sprites);

    world_.clear();
    world_base_.assign(textures.size(), 0);
    world_pals_.assign(textures.size(), 1);
    sprite_base_.assign(sprites.size(), 0);
    sprite_pals_.assign(sprites.size(), 1);
    sprite_.clear();

    Vector<Page> pages;
    size_t layer = 0;
    size_t offset = 0;
    size_t placed = 0;
    size_t skipped = 0;

    // World textures, with their palette variants. A texture lump carries
    // numpal 16-colour palettes, and ANIMDEFS animates some textures by
    // stepping through them rather than through separate lumps -- so all of
    // them have to be packed, exactly as sprites are.
    for (auto& lump_ptr : textures) {
        auto& lump = *lump_ptr;
        size_t i = lump.section_index();

        world_base_[i] = world_.size();
        world_pals_[i] = world_palettes_(lump.lump_index());

        for (size_t p = 0; p < world_pals_[i]; ++p) {
            AtlasEntry e {};

            try {
                auto image = I_ReadImage(static_cast<int>(lump.lump_index()),
                                         false, true, true, static_cast<int>(p));

                e.width = image.width();
                e.height = image.height();

                if (place_(static_cast<size_t>(e.width) * e.height, layer, offset, e)
                    && blit_(pages, e, image)) {
                    ++placed;
                } else {
                    e = AtlasEntry {};
                    ++skipped;
                    log::warn("Texture array: texture {} palette {} would not fit",
                              lump.name().data(), (int)p);
                }
            } catch (...) {
                e = AtlasEntry {};
                ++skipped;
                log::warn("Texture array: texture {} palette {} could not be read",
                          lump.name().data(), (int)p);
            }

            world_.push_back(e);
        }
    }

    // Sprites, with their palette variants.
    for (auto& lump_ptr : sprites) {
        auto& lump = *lump_ptr;
        size_t i = lump.section_index();

        sprite_base_[i] = sprite_.size();
        sprite_pals_[i] = static_cast<size_t>(spritecount ? spritecount[i] : 1);
        if (sprite_pals_[i] < 1)
            sprite_pals_[i] = 1;

        for (size_t p = 0; p < sprite_pals_[i]; ++p) {
            AtlasEntry e {};

            try {
                auto image = I_ReadImage(static_cast<int>(lump.lump_index()),
                                         false, true, true, static_cast<int>(p));

                e.width = image.width();
                e.height = image.height();

                if (place_(static_cast<size_t>(e.width) * e.height, layer, offset, e)
                    && blit_(pages, e, image)) {
                    ++placed;
                } else {
                    e = AtlasEntry {};
                    ++skipped;
                    log::warn("Texture array: sprite {} palette {} would not fit",
                              lump.name().data(), (int)p);
                }
            } catch (...) {
                e = AtlasEntry {};
                ++skipped;
                log::warn("Texture array: sprite {} palette {} could not be read",
                          lump.name().data(), (int)p);
            }

            sprite_.push_back(e);
        }
    }

    layers_ = pages.size();

    if (layers_ == 0) {
        log::warn("Texture array: nothing to pack");
        return false;
    }

    if (layers_ > MAX_LAYERS) {
        log::warn("Texture array needs {} layers, the vertex attribute carries {}",
                  layers_, MAX_LAYERS);
        return false;
    }

    dglGenTextures(1, &texture_);
    glActiveTexture(GL_TEXTURE0);
    dglBindTexture(GL_TEXTURE_2D_ARRAY, texture_);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                  static_cast<GLsizei>(PAGE_SIZE), static_cast<GLsizei>(PAGE_SIZE),
                  static_cast<GLsizei>(layers_), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    for (size_t i = 0; i < layers_; ++i) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                         0, 0, static_cast<GLint>(i),
                         static_cast<GLsizei>(PAGE_SIZE), static_cast<GLsizei>(PAGE_SIZE), 1,
                         GL_RGBA, GL_UNSIGNED_BYTE, pages[i].data());
    }

    // The shader fetches texels directly, so no filtering and no mipmaps.
    dglTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    dglTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    dglTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, DGL_CLAMP);
    dglTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, DGL_CLAMP);
    dglTexParameteri(GL_TEXTURE_2D_ARRAY, 0x813D /* GL_TEXTURE_MAX_LEVEL */, 0);

    dglBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    built_ = true;

    log::info("Texture array: {} images in {} layers ({} MB), {} skipped",
              placed, layers_, (layers_ * PAGE_TEXELS * 4) / (1024 * 1024), skipped);

    // A skipped image leaves a zeroed entry behind, and a zeroed entry samples
    // a zero-by-zero rectangle: the thing it belongs to is simply not drawn,
    // for the whole session, with nothing on screen to say why. That is the
    // shape of an invisible plasma ball. The array is built once at startup, so
    // a restart rebuilds it and the symptom disappears -- which is exactly how
    // it was described, and exactly why it must not be reported at info level
    // among two hundred other lines.
    if (skipped) {
        log::warn("Texture array: {} images are MISSING and whatever uses them "
                  "will not be drawn this session. Restart to rebuild.", skipped);
    }

    return true;
}

//
// CMD_TexAtlasInfo
//

static CMD(TexAtlasInfo)
{
    (void)param;
    atlas_stats();
}

//
// Dumping
//

void shader::atlas_dump()
{
    if (!built_) {
        I_Printf("Texture array has not been built\n");
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    dglBindTexture(GL_TEXTURE_2D_ARRAY, texture_);

    // Read the whole array back in one go, then split it per layer.
    Vector<uint32> all(PAGE_TEXELS * layers_, 0u);
    dglGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, all.data());
    dglBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    for (size_t i = 0; i < layers_; ++i) {
        RgbaImage image { static_cast<uint16>(PAGE_SIZE), static_cast<uint16>(PAGE_SIZE) };
        std::memcpy(image.data_ptr(), all.data() + i * PAGE_TEXELS, PAGE_TEXELS * 4);

        auto name = fmt::format("atlas{:03d}.png", i);
        std::ofstream file(name, std::ios_base::binary);

        Image out = std::move(image);
        out.save(file, ImageFormat::png);

        I_Printf("Wrote %s\n", name.c_str());
    }
}

//
// Verifying
//

void shader::atlas_verify()
{
    if (!built_) {
        I_Printf("Texture array has not been built\n");
        return;
    }

    // Read the array back from the GPU, so the check covers the upload and the
    // layer indexing, not just the packing arithmetic.
    Vector<uint32> all(PAGE_TEXELS * layers_, 0u);

    glActiveTexture(GL_TEXTURE0);
    dglBindTexture(GL_TEXTURE_2D_ARRAY, texture_);
    dglGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, all.data());
    dglBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    size_t checked = 0;
    size_t mismatched = 0;
    size_t unreadable = 0;

    auto compare = [&](const AtlasEntry& e, size_t lump_index, int pal) {
        if (!e.valid())
            return;

        try {
            auto image = I_ReadImage(static_cast<int>(lump_index), false, true, true, pal);

            if (image.width() != e.width || image.height() != e.height) {
                ++mismatched;
                return;
            }

            size_t texels = static_cast<size_t>(e.width) * e.height;
            const uint32* want = reinterpret_cast<const uint32*>(image.data_ptr());
            const uint32* got = all.data() + e.layer * PAGE_TEXELS + e.offset;

            if (std::memcmp(want, got, texels * 4) != 0)
                ++mismatched;

            ++checked;
        } catch (...) {
            ++unreadable;
        }
    };

    for (auto& lump_ptr : wad::list_section(wad::Section::textures)) {
        auto& lump = *lump_ptr;
        size_t i = lump.section_index();

        // Straight at the packed entries: atlas_world would route the lookup
        // through texturetranslation, which is exactly what is not wanted here.
        for (size_t p = 0; p < world_pals_[i]; ++p) {
            compare(world_[world_base_[i] + p],
                    lump.lump_index(), static_cast<int>(p));
        }
    }

    for (auto& lump_ptr : wad::list_section(wad::Section::sprites)) {
        auto& lump = *lump_ptr;
        size_t i = lump.section_index();

        for (size_t p = 0; p < sprite_pals_[i]; ++p) {
            compare(atlas_sprite(static_cast<int>(i), static_cast<int>(p)),
                    lump.lump_index(), static_cast<int>(p));
        }
    }

    I_Printf("Texture array verify: %i checked, %i mismatched, %i unreadable\n",
             (int)checked, (int)mismatched, (int)unreadable);
}

//
// CMD_TexAtlasBuild / CMD_TexAtlasDump
//

static CMD(TexAtlasVerify)
{
    (void)param;
    atlas_verify();
}

static CMD(TexAtlasBuild)
{
    (void)param;

    if (atlas_build()) {
        I_Printf("Texture array ready: %i layers, GL name %i\n",
                 (int)atlas_layers(), (int)atlas_texture());
    } else {
        I_Printf("Texture array could not be built\n");
    }
}

static CMD(TexAtlasDump)
{
    (void)param;
    atlas_dump();
}

void shader::init_atlas()
{
    G_AddCommand("texatlasinfo", CMD_TexAtlasInfo, 0);
    G_AddCommand("texatlasbuild", CMD_TexAtlasBuild, 0);
    G_AddCommand("texatlasdump", CMD_TexAtlasDump, 0);
    // Was reachable only as -texatlasverify, which is no use at all when the
    // thing you want to check is a sprite that went missing ten minutes into a
    // session. The interesting moment is while it is wrong, not at startup.
    G_AddCommand("texatlasverify", CMD_TexAtlasVerify, 0);

    if (M_CheckParm("-texatlasbuild")) {
        atlas_build();
    }

    if (M_CheckParm("-texatlasdump")) {
        atlas_build();
        atlas_dump();
    }

    if (M_CheckParm("-texatlasverify")) {
        atlas_build();
        atlas_verify();
    }

    if (M_CheckParm("-texatlasinfo")) {
        atlas_stats();
    }
}
