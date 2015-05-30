/** @file scrollareawidget.cpp  Scrollable area.
 *
 * @todo The scroll indicator is currently only implemented for the vertical
 * direction.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/ScrollAreaWidget"

#include <de/GLState>
#include <de/KeyEvent>
#include <de/MouseEvent>
#include <de/Lockable>
#include <de/Drawable>

namespace de {

DENG_GUI_PIMPL(ScrollAreaWidget), public Lockable
{
    /**
     * Rectangle for all the content shown in the widget. The widget's
     * rectangle is the viewport into this content rectangle.
     */
    RuleRectangle contentRule;

    ScalarRule *x;
    ScalarRule *y;
    Rule *maxX;
    Rule *maxY;

    Origin origin;
    bool pageKeysEnabled;
    bool scrollingEnabled;
    Animation scrollOpacity;
    int scrollBarWidth;
    Rectanglef indicatorUv;
    bool indicatorAnimating;
    String scrollBarColorId;
    ColorBank::Colorf scrollBarColor;

    // GL objects.
    bool indicatorShown;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i)
        , origin(Top)
        , pageKeysEnabled(true)
        , scrollingEnabled(true)
        , scrollOpacity(0)
        , scrollBarWidth(0)
        , indicatorAnimating(false)
        , scrollBarColorId("accent")
        , indicatorShown(false)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uColor    ("uColor",     GLUniform::Vec4)
    {
        contentRule.setDebugName("ScrollArea-contentRule");

        updateStyle();

        x = new ScalarRule(0);
        y = new ScalarRule(0);

        maxX = new OperatorRule(OperatorRule::Maximum, Const(0),
                                contentRule.width() - self.rule().width() + self.margins().width());

        maxY = new OperatorRule(OperatorRule::Maximum, Const(0),
                                contentRule.height() - self.rule().height() + self.margins().height());
    }

    ~Instance()
    {
        releaseRef(x);
        releaseRef(y);
        releaseRef(maxX);
        releaseRef(maxY);
    }

    void glInit()
    {
        if(indicatorShown)
        {
            DefaultVertexBuf *buf = new DefaultVertexBuf;
            drawable.addBuffer(buf);

            shaders().build(drawable.program(), "generic.textured.color_ucolor")
                    << uMvpMatrix << uAtlas() << uColor;
        }
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void updateStyle()
    {
        Style const &st = style();

        scrollBarWidth = st.rules().rule("scrollarea.bar").valuei();
        scrollBarColor = st.colors().colorf(scrollBarColorId);
    }

    void restartScrollOpacityFade()
    {
        indicatorAnimating = true;
        if(origin == Bottom && self.isAtBottom())
        {
            scrollOpacity.setValue(0, .7f, .2f);
        }
        else
        {
            scrollOpacity.setValueFrom(.8f, .333f, 5, 2);
        }
    }
};

ScrollAreaWidget::ScrollAreaWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    setBehavior(ChildHitClipping);
    setBehavior(ChildVisibilityClipping); // enable clip-culling

    // Link the content rule into the widget's rectangle.
    d->contentRule.setInput(Rule::Left, rule().left() + margins().left() -
                            OperatorRule::minimum(*d->x, *d->maxX));

    setOrigin(Top);
    setContentWidth(0);
    setContentHeight(0);
}

void ScrollAreaWidget::setScrollBarColor(DotPath const &colorId)
{
    d->scrollBarColorId = colorId;
    d->updateStyle();
}

void ScrollAreaWidget::setOrigin(Origin origin)
{
    DENG2_GUARD(d);

    d->origin = origin;

    if(origin == Top)
    {
        // Anchor content to the top of the widget.
        d->contentRule.setInput(Rule::Top, rule().top() + margins().top() -
                                OperatorRule::minimum(*d->y, *d->maxY));

        d->contentRule.clearInput(Rule::Bottom);
    }
    else
    {
        // Anchor content to the bottom of the widget.
        d->contentRule.setInput(Rule::Bottom, rule().bottom() - margins().bottom() +
                                OperatorRule::minimum(*d->y, *d->maxY));

        d->contentRule.clearInput(Rule::Top);
    }
}

ScrollAreaWidget::Origin ScrollAreaWidget::origin() const
{
    return d->origin;
}

void ScrollAreaWidget::setIndicatorUv(Rectanglef const &uv)
{
    d->indicatorUv = uv;
}

void ScrollAreaWidget::setIndicatorUv(Vector2f const &uvPoint)
{
    d->indicatorUv = Rectanglef::fromSize(uvPoint, Vector2f(0, 0));
}

void ScrollAreaWidget::setContentWidth(int width)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Width, Const(width));
}

void ScrollAreaWidget::setContentWidth(Rule const &width)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Width, width);
}

void ScrollAreaWidget::setContentHeight(int height)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Height, Const(height));
}

void ScrollAreaWidget::setContentHeight(Rule const &height)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Height, height);
}

void ScrollAreaWidget::setContentSize(Rule const &width, Rule const &height)
{
    DENG2_GUARD(d);
    setContentWidth(width);
    setContentHeight(height);
}

void ScrollAreaWidget::setContentSize(Vector2i const &size)
{
    DENG2_GUARD(d);
    setContentWidth(size.x);
    setContentHeight(size.y);
}

void ScrollAreaWidget::setContentSize(Vector2ui const &size)
{
    setContentSize(Vector2i(size.x, size.y));
}

void ScrollAreaWidget::modifyContentWidth(int delta)
{
    DENG2_GUARD(d);
    setContentWidth(de::max(0, d->contentRule.width().valuei() + delta));
}

void ScrollAreaWidget::modifyContentHeight(int delta)
{
    DENG2_GUARD(d);
    setContentHeight(de::max(0, d->contentRule.height().valuei() + delta));
}

int ScrollAreaWidget::contentWidth() const
{
    DENG2_GUARD(d);
    return d->contentRule.width().valuei();
}

int ScrollAreaWidget::contentHeight() const
{
    DENG2_GUARD(d);
    return d->contentRule.height().valuei();
}

RuleRectangle const &ScrollAreaWidget::contentRule() const
{
    return d->contentRule;
}

ScalarRule &ScrollAreaWidget::scrollPositionX() const
{
    return *d->x;
}

ScalarRule &ScrollAreaWidget::scrollPositionY() const
{
    return *d->y;
}

Rule const &ScrollAreaWidget::maximumScrollX() const
{
    return *d->maxX;
}

Rule const &ScrollAreaWidget::maximumScrollY() const
{
    return *d->maxY;
}

bool ScrollAreaWidget::isScrolling() const
{
    return !d->x->animation().done() || !d->y->animation().done();
}

Rectanglei ScrollAreaWidget::viewport() const
{
    Vector4i const margin = margins().toVector();

    Rectanglei vp = rule().recti().moved(margin.xy());
    if(int(vp.width()) <= margin.x + margin.z)
    {
        vp.setWidth(0);
    }
    else
    {
        vp.bottomRight.x -= margin.x + margin.z;
    }
    if(int(vp.height()) <= margin.y + margin.w)
    {
        vp.setHeight(0);
    }
    else
    {
        vp.bottomRight.y -= margin.y + margin.w;
    }
    return vp;
}

Vector2i ScrollAreaWidget::viewportSize() const
{
    return Vector2i(rule().width().valuei()  - margins().width().valuei(),
                    rule().height().valuei() - margins().height().valuei())
            .max(Vector2i(0, 0));
}

Vector2i ScrollAreaWidget::scrollPosition() const
{
    DENG2_GUARD(d);
    return Vector2i(scrollPositionX().valuei(), scrollPositionY().valuei());
}

Vector2i ScrollAreaWidget::scrollPageSize() const
{
    return viewportSize();
}

Vector2i ScrollAreaWidget::maximumScroll() const
{
    DENG2_GUARD(d);
    return Vector2i(maximumScrollX().valuei(), maximumScrollY().valuei());
}

void ScrollAreaWidget::scroll(Vector2i const &to, TimeDelta span)
{
    scrollX(to.x, span);
    scrollY(to.y, span);
}

void ScrollAreaWidget::scrollX(int to, TimeDelta span)
{
    d->x->set(de::clamp(0, to, maximumScrollX().valuei()), span);
}

void ScrollAreaWidget::scrollY(int to, TimeDelta span)
{
    d->y->set(de::clamp(0, to, maximumScrollY().valuei()), span);
    d->restartScrollOpacityFade();
}

bool ScrollAreaWidget::isAtBottom() const
{
    return d->origin == Bottom && d->y->animation().target() == 0;
}

void ScrollAreaWidget::enableScrolling(bool enabled)
{
    d->scrollingEnabled = enabled;
}

void ScrollAreaWidget::enablePageKeys(bool enabled)
{
    d->pageKeysEnabled = enabled;
}

void ScrollAreaWidget::enableIndicatorDraw(bool enabled)
{
    d->indicatorShown = enabled;
}

bool ScrollAreaWidget::handleEvent(Event const &event)
{
    // Mouse wheel scrolling.
    if(d->scrollingEnabled && event.type() == Event::MouseWheel && hitTest(event))
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
#ifdef MACOSX
        if(mouse.wheelMotion() == MouseEvent::FineAngle)
        {
            d->y->set(de::clamp(0, int(d->y->animation().target()) +
                                toDevicePixels(mouse.wheel().y / 2 * (d->origin == Top? -1 : 1)),
                                d->maxY->valuei()), .05f);
            d->restartScrollOpacityFade();
        }
#else
        if(mouse.wheelMotion() == MouseEvent::Step)
        {
            unsigned int lineCount = 1;
#ifdef WIN32
            // Use the number of lines to scroll from system preferences.
            SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lineCount, 0);
            if(lineCount == WHEEL_PAGESCROLL)
            {
                lineCount = contentRect().height() / style().fonts().font("default").height().valuei();
            }
#endif
            d->y->set(de::clamp(0, int(d->y->animation().target()) +
                                int(mouse.wheel().y * lineCount * style().fonts().font("default").height().valuei() *
                                (d->origin == Top? -1 : 1)), d->maxY->valuei()), .15f);
            d->restartScrollOpacityFade();
        }
#endif
        return true;
    }

    // Page key scrolling.
    if(d->scrollingEnabled && event.isKeyDown())
    {
        KeyEvent const &ev = event.as<KeyEvent>();

        float pageSize = scrollPageSize().y;
        if(d->origin == Bottom) pageSize = -pageSize;

        switch(ev.ddKey())
        {
        case DDKEY_PGUP:
            if(!d->pageKeysEnabled) return false;
            if(ev.modifiers().testFlag(KeyEvent::Shift))
            {
                scrollToTop();
            }
            else
            {
                scrollY(d->y->animation().target() - pageSize/2, .3f);
            }
            return true;

        case DDKEY_PGDN:
            if(!d->pageKeysEnabled) return false;
            if(ev.modifiers().testFlag(KeyEvent::Shift))
            {
                scrollToBottom();
            }
            else
            {
                scrollY(d->y->animation().target() + pageSize/2, .3f);
            }
            return true;

        default:
            break;
        }
    }

    return GuiWidget::handleEvent(event);
}

void ScrollAreaWidget::scrollToTop(TimeDelta span)
{
    if(d->origin == Top)
    {
        scrollY(0, span);
    }
    else
    {
        scrollY(maximumScrollY().valuei(), span);
    }
}

void ScrollAreaWidget::scrollToBottom(TimeDelta span)
{
    if(d->origin == Top)
    {
        scrollY(maximumScrollY().valuei(), span);
    }
    else
    {
        scrollY(0, span);
    }
}

void ScrollAreaWidget::scrollToLeft(TimeDelta span)
{
    scrollX(0, span);
}

void ScrollAreaWidget::scrollToRight(TimeDelta span)
{
    scrollX(maximumScrollX().valuei(), span);
}

void ScrollAreaWidget::glInit()
{
    d->glInit();
}

void ScrollAreaWidget::glDeinit()
{
    d->glDeinit();
}

void ScrollAreaWidget::glMakeScrollIndicatorGeometry(DefaultVertexBuf::Builder &verts,
                                                     Vector2f const &origin)
{
    // Draw the scroll indicator.
    if(d->scrollOpacity <= 0) return;

    Vector2i const viewSize = viewportSize();
    if(viewSize == Vector2i(0, 0)) return;

    int const indHeight = de::clamp(
                margins().height().valuei(),
                int(float(viewSize.y * viewSize.y) / float(d->contentRule.height().value())),
                viewSize.y / 2);

    float indPos = scrollPositionY().value() / maximumScrollY().value();
    if(d->origin == Top) indPos = 1 - indPos;

    float const avail = viewSize.y - indHeight;

    verts.makeQuad(Rectanglef(origin + Vector2f(viewSize.x + margins().left().value() - 2 * d->scrollBarWidth,
                                                avail - indPos * avail + indHeight),
                              origin + Vector2f(viewSize.x + margins().left().value() - d->scrollBarWidth,
                                                avail - indPos * avail)),
                   Vector4f(1, 1, 1, d->scrollOpacity) * d->scrollBarColor,
                   d->indicatorUv);
}

void ScrollAreaWidget::viewResized()
{
    GuiWidget::viewResized();
    d->uMvpMatrix = root().projMatrix2D();
}

void ScrollAreaWidget::update()
{
    GuiWidget::update();

    if(d->indicatorAnimating)
    {
        requestGeometry();
    }
    if(d->scrollOpacity.done())
    {
        d->indicatorAnimating = false;
    }

    // Clamp the scroll position.
    if(d->x->value() > d->maxX->value())
    {
        d->x->set(d->maxX->value());
    }
    if(d->y->value() > d->maxY->value())
    {
        d->y->set(d->maxY->value());
    }
}

void ScrollAreaWidget::drawContent()
{
    if(d->indicatorShown)
    {
        d->uColor = Vector4f(1, 1, 1, visibleOpacity());

        // The indicator is quite simple, so just keep it dynamic. This will
        // also avoid the need to detect when the indicator is moving and
        // whether the atlas has been repositioned.

        setIndicatorUv(root().atlas().imageRectf(root().solidWhitePixel()).middle());

        DefaultVertexBuf::Builder verts;
        glMakeScrollIndicatorGeometry(verts, rule().recti().topLeft + margins().toVector().xy());
        d->drawable.buffer<DefaultVertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Dynamic);

        d->drawable.draw();
    }
}

void ScrollAreaWidget::preDrawChildren()
{
    if(behavior().testFlag(ChildVisibilityClipping))
    {
        GLState::push().setNormalizedScissor(normalizedRect());
    }
}

void ScrollAreaWidget::postDrawChildren()
{
    if(behavior().testFlag(ChildVisibilityClipping))
    {
        GLState::pop();
    }
}

} // namespace de
