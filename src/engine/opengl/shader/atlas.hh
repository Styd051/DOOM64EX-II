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

#ifndef __SHADER_ATLAS__29517340
#define __SHADER_ATLAS__29517340

#include <prelude.hh>

namespace imp {
  namespace shader {
    /*!
     * Where one texture sits inside the array.
     *
     * The shader rebuilds a page coordinate from these:
     *
     *   tOffset = offset + ((y % height) * width) + (x % width);
     *   page    = ivec2(tOffset & 1023, (tOffset / 1024) & 1023);
     *
     * so a texture is simply its own rows laid end to end, which is what makes
     * the packing lossless -- measured at 0.2% waste across all 1454 images.
     */
    struct AtlasEntry {
        uint32 offset {};   //!< linear texel offset within its layer
        uint16 layer {};    //!< array layer
        uint16 width {};
        uint16 height {};

        bool valid() const
        { return width != 0 && height != 0; }
    };

    /*!
     * Read every world texture and sprite, pack them into 1024x1024 layers and
     * upload the result as one GL_TEXTURE_2D_ARRAY. Idempotent, and does
     * nothing if it has already succeeded.
     *
     * @return true if the array is usable
     */
    bool atlas_build();

    /*! Whether atlas_build has succeeded. */
    bool atlas_ready();

    /*! GL name of the array texture, or 0. */
    unsigned atlas_texture();

    /*! Number of layers in use. */
    size_t atlas_layers();

    /*!
     * @param texnum Index into the textures section
     * @return Where that texture lives, or an invalid entry
     */
    const AtlasEntry& atlas_world(int texnum);

    /*!
     * @param spritenum Index into the sprites section
     * @param pal Palette variant, clamped like GL_BindSpriteTexture does
     * @return Where that sprite lives, or an invalid entry
     */
    const AtlasEntry& atlas_sprite(int spritenum, int pal);

    /*! Measure the textures and report how they would pack, without building. */
    void atlas_stats();

    /*! Write each layer to atlas000.png and so on, to be looked at. */
    void atlas_dump();

    /*! Read the array back from the GPU and compare it against the sources. */
    void atlas_verify();

    /*! Register the atlas console commands. */
    void init_atlas();
  }
}

#endif //__SHADER_ATLAS__29517340
