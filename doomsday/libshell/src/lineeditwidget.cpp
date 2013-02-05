/** @file lineeditwidget.cpp  Widget for word-wrapped text editing.
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

#include <de/String>
#include <de/RuleRectangle>
#include "de/shell/LineEditWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"

namespace de {
namespace shell {

struct LineEditWidget::Instance
{
    LineEditWidget &self;
    ConstantRule *height;
    bool signalOnEnter;
    String prompt;
    String text;
    int cursor; ///< Index in range [0...text.size()]

    // Word wrapping.
    LineWrapping wraps;

    Instance(LineEditWidget &cli)
        : self(cli),
          signalOnEnter(true),
          cursor(0)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);

        wraps.append(WrappedLine(0, 0, true));
    }

    ~Instance()
    {
        releaseRef(height);
    }

    /**
     * Determines where word wrapping needs to occur and updates the height of
     * the widget to accommodate all the needed lines.
     */
    void updateWrapsAndHeight()
    {
        wraps.wrapTextToWidth(text, de::max(1, self.rule().recti().width() - prompt.size() - 1));
        height->set(wraps.height());
    }

    WrappedLine lineSpan(int line) const
    {
        DENG2_ASSERT(line < wraps.size());
        return wraps[line];
    }

    /**
     * Calculates the visual position of the cursor (of the current command),
     * including the line that it is on.
     */
    de::Vector2i lineCursorPos() const
    {
        de::Vector2i pos(cursor);
        for(pos.y = 0; pos.y < wraps.size(); ++pos.y)
        {
            WrappedLine span = lineSpan(pos.y);
            if(!span.isFinal) span.end--;
            if(cursor >= span.start && cursor <= span.end)
            {
                // Stop here. Cursor is on this line.
                break;
            }
            pos.x -= span.end - span.start + 1;
        }
        return pos;
    }

    /**
     * Attemps to move the cursor up or down by a line.
     *
     * @return @c true, if cursor was moved. @c false, if there were no more
     * lines available in that direction.
     */
    bool moveCursorByLine(int lineOff)
    {
        DENG2_ASSERT(lineOff == 1 || lineOff == -1);

        de::Vector2i const linePos = lineCursorPos();

        // Check for no room.
        if(!linePos.y && lineOff < 0) return false;
        if(linePos.y == wraps.size() - 1 && lineOff > 0) return false;

        // Move cursor onto the adjacent line.
        WrappedLine span = lineSpan(linePos.y + lineOff);
        cursor = span.start + linePos.x;
        if(!span.isFinal) span.end--;
        if(cursor > span.end) cursor = span.end;
        return true;
    }

    void insert(String const &str)
    {
        text.insert(cursor++, str);
    }

    void doBackspace()
    {
        if(!text.isEmpty() && cursor > 0)
        {
            text.remove(--cursor, 1);
        }
    }

    void doDelete()
    {
        if(text.size() > cursor)
        {
            text.remove(cursor, 1);
        }
    }

    void doLeft()
    {
        if(cursor > 0) --cursor;
    }

    void doRight()
    {
        if(cursor < text.size()) ++cursor;
    }

    void doHome()
    {
        cursor = lineSpan(lineCursorPos().y).start;
    }

    void doEnd()
    {
        WrappedLine const span = lineSpan(lineCursorPos().y);
        cursor = span.end - (span.isFinal? 0 : 1);
    }

    void killEndOfLine()
    {
        text.remove(cursor, lineSpan(lineCursorPos().y).end - cursor);
    }
};

LineEditWidget::LineEditWidget(de::String const &name)
    : TextWidget(name), d(new Instance(*this))
{
    setBehavior(HandleEventsOnlyWhenFocused);

    rule().setInput(Rule::Height, *d->height);
}

LineEditWidget::~LineEditWidget()
{
    delete d;
}

void LineEditWidget::setPrompt(String const &promptText)
{
    d->prompt = promptText;
    d->wraps.clear();

    if(hasRoot())
    {
        d->updateWrapsAndHeight();
        redraw();
    }
}

Vector2i LineEditWidget::cursorPosition() const
{
    de::Rectanglei pos = rule().recti();
    return pos.topLeft + Vector2i(d->prompt.size(), 0) + d->lineCursorPos();
}

void LineEditWidget::viewResized()
{
    d->updateWrapsAndHeight();
}

void LineEditWidget::update()
{
    if(d->wraps.isEmpty())
    {
        d->updateWrapsAndHeight();
    }
}

void LineEditWidget::draw()
{
    Rectanglei pos = rule().recti();

    // Temporary buffer for drawing.
    TextCanvas buf(pos.size());

    TextCanvas::Char::Attribs attr =
            (hasFocus()? TextCanvas::Char::Reverse : TextCanvas::Char::DefaultAttributes);
    buf.clear(TextCanvas::Char(' ', attr));

    buf.drawText(Vector2i(0, 0), d->prompt, attr | TextCanvas::Char::Bold);
    buf.drawWrappedText(Vector2i(d->prompt.size(), 0), d->text, d->wraps, attr);

    targetCanvas().draw(buf, pos.topLeft);
}

bool LineEditWidget::handleEvent(Event const *event)
{
    // There are only key press events.
    DENG2_ASSERT(event->type() == Event::KeyPress);
    KeyEvent const *ev = static_cast<KeyEvent const *>(event);

    bool eaten = true;

    // Insert text?
    if(!ev->text().isEmpty())
    {
        d->insert(ev->text());
    }
    else
    {
        // Control character.
        eaten = handleControlKey(ev->key());
    }

    if(eaten)
    {
        d->updateWrapsAndHeight();
        redraw();
        return true;
    }

    return TextWidget::handleEvent(event);
}

bool LineEditWidget::handleControlKey(int key)
{
    switch(key)
    {
    case Qt::Key_Backspace:
        d->doBackspace();
        return true;

    case Qt::Key_Delete:
        d->doDelete();
        return true;

    case Qt::Key_Left:
        d->doLeft();
        return true;

    case Qt::Key_Right:
        d->doRight();
        return true;

    case Qt::Key_Home:
        d->doHome();
        return true;

    case Qt::Key_End:
        d->doEnd();
        return true;

    case Qt::Key_K: // assuming Control mod
        d->killEndOfLine();
        return true;

    case Qt::Key_Up:
        // First try moving within the current command.
        if(!d->moveCursorByLine(-1)) return false; // not eaten
        return true;

    case Qt::Key_Down:
        // First try moving within the current command.
        if(!d->moveCursorByLine(+1)) return false; // not eaten
        return true;

    case Qt::Key_Enter:
        if(d->signalOnEnter)
        {
            emit enterPressed(d->text);
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

void LineEditWidget::setText(String const &contents)
{
    d->text = contents;
    d->cursor = contents.size();
    d->wraps.clear();

    if(hasRoot())
    {
        d->updateWrapsAndHeight();
        redraw();
    }
}

String LineEditWidget::text() const
{
    return d->text;
}

void LineEditWidget::setCursor(int index)
{
    d->cursor = index;
    redraw();
}

int LineEditWidget::cursor() const
{
    return d->cursor;
}

void LineEditWidget::setSignalOnEnter(int enterSignal)
{
    d->signalOnEnter = enterSignal;
}

} // namespace shell
} // namespace de
