// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007-2012 Samuel Villarreal
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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    PNG image system
//
//-----------------------------------------------------------------------------

#include <algorithm>
#include <sstream>
#include <cmath>

#include "doomdef.h"
#include "i_swap.h"
#include "z_zone.h"
#include "gl_texture.h"

#include "core/cvar.hh"
#include "image/image.hh"
#include "image/palette_cache.hh"
#include "wad/wad.hh"
#include "console/con_console.h"

//
// I_TranslatePalette
//
// Was: raise every colour of every image to the gamma curve as it was read.
//
// That is the wrong place for it, and it showed. Baking the curve into the
// palette hits everything the engine ever loads -- the status bar, the menus,
// the title screen -- so turning the gamma up to where the world looked right
// left the DOOM 64 logo washed out. It also has to reload every texture to take
// effect, which is why the cvar carried a GL_DumpTextures callback.
//
// The curve now lives in progs/d64ex/gbuffer.shader, on the finished world image
// and on nothing else, which is also where the measurement against KEX put it.
// Nothing calls this any more; kept as the record of what moved and why.
//
// static void I_TranslatePalette(char *data, size_t count, size_t size) {
//     if (i_gamma == 0) return;
//     for(size_t i = 0; i + size - 1 < count * size; i += size) {
//         data[i + 0] = s_rgb_gamma(static_cast<uint8>(data[i + 0]));
//         ...
//     }
// }

Image I_ReadImage(int lump, dboolean palette, dboolean nopack, double alpha, int palindex) {
    // get lump data
    auto l = wad::open(lump).value();

    auto image = l.read_image().value();

    if (palindex && image.is_indexed()) {
        auto pal = image.palette();

        char palname[9];
        snprintf(palname, sizeof(palname), "PAL%4.4s%d", l.name().data(), palindex);

        if (wad::exists(palname)) {
            image.set_palette(cache::palette(palname));
        } else {
            log::debug("TODO: safe 16-colour palette swap to #{} in {}", palindex, l.name());
            Palette newpal = {pal.pixel_format(), 16};
            std::copy_n(pal.data_ptr() + palindex * 16 * pal.pixel_info().width, 16 * pal.pixel_info().width, newpal.data_ptr());
            image.set_palette(newpal);
        }
    }

    if (!palette) {
        image.convert(alpha ? PixelFormat::rgba : PixelFormat::rgb);
    }

    return image;
}
