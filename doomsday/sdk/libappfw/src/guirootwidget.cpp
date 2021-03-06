/** @file guirootwidget.cpp  Graphical root widget.
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

#include "de/GuiRootWidget"
#include "de/GuiWidget"
#include "de/BaseGuiApp"
#include "de/Style"
#include "de/BaseWindow"

#include <de/CanvasWindow>
#include <de/TextureBank>
#include <de/GLUniform>
#include <de/GLTarget>
#include <de/GLState>

#include <QImage>
#include <QPainter>

namespace de {

// Identifiers for images generated by GuiRootWidget.
static DotPath const ID_SOLID_WHITE        = "GuiRootWidget.solid.white";
static DotPath const ID_THIN_ROUND_CORNERS = "GuiRootWidget.frame.thin";
static DotPath const ID_BOLD_ROUND_CORNERS = "GuiRootWidget.frame.bold";
static DotPath const ID_DOT                = "GuiRootWidget.dot";

#ifdef DENG2_QT_5_0_OR_NEWER
#  define DPI_SCALED(x)       ((x) * qApp->devicePixelRatio())
#else
#  define DPI_SCALED(x)       (x)
#endif

DENG2_PIMPL(GuiRootWidget)
, DENG2_OBSERVES(Widget, ChildAddition)
{
    /*
     * Built-in runtime-generated images:
     */
    struct SolidWhiteImage : public TextureBank::ImageSource {
        Image load() const {
            return Image::solidColor(Image::Color(255, 255, 255, 255),
                                     Image::Size(1, 1));
        }
    };
    struct ThinCornersImage : public TextureBank::ImageSource {
        Image load() const {
            QImage img(QSize(DPI_SCALED(15), DPI_SCALED(15)), QImage::Format_ARGB32);
            img.fill(QColor(255, 255, 255, 0).rgba());
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Qt::white, DPI_SCALED(1)));
            painter.drawEllipse(DPI_SCALED(QPointF(8, 8)), DPI_SCALED(6), DPI_SCALED(6));
            return img;
        }
    };
    struct BoldCornersImage : public TextureBank::ImageSource {
        Image load() const {
            QImage img(QSize(DPI_SCALED(12), DPI_SCALED(12)), QImage::Format_ARGB32);
            img.fill(QColor(255, 255, 255, 0).rgba());
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(255, 255, 255, 255), DPI_SCALED(2)));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(DPI_SCALED(QPointF(6, 6)), DPI_SCALED(4), DPI_SCALED(4));
            return img;
        }
    };
    struct TinyDotImage : public TextureBank::ImageSource {
        Image load() const {
            QImage img(QSize(DPI_SCALED(5), DPI_SCALED(5)), QImage::Format_ARGB32);
            img.fill(QColor(255, 255, 255, 0).rgba());
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.drawEllipse(DPI_SCALED(QPointF(2.5f, 2.5f)), DPI_SCALED(2), DPI_SCALED(2));
            return img;
        }
    };
    struct StyleImage : public TextureBank::ImageSource {
        StyleImage(DotPath const &id) : ImageSource(id) {}
        Image load() const {
            return Style::get().images().image(id());
        }
    };

    CanvasWindow *window;
    QScopedPointer<AtlasTexture> atlas; ///< Shared atlas for most UI graphics/text.
    GLUniform uTexAtlas;
    TextureBank texBank; ///< Bank for the atlas contents.
    bool noFramesDrawnYet;

    Instance(Public *i, CanvasWindow *win)
        : Base(i)
        , window(win)
        , atlas(0)
        , uTexAtlas("uTex", GLUniform::Sampler2D)
        , noFramesDrawnYet(true)
    {
        self.audienceForChildAddition() += this;
    }

    ~Instance()
    {
        GuiWidget::recycleTrashedWidgets();

        // Tell all widgets to release their resource allocations. The base
        // class destructor will destroy all widgets, but this class governs
        // shared GL resources, so we'll ask the widgets to do this now.
        self.notifyTree(&Widget::deinitialize);

        // Destroy GUI widgets while the shared resources are still available.
        self.clearTree();
    }

    void initAtlas()
    {
        if(atlas.isNull())
        {
            atlas.reset(AtlasTexture::newWithKdTreeAllocator(
                            Atlas::BackingStore | Atlas::AllowDefragment,
                            GLTexture::maximumSize().min(GLTexture::Size(4096, 4096))));
            uTexAtlas = *atlas;
            texBank.setAtlas(*atlas);

            // Load a set of general purpose textures (derived classes may extend this).
            self.loadCommonTextures();
        }
    }

    void initBankContents()
    {
        // Built-in images.
        texBank.add(ID_SOLID_WHITE,        new SolidWhiteImage);
        texBank.add(ID_THIN_ROUND_CORNERS, new ThinCornersImage);
        texBank.add(ID_BOLD_ROUND_CORNERS, new BoldCornersImage);
        texBank.add(ID_DOT,                new TinyDotImage);

        // All style images.
        Style const &st = Style::get();
        ImageBank::Names imageNames;
        st.images().allItems(imageNames);
        foreach(String const &name, imageNames)
        {
            texBank.add("Style." + name, new StyleImage(name));
        }
    }

    void widgetChildAdded(Widget &child)
    {
        // Make sure newly added children know the view size.
        child.viewResized();
        child.notifyTree(&Widget::viewResized);
    }
};

GuiRootWidget::GuiRootWidget(CanvasWindow *window)
    : d(new Instance(this, window))
{}

void GuiRootWidget::setWindow(CanvasWindow *window)
{
    d->window = window;
}

CanvasWindow &GuiRootWidget::window()
{
    DENG2_ASSERT(d->window != 0);
    return *d->window;
}

void GuiRootWidget::addOnTop(GuiWidget *widget)
{
    add(widget);
}

void GuiRootWidget::moveToTop(GuiWidget &widget)
{
    if(widget.parentWidget())
    {
        widget.parentWidget()->remove(widget);
    }
    addOnTop(&widget);
}

AtlasTexture &GuiRootWidget::atlas()
{
    d->initAtlas();
    return *d->atlas;
}

GLUniform &GuiRootWidget::uAtlas()
{
    d->initAtlas();
    return d->uTexAtlas;
}

Id GuiRootWidget::solidWhitePixel() const
{
    d->initAtlas();
    return d->texBank.texture(ID_SOLID_WHITE);
}

Id GuiRootWidget::roundCorners() const
{
    d->initAtlas();
    return d->texBank.texture(ID_THIN_ROUND_CORNERS);
}

Id GuiRootWidget::boldRoundCorners() const
{
    d->initAtlas();
    return d->texBank.texture(ID_BOLD_ROUND_CORNERS);
}

Id GuiRootWidget::borderGlow() const
{
    d->initAtlas();
    return d->texBank.texture("Style.window.borderglow");
}

Id GuiRootWidget::tinyDot() const
{
    d->initAtlas();
    return d->texBank.texture(ID_DOT);
}

Id GuiRootWidget::styleTexture(DotPath const &styleImagePath) const
{
    d->initAtlas();
    return d->texBank.texture(String("Style.") + styleImagePath);
}

GLShaderBank &GuiRootWidget::shaders()
{
    return BaseGuiApp::shaders();
}

Matrix4f GuiRootWidget::projMatrix2D() const
{
    RootWidget::Size const size = viewSize();
    return Matrix4f::ortho(0, size.x, 0, size.y);
}

void GuiRootWidget::routeMouse(Widget *routeTo)
{
    setEventRouting(QList<int>()
                    << Event::MouseButton
                    << Event::MouseMotion
                    << Event::MousePosition
                    << Event::MouseWheel, routeTo);
}

void GuiRootWidget::dispatchLatestMousePosition()
{}

bool GuiRootWidget::processEvent(Event const &event)
{
    if(!RootWidget::processEvent(event))
    {
        if(event.type() == Event::MouseButton)
        {
            // Button events that no one handles will relinquish input focus.
            setFocus(0);
        }
        return false;
    }
    return true;
}

void GuiRootWidget::handleEventAsFallback(Event const &)
{}

void GuiRootWidget::loadCommonTextures()
{   
    d->initBankContents();
}

GuiWidget const *GuiRootWidget::globalHitTest(Vector2i const &pos) const
{
    Widget::Children const childs = children();
    for(int i = childs.size() - 1; i >= 0; --i)
    {
        if(GuiWidget const *w = childs.at(i)->maybeAs<GuiWidget>())
        {
            if(GuiWidget const *hit = w->treeHitTest(pos))
            {
                return hit;
            }
        }
    }
    return 0;
}

GuiWidget const *GuiRootWidget::guiFind(String const &name) const
{
    return find(name)->maybeAs<GuiWidget>();
}

void GuiRootWidget::update()
{
    if(window().canvas().isGLReady())
    {
        // Allow GL operations.
        window().canvas().makeCurrent();

        RootWidget::update();

        // Request a window draw so that the updated content becomes visible.
        window().as<BaseWindow>().draw();
    }
}

void GuiRootWidget::draw()
{
    if(d->noFramesDrawnYet)
    {
        // Widgets may not yet be ready on the first frame; make sure
        // we don't show garbage.
        window().canvas().renderTarget().clear(GLTarget::Color);

        d->noFramesDrawnYet = false;
    }

#ifdef DENG2_DEBUG
    // Detect mistakes in GLState stack usage.
    dsize const depthBeforeDrawing = GLState::stackDepth();
#endif

    RootWidget::draw();

    DENG2_ASSERT(GLState::stackDepth() == depthBeforeDrawing);
}

void GuiRootWidget::drawUntil(Widget &until)
{
    NotifyArgs args(&Widget::draw);
    args.conditionFunc  = &Widget::isVisible;
    args.preNotifyFunc  = &Widget::preDrawChildren;
    args.postNotifyFunc = &Widget::postDrawChildren;
    args.until          = &until;
    notifyTree(args);
}

} // namespace de
