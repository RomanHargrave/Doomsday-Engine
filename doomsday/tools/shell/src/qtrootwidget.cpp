/** @file qtrootwidget.cpp  Root widget that works with a Qt canvas->
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

#include "qtrootwidget.h"
#include "qttextcanvas.h"
#include <de/shell/KeyEvent>
#include <de/AnimationVector>
#include <de/Clock>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#ifdef DENG2_QT_5_0_OR_NEWER
#  include <QGuiApplication>
#endif

using namespace de;
using namespace de::shell;

static int const REFRESH_INTERVAL = 1000 / 30; // ms
static int const BLINK_INTERVAL   = 500;       // ms

#ifdef MACOSX
#  define CONTROL_MOD   Qt::MetaModifier
#else
#  define CONTROL_MOD   Qt::ControlModifier
#endif

DENG2_PIMPL(QtRootWidget)
{
    int margin;
    Vector2i charSize;
    float dpiFactor;
    QtTextCanvas *canvas;
    TextRootWidget root;
    QFont font;
    QFont overlayFont;
    QTimer *blinkTimer;
    QTimer *cursorTimer;
    bool blinkVisible;
    bool cursorVisible;
    QPoint origin;
    QString overlay;

    Instance(Public &inst)
        : Base(inst)
        , margin(4)
        , dpiFactor(1)
        , canvas(new QtTextCanvas(Vector2ui(1, 1)))
        , root(canvas)
        , blinkTimer(0)
        , cursorTimer(0)
        , blinkVisible(true)
        , cursorVisible(true)
    {
#ifdef DENG2_QT_5_1_OR_NEWER
        dpiFactor = qApp->devicePixelRatio();
#endif
        canvas->setForegroundColor(Qt::black);
        canvas->setBackgroundColor(Qt::white);
    }

    void setFont(QFont const &fnt)
    {
        font = fnt;

        QFontMetrics metrics(font);
        charSize.x = metrics.width('W');
        charSize.y = metrics.lineSpacing();

        canvas->setFont(font);
        canvas->setCharSize(charSize);
    }

    void updateSize(int widthPx, int heightPx)
    {
        if(!charSize.x || !charSize.y) return;

        // Determine number of characters that fits in the new size.
        Vector2ui size((widthPx - 2*margin) / charSize.x, (heightPx - 2*margin) / charSize.y);
        root.setViewSize(size);

        origin = QPoint(margin, heightPx - canvas->image().height()/dpiFactor - margin);
    }
};

QtRootWidget::QtRootWidget(QWidget *parent)
    : QWidget(parent), d(new Instance(*this))
{
    setFocusPolicy(Qt::StrongFocus);

    // Continually check for need to update.
    QTimer *refresh = new QTimer(this);
    connect(refresh, SIGNAL(timeout()), this, SLOT(updateIfRequested()));
    refresh->start(REFRESH_INTERVAL);

    // Blinking timers.
    d->blinkTimer = new QTimer(this);
    connect(d->blinkTimer, SIGNAL(timeout()), this, SLOT(blink()));
    d->blinkTimer->start(BLINK_INTERVAL);

    d->cursorTimer = new QTimer(this);
    connect(d->cursorTimer, SIGNAL(timeout()), this, SLOT(cursorBlink()));
    d->cursorTimer->start(BLINK_INTERVAL);
}

TextRootWidget &QtRootWidget::rootWidget()
{
    return d->root;
}

QtTextCanvas &QtRootWidget::canvas()
{
    return *d->canvas;
}

void QtRootWidget::setFont(QFont const &font)
{
    d->setFont(font);
    d->updateSize(width(), height());

    setMinimumSize(d->charSize.x * 40 + 2 * d->margin,
                   d->charSize.y * 6 + 2 * d->margin);

    d->overlayFont = QWidget::font();
    d->overlayFont.setBold(true);
    d->overlayFont.setPixelSize(24);
}

void QtRootWidget::setOverlaidMessage(const QString &msg)
{
    d->overlay = msg;
    update();
}

void QtRootWidget::keyPressEvent(QKeyEvent *ev)
{
    bool eaten;

    /*
    qDebug() << "key:" << QString::number(ev->key(), 16) << "text:" << ev->text()
             << "mods:" << ev->modifiers();
    */

    if(!ev->text().isEmpty() && ev->text()[0].isPrint() &&
            !ev->modifiers().testFlag(CONTROL_MOD))
    {
        eaten = d->root.processEvent(KeyEvent(ev->text()));
    }
    else
    {
        int key = ev->key();
        KeyEvent::Modifiers mods = ev->modifiers().testFlag(CONTROL_MOD)?
                    KeyEvent::Control : KeyEvent::None;

        if(key == Qt::Key_Return) key = Qt::Key_Enter;

        // Special control key mappings.
        if(mods & KeyEvent::Control)
        {
            switch(key)
            {
            case Qt::Key_A:
                key = Qt::Key_Home;
                mods = KeyEvent::None;
                break;

            case Qt::Key_D:
                key = Qt::Key_Delete;
                mods = KeyEvent::None;
                break;

            case Qt::Key_E:
                key = Qt::Key_End;
                mods = KeyEvent::None;
                break;

            default:
                break;
            }
        }

        eaten = d->root.processEvent(KeyEvent(key, mods));
    }

    if(eaten)
    {
        ev->accept();

        // Restart cursor blink.
        d->cursorVisible = true;
        d->cursorTimer->stop();
        d->cursorTimer->start(BLINK_INTERVAL);

        update();
    }
    else
    {
        ev->ignore();
        QWidget::keyPressEvent(ev);
    }
}

void QtRootWidget::resizeEvent(QResizeEvent *ev)
{
    d->updateSize(width(), height());

    QWidget::resizeEvent(ev);
}

void QtRootWidget::paintEvent(QPaintEvent *)
{
    Clock::get().setTime(Time());

    d->canvas->setBlinkVisible(d->blinkVisible);

    // Update changed portions.
    d->root.update();
    d->root.draw();

    QSize size(width(), height());

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.fillRect(QRect(QPoint(0, 0), size), d->canvas->backgroundColor());

    QImage const &buf = d->canvas->image();
    QPoint origin = d->origin;

    painter.drawImage(QRect(origin, buf.size()/d->dpiFactor), buf);

    // Blinking cursor.
    if(d->cursorVisible)
    {
        QPoint pos = origin + QPoint(d->charSize.x * d->canvas->cursorPosition().x,
                                     d->charSize.y * d->canvas->cursorPosition().y);

        TextCanvas::Char ch = d->canvas->at(d->canvas->cursorPosition());

        painter.setPen(Qt::NoPen);
        painter.fillRect(QRect(pos, QSize(de::max(1, d->charSize.x / 5), d->charSize.y)),
                         ch.attribs.testFlag(TextCanvas::Char::Reverse)?
                             d->canvas->backgroundColor() : d->canvas->foregroundColor());
    }

    // Overlaid message?
    if(!d->overlay.isEmpty())
    {
        painter.setFont(d->overlayFont);
        painter.setBrush(Qt::NoBrush);
        QColor fg = d->canvas->foregroundColor();
        painter.setPen(Qt::white);
        painter.drawText(QRect(2, 2, width(), height()), d->overlay, QTextOption(Qt::AlignCenter));
        painter.setPen(fg);
        painter.drawText(rect(), d->overlay, QTextOption(Qt::AlignCenter));
    }
}

void QtRootWidget::updateIfRequested()
{
    if(d->root.drawWasRequested())
    {
        update();
    }
}

void QtRootWidget::blink()
{
    d->blinkVisible = !d->blinkVisible;
    update();
}

void QtRootWidget::cursorBlink()
{
    d->cursorVisible = !d->cursorVisible;
    update();
}
