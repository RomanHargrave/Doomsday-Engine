/** @file vrwindowtransform.cpp  Window content transformation for virtual reality.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/vrwindowtransform.h"
#include "de_platform.h"
#include "con_main.h"
#include "render/vr.h"

#include <de/Drawable>
#include <de/GLFramebuffer>

using namespace de;

DENG2_PIMPL(VRWindowTransform)
{
    Drawable oculusRift;
    GLUniform uOculusRiftFB;
    GLUniform uOculusDistortionScale;
    GLUniform uOculusScreenSize;
    GLUniform uOculusLensSeparation;
    GLUniform uOculusHmdWarpParam;
    GLUniform uOculusChromAbParam;

    typedef GLBufferT<Vertex3Tex> OculusRiftVBuf;
    GLFramebuffer unwarpedFB;

    Instance(Public *i)
        : Base(i),
          uOculusRiftFB("texture", GLUniform::Sampler2D),
          uOculusDistortionScale("distortionScale", GLUniform::Float),
          uOculusScreenSize("screenSize", GLUniform::Vec2),
          uOculusLensSeparation("lensSeparationDistance", GLUniform::Float),
          uOculusHmdWarpParam("hmdWarpParam", GLUniform::Vec4),
          uOculusChromAbParam("chromAbParam", GLUniform::Vec4)
    {}

    void init()
    {
        /// @todo Only do this when Oculus Rift mode is enabled.
        /// Free the allocated resources when non-Rift mode in use.

        OculusRiftVBuf *buf = new OculusRiftVBuf;
        oculusRift.addBuffer(buf);

        // Set up a simple static quad.
        OculusRiftVBuf::Type const verts[4] = {
            { Vector3f(-1,  1, 0.5f), Vector2f(0, 1), },
            { Vector3f( 1,  1, 0.5f), Vector2f(1, 1), },
            { Vector3f(-1, -1, 0.5f), Vector2f(0, 0), },
            { Vector3f( 1, -1, 0.5f), Vector2f(1, 0), }
        };
        buf->setVertices(gl::TriangleStrip, verts, 4, gl::Static);

        self.window().root().shaders()
                .build(oculusRift.program(), "vr.oculusrift.barrel")
                    << uOculusRiftFB
                    << uOculusDistortionScale
                    << uOculusScreenSize
                    << uOculusLensSeparation
                    << uOculusHmdWarpParam
                    << uOculusChromAbParam;

        unwarpedFB.glInit();
        uOculusRiftFB = unwarpedFB.colorTexture();
    }

    void deinit()
    {
        oculusRift.clear();
        unwarpedFB.glDeinit();
    }

    Canvas &canvas() const
    {
        return self.window().canvas();
    }

    GLTarget &target() const
    {
        return canvas().renderTarget();
    }

    int width() const
    {
        return canvas().width();
    }

    int height() const
    {
        return canvas().height();
    }

    void drawContent() const
    {
        self.window().root().draw();
    }

    /**
     * Draws the entire UI in two halves, one for the left eye and one for the
     * right. The Oculus Rift optical distortion effect is applied using a
     * shader.
     *
     * @todo unwarpedTarget and unwarpedTexture should be cleared/deleted when
     * Oculus Rift mode is disabled (or whenever they are not needed).
     */
    void vrDrawOculusRift()
    {
        VR::applyFrustumShift = false;

        /// @todo shrunken hud
        // Allocate offscreen buffers - larger than Oculus Rift size, to get adequate resolution at center after warp
        // For some reason, 1.5X looks best, even though objects are ~2.3X unwarped size at center.
        Canvas::Size textureSize(1920, 1200); // 1.5 * 1280x800
        // Canvas::Size textureSize(2560, 1600); // 2 * 1280x800 // Undesirable relative softness at very center of image
        // Canvas::Size textureSize(3200, 2000); // 2.5 * 1280x800 // Softness here too
        unwarpedFB.resize(textureSize);
        unwarpedFB.setSampleCount(GLFramebuffer::defaultMultisampling());

        // Set render target to offscreen temporarily.
        GLState::push()
                .setTarget(unwarpedFB.target())
                .setViewport(Rectangleui::fromSize(unwarpedFB.size()))
                .apply();
        unwarpedFB.target().unsetActiveRect(true);
        unwarpedFB.target().clear(GLTarget::ColorDepth);

        // Left eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        unwarpedFB.target().setActiveRect(Rectangleui(0, 0, textureSize.x/2, textureSize.y), true);
        drawContent();

        VR::holdViewPosition(); // Don't (late-schedule) change view direction between eye renders

        // Right eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        unwarpedFB.target().setActiveRect(Rectangleui(textureSize.x/2, 0, textureSize.x/2, textureSize.y), true);
        drawContent();

        VR::releaseViewPosition(); // OK, you can change the viewpoint henceforth

        unwarpedFB.target().unsetActiveRect(true);

        GLState::pop().apply();

        // Necessary until the legacy code uses GLState, too:
        glEnable(GL_TEXTURE_2D);

        target().clear(GLTarget::Color);
        GLState::push()
                .setDepthTest(false);

        // Copy contents of offscreen buffer to normal screen.
        uOculusDistortionScale = VR::riftState.distortionScale();
        uOculusScreenSize = VR::riftState.screenSize();
        uOculusLensSeparation = VR::riftState.lensSeparationDistance();
        uOculusHmdWarpParam = VR::riftState.hmdWarpParam();
        uOculusChromAbParam = VR::riftState.chromAbParam();
        //
        oculusRift.draw();

        glBindTexture(GL_TEXTURE_2D, 0);
        glDepthMask(GL_TRUE);

        GLState::pop().apply();

        VR::applyFrustumShift = true; // restore default
    }
};

VRWindowTransform::VRWindowTransform(ClientWindow &window)
    : WindowTransform(window), d(new Instance(this))
{}

void VRWindowTransform::glInit()
{
    d->init();
}

void VRWindowTransform::glDeinit()
{
    d->deinit();
}

Vector2ui VRWindowTransform::logicalRootSize(Vector2ui const &physicalCanvasSize) const
{
    Canvas::Size size = physicalCanvasSize;

    switch(VR::mode())
    {
    // Left-right screen split modes
    case VR::MODE_CROSSEYE:
    case VR::MODE_PARALLEL:
        // Adjust effective UI size for stereoscopic rendering.
        size.y *= 2;
        size *= .75f; // Make it a bit bigger.
        break;

    case VR::MODE_OCULUS_RIFT:
        /// @todo - taskbar needs to elevate above bottom of screen in Rift mode
        // Adjust effective UI size for stereoscopic rendering.
        size.x = size.y * VR::riftState.aspect();
        size *= 1.0f; // Use a large font in taskbar
        break;

    // Allow UI to squish in top/bottom and SBS mode: 3D hardware will unsquish them
    case VR::MODE_TOP_BOTTOM:
    case VR::MODE_SIDE_BY_SIDE:
    default:
        break;
    }

    return size;
}

Vector2f VRWindowTransform::windowToLogicalCoords(Vector2i const &winPos) const
{
    // We need to map the real window coordinates to logical root view
    // coordinates according to the used transformation.

    Vector2f pos = winPos;

    Vector2f const size = window().canvas().size();
    Vector2f const viewSize = Vector2f(window().root().viewWidth().value(),
                                       window().root().viewHeight().value());

    switch(VR::mode())
    {
    // Left-right screen split modes
    case VR::MODE_SIDE_BY_SIDE:
    case VR::MODE_CROSSEYE:
    case VR::MODE_PARALLEL:
    case VR::MODE_OCULUS_RIFT:
        // Make it possible to access both frames.
        if(pos.x >= size.x/2)
        {
            pos.x -= size.x/2;
        }
        pos.x *= 2;

        // Scale to logical size.
        pos = pos / size * viewSize;
        break;

    // Top-bottom screen split modes
    case VR::MODE_TOP_BOTTOM:
        // Make it possible to access both frames.
        if(pos.y >= size.y/2)
        {
            pos.y -= size.y/2;
        }
        pos.y *= 2;

        // Scale to logical size.
        pos = pos / size * viewSize;
        break;

    default:
        // Not transformed.
        break;
    }

    return pos;
}

void VRWindowTransform::drawTransformed()
{
    switch(VR::mode())
    {
    // A) Single view type stereo 3D modes here:
    case VR::MODE_MONO:
        // Non-stereoscopic frame.
        d->drawContent();
        break;

    case VR::MODE_LEFT:
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);
        d->drawContent();
        break;

    case VR::MODE_RIGHT:
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        d->drawContent();
        break;

    // B) Split-screen type stereo 3D modes here:
    case VR::MODE_TOP_BOTTOM: // Left goes on top
        // Left eye view on top of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        d->target().setActiveRect(Rectangleui(0, 0, d->width(), d->height()/2), true);
        d->drawContent();
        // Right eye view on bottom of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        d->target().setActiveRect(Rectangleui(0, d->height()/2, d->width(), d->height()/2), true);
        d->drawContent();
        break;

    case VR::MODE_SIDE_BY_SIDE: // Squished aspect
        // Left eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        d->target().setActiveRect(Rectangleui(0, 0, d->width()/2, d->height()), true);
        d->drawContent();
        // Right eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        d->target().setActiveRect(Rectangleui(d->width()/2, 0, d->width()/2, d->height()), true);
        d->drawContent();
        break;

    case VR::MODE_PARALLEL: // Normal aspect
        // Left eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        d->target().setActiveRect(Rectangleui(0, 0, d->width()/2, d->height()), true);
        d->drawContent();
        // Right eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        d->target().setActiveRect(Rectangleui(d->width()/2, 0, d->width()/2, d->height()), true);
        d->drawContent();
        break;

    case VR::MODE_CROSSEYE: // Normal aspect
        // RIght eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        d->target().setActiveRect(Rectangleui(0, 0, d->width()/2, d->height()), true);
        d->drawContent();
        // Left eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        d->target().setActiveRect(Rectangleui(d->width()/2, 0, d->width()/2, d->height()), true);
        d->drawContent();
        break;

    case VR::MODE_OCULUS_RIFT:
        d->vrDrawOculusRift();
        break;

    // Overlaid type stereo 3D modes below:
    case VR::MODE_GREEN_MAGENTA:
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);        
        GLState::push().setColorMask(gl::WriteGreen | gl::WriteAlpha).apply(); // Left eye view green
        d->drawContent();
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        GLState::current().setColorMask(gl::WriteRed | gl::WriteBlue | gl::WriteAlpha).apply(); // Right eye view magenta
        d->drawContent();
        GLState::pop().apply();
        break;

    case VR::MODE_RED_CYAN:
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);
        GLState::push().setColorMask(gl::WriteRed | gl::WriteAlpha).apply(); // Left eye view red
        d->drawContent();
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        GLState::current().setColorMask(gl::WriteGreen | gl::WriteBlue | gl::WriteAlpha).apply(); // Right eye view cyan
        d->drawContent();
        GLState::pop().apply();
        break;

    case VR::MODE_QUAD_BUFFERED:
        if(d->canvas().format().stereo())
        {
            // Left eye view
            VR::eyeShift = VR::getEyeShift(-1);
            d->drawContent();
            d->canvas().framebuffer().swapBuffers(d->canvas(), gl::SwapStereoLeftBuffer);

            // Right eye view
            VR::eyeShift = VR::getEyeShift(+1);
            d->drawContent();
            d->canvas().framebuffer().swapBuffers(d->canvas(), gl::SwapStereoRightBuffer);
        }
        else
        {
            // Normal non-stereoscopic frame.
            d->drawContent();
        }
        break;

    case VR::MODE_ROW_INTERLEAVED:
    {
        // Use absolute screen position of window to determine whether the
        // first scan line is odd or even.
        QPoint ulCorner(0, 0);
        ulCorner = d->canvas().mapToGlobal(ulCorner); // widget to screen coordinates
        bool rowParityIsEven = ((ulCorner.x() % 2) == 0);
        DENG_UNUSED(rowParityIsEven);
        /// @todo - use row parity in shader or stencil, to actually interleave rows.
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);
        d->drawContent();
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        d->drawContent();
        break;
    }

    case VR::MODE_COLUMN_INTERLEAVED: /// @todo implement column interleaved stereo 3D after row intleaved is working correctly...
    case VR::MODE_CHECKERBOARD: /// @todo implement checker stereo 3D after row intleaved is working correctly ...
    default:
        // Non-stereoscopic frame.
        d->drawContent();
        break;
    }

    // Restore default VR dynamic parameters
    d->target().unsetActiveRect(true);
    VR::eyeShift = 0;
}