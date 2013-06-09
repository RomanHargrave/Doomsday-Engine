/** @file guiwidget.cpp  Base class for graphical widgets.
 *
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

#include "ui/widgets/guiwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "clientapp.h"
#include <de/garbage.h>
#include <de/MouseEvent>
#include <de/GLState>

using namespace de;

DENG2_PIMPL(GuiWidget)
{
    RuleRectangle rule;
    Rectanglei savedPos;
    bool inited;
    bool needGeometry;
    bool styleChanged;
    Background background;
    Animation opacity;

    // Style.
    DotPath fontId;
    DotPath textColorId;

    Instance(Public *i)
        : Base(i),
          inited(false),
          needGeometry(true),
          styleChanged(false),
          opacity(1.f, Animation::Linear),
          fontId("default"),
          textColorId("text")
    {}

    ~Instance()
    {
        // Deinitialize now if it hasn't been done already.
        self.deinitialize();
    }
};

GuiWidget::GuiWidget(String const &name) : Widget(name), d(new Instance(this))
{}

GuiRootWidget &GuiWidget::root()
{
    return static_cast<GuiRootWidget &>(Widget::root());
}

Style const &GuiWidget::style() const
{
    return ClientApp::windowSystem().style();
}

Font const &GuiWidget::font() const
{
    return style().fonts().font(d->fontId);
}

void GuiWidget::setFont(DotPath const &id)
{
    d->fontId = id;
    d->styleChanged = true;
}

ColorBank::Color GuiWidget::textColor() const
{
    return style().colors().color(d->textColorId);
}

ColorBank::Colorf GuiWidget::textColorf() const
{
    return style().colors().colorf(d->textColorId);
}

void GuiWidget::setTextColor(DotPath const &id)
{
    d->textColorId = id;
    d->styleChanged = true;
}

RuleRectangle &GuiWidget::rule()
{
    return d->rule;
}

RuleRectangle const &GuiWidget::rule() const
{
    return d->rule;
}

static void deleteGuiWidget(void *ptr)
{
    delete reinterpret_cast<GuiWidget *>(ptr);
}

void GuiWidget::deleteLater()
{
    Garbage_TrashInstance(this, deleteGuiWidget);
}

void GuiWidget::set(Background const &bg)
{
    d->background = bg;
}

bool GuiWidget::clipped() const
{
    return behavior().testFlag(ContentClipping);
}

GuiWidget::Background const &GuiWidget::background() const
{
    return d->background;
}

void GuiWidget::setOpacity(float opacity, TimeDelta span)
{
    d->opacity.setValue(opacity, span);
}

float GuiWidget::opacity() const
{
    return d->opacity;
}

float GuiWidget::visibleOpacity() const
{
    float opacity = d->opacity;
    for(Widget *i = Widget::parent(); i != 0; i = i->parent())
    {
        GuiWidget *w = dynamic_cast<GuiWidget *>(i);
        if(w)
        {
            opacity *= w->d->opacity;
        }
    }
    return opacity;
}

void GuiWidget::initialize()
{
    if(d->inited) return;

    try
    {
        d->inited = true;
        glInit();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Error when initializing widget '%s':\n")
                << name() << er.asText();
    }
}

void GuiWidget::deinitialize()
{
    if(!d->inited) return;

    try
    {
        d->inited = false;
        glDeinit();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Error when deinitializing widget '%s':\n")
                << name() << er.asText();
    }
}

void GuiWidget::update()
{
    if(!d->inited)
    {
        initialize();
    }
    if(d->styleChanged)
    {
        d->styleChanged = false;
        updateStyle();
    }
}

void GuiWidget::drawIfVisible()
{
    if(!isHidden() && d->inited)
    {
        if(clipped())
        {
            GLState::push().setScissor(rule().recti());
        }

        draw();

        if(clipped())
        {
            GLState::pop();
        }
    }
}

bool GuiWidget::hitTest(Vector2i const &pos) const
{
    if(behavior().testFlag(Unhittable))
    {
        // Can never be hit by anything.
        return false;
    }
    return rule().recti().contains(pos);
}

bool GuiWidget::hitTest(Event const &event) const
{
    return event.isMouse() && hitTest(event.as<MouseEvent>().pos());
}

GuiWidget::MouseClickStatus GuiWidget::handleMouseClick(Event const &event)
{
    if(event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.button() != MouseEvent::Left)
        {
            return MouseClickUnrelated;
        }

        if(mouse.state() == MouseEvent::Pressed && hitTest(mouse.pos()))
        {
            root().routeMouse(this);
            return MouseClickStarted;
        }

        if(mouse.state() == MouseEvent::Released && root().isEventRouted(event.type(), this))
        {
            root().routeMouse(0);
            if(hitTest(mouse.pos()))
            {
                return MouseClickFinished;
            }
            return MouseClickAborted;
        }
    }
    return MouseClickUnrelated;
}

void GuiWidget::glInit()
{}

void GuiWidget::glDeinit()
{}

void GuiWidget::requestGeometry(bool yes)
{
    d->needGeometry = yes;
}

bool GuiWidget::geometryRequested() const
{
    return d->needGeometry;
}

void GuiWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    // Is there a solid fill?
    if(d->background.solidFill.w > 0)
    {
        verts.makeQuad(rule().recti(),
                       d->background.solidFill,
                       root().atlas().imageRectf(root().solidWhitePixel()).middle());
    }

    switch(d->background.type)
    {
    case Background::GradientFrame:
        verts.makeFlexibleFrame(rule().recti(),
                                d->background.thickness,
                                d->background.color,
                                root().atlas().imageRectf(root().gradientFrame()));
        break;

    case Background::None:
        break;
    }
}

bool GuiWidget::hasChangedPlace(Rectanglei &currentPlace)
{
    currentPlace = rule().recti();
    bool changed = (d->savedPos != currentPlace);
    d->savedPos = currentPlace;
    return changed;
}

void GuiWidget::updateStyle()
{}
