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
// DESCRIPTION: Off-screen rendering and the post-process chain.
//
// The engine has always drawn straight to the window. Every effect KEX's
// shaders provide -- FXAA first, then SAO and the motion blur -- needs the
// finished scene as a texture instead, so this puts a framebuffer in front of
// it: the world is rendered into that, and a full-screen quad brings it back
// out through whichever post shader is enabled.
//
// Only the world goes through it. The HUD, the automap and the menus are drawn
// afterwards, straight to the window, so an anti-aliaser never touches text.
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_POSTPROCESS__30551782
#define __SHADER_POSTPROCESS__30551782

#include <prelude.hh>

namespace imp {
  namespace shader {
    /*! Build the post-process programs. Called from shader::init. */
    void init_post();

    /*! Whether a post-process pass is enabled and usable this frame. */
    bool post_active();

    /*!
     * Redirect drawing into the off-screen buffer.
     *
     * Does nothing, and reports false, when no pass is enabled -- the caller
     * then draws to the window exactly as before.
     *
     * @return true if the framebuffer was bound
     */
    bool post_begin();

    /*!
     * Return to the window and run the chain over what was drawn.
     *
     * Safe to call when post_begin returned false; it does nothing then.
     */
    void post_end();

    /*! Drop the framebuffer, so the next frame rebuilds it at the new size. */
    void post_invalidate();

    /*!
     * How far to push the scene back behind whatever is drawn over it.
     *
     * The renderer does not decide this. It has no business reading menuactive
     * or any other game state to guess whether an interface is up: that state
     * is written from a dozen places for their own reasons, and a rendering
     * decision taken from it cannot be predicted by reading either module. The
     * interface says what it wants instead, and says it every tic.
     *
     * @param amount 0 for a sharp scene, 1 for the full r_MenuBlur radius.
     *               Values in between scale the radius, which is what makes the
     *               blur arrive with a fading menu instead of ahead of it.
     */
    /*!
     * Draw one layer of the virtual VRAM over the window.
     *
     * r_ShowVirtualVRAM picks the layer, 1 for the first; 0 draws nothing.
     * Called after the world and before the interface, so the status bar stays
     * readable over it.
     */
    void post_show_vram();

    void post_scene_blur(float amount);
  }
}

#endif //__SHADER_POSTPROCESS__30551782
