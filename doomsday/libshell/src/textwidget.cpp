/** @file textwidget.cpp  Generic widget with a text-based visual.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/TextWidget"
#include "de/shell/TextRootWidget"

namespace de {
namespace shell {

struct TextWidget::Instance
{
    TextCanvas *canvas;
    RectangleRule *rule;

    Instance() : canvas(0), rule(new RectangleRule)
    {}

    ~Instance()
    {
        releaseRef(rule);
    }
};

TextWidget::TextWidget(String const &name) : Widget(name), d(new Instance)
{}

TextWidget::~TextWidget()
{
    delete d;
}

TextRootWidget &TextWidget::root() const
{
    TextRootWidget *r = dynamic_cast<TextRootWidget *>(&Widget::root());
    DENG2_ASSERT(r != 0);
    return *r;
}

void TextWidget::setTargetCanvas(TextCanvas *canvas)
{
    d->canvas = canvas;
}

TextCanvas *TextWidget::targetCanvas() const
{
    if(!d->canvas)
    {
        // A specific target not defined, use the root canvas.
        return &root().rootCanvas();
    }
    return d->canvas;
}

void TextWidget::drawAndShow()
{
    if(targetCanvas())
    {
        draw();
        notifyTree(&Widget::draw);
        targetCanvas()->show();
    }
}

void TextWidget::setRule(RectangleRule *rule)
{
    releaseRef(d->rule);
    d->rule = holdRef(rule);
}

RectangleRule &TextWidget::rule()
{
    DENG2_ASSERT(d->rule != 0);
    return *d->rule;
}

Vector2i TextWidget::cursorPosition()
{
    return Vector2i(floor(rule().left()->value()),
                    floor(rule().top()->value()));
}

} // namespace shell
} // namespace de
