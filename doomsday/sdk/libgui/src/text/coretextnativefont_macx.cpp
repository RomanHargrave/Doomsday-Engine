/** @file coretextnativefont_macx.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "coretextnativefont_macx.h"
#include <de/Log>

#include <QFont>
#include <QColor>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

namespace de {

struct CoreTextFontCache : public Lockable
{
    struct Key {
        String name;
        dfloat size;

        Key(String const &n = "", dfloat s = 12.f) : name(n), size(s) {}
        bool operator < (Key const &other) const {
            if(name == other.name) {
                return size < other.size && !fequal(size, other.size);
            }
            return name < other.name;
        }
    };

    typedef QMap<Key, CTFontRef> Fonts;
    Fonts fonts;

    CGColorSpaceRef _colorspace; ///< Shared by all fonts.

    CoreTextFontCache() : _colorspace(0)
    {}

    ~CoreTextFontCache()
    {
        clear();
        if(_colorspace)
        {
            CGColorSpaceRelease(_colorspace);
        }
    }

    CGColorSpaceRef colorspace()
    {
        if(!_colorspace)
        {
            _colorspace = CGColorSpaceCreateDeviceRGB();
        }
        return _colorspace;
    }

    void clear()
    {
        DENG2_GUARD(this);

        foreach(CTFontRef ref, fonts.values())
        {
            CFRelease(ref);
        }
    }

    CTFontRef getFont(String const &postScriptName, dfloat pointSize)
    {
        CTFontRef font;

        // Only lock the cache while accessing the fonts database. If we keep the
        // lock while printing log output, a flush might occur, which might in turn
        // lead to text rendering and need for font information -- causing a hang.
        {
            DENG2_GUARD(this);

            Key const key(postScriptName, pointSize);
            if(fonts.contains(key))
            {
                // Already got it.
                return fonts[key];
            }

            // Get a reference to the font.
            CFStringRef name = CFStringCreateWithCharacters(nil, (UniChar *) postScriptName.data(),
                                                            postScriptName.size());
            font = CTFontCreateWithName(name, pointSize, nil);
            CFRelease(name);

            fonts.insert(key, font);
        }

        LOG_GL_VERBOSE("Cached native font '%s' size %.1f") << postScriptName << pointSize;

        return font;
    }

#ifdef DENG2_DEBUG
    float fontSize(CTFontRef font) const
    {
        DENG2_FOR_EACH_CONST(Fonts, i, fonts)
        {
            if(i.value() == font) return i.key().size;
        }
        DENG2_ASSERT(!"Font not in cache");
        return 0;
    }

    int fontWeight(CTFontRef font) const
    {
        DENG2_FOR_EACH_CONST(Fonts, i, fonts)
        {
            if(i.value() == font)
            {
                if(i.key().name.contains("Light")) return 25;
                if(i.key().name.contains("Bold")) return 75;
                return 50;
            }
        }
        DENG2_ASSERT(!"Font not in cache");
        return 0;
    }
#endif
};

static CoreTextFontCache fontCache;

DENG2_PIMPL(CoreTextNativeFont)
{
    enum Transformation { NoTransform, Uppercase, Lowercase };

    CTFontRef font;
    float ascent;
    float descent;
    float height;
    float lineSpacing;

    // Note that while fonts are used from multiple threads, native font instances
    // are copied once per each rich formatting range so we don't need to worry
    // about this cached data being used from many threads.
    String lineText;
    CTLineRef line;

    Transformation xform;

    Instance(Public *i)
        : Base(i)
        , font(0)
        , ascent(0)
        , descent(0)
        , height(0)
        , lineSpacing(0)
        , line(0)
        , xform(NoTransform)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , font(other.font)
        , ascent(other.ascent)
        , descent(other.descent)
        , height(other.height)
        , lineSpacing(other.lineSpacing)
        , line(0)
        , xform(other.xform)
    {}

    ~Instance()
    {
        release();
    }

    String applyTransformation(String const &str) const
    {
        switch(xform)
        {
        case Uppercase:
            return str.toUpper();

        case Lowercase:
            return str.toLower();

        default:
            break;
        }
        return str;
    }

    void release()
    {
        font = 0;
        releaseLine();
    }

    void releaseLine()
    {
        if(line)
        {
            CFRelease(line);
            line = 0;
        }
        lineText.clear();
    }

    void updateFontAndMetrics()
    {
        release();

        // Get a reference to the font.
        font = fontCache.getFont(self.nativeFontName(), self.size());

        // Get basic metrics about the font.
        ascent      = ceil(CTFontGetAscent(font));
        descent     = ceil(CTFontGetDescent(font));
        height      = ascent + descent;
        lineSpacing = height + CTFontGetLeading(font);
    }

    void makeLine(String const &text, CGColorRef color = 0)
    {
        if(lineText == text) return; // Already got it.

        releaseLine();
        lineText = text;

        void const *keys[]   = { kCTFontAttributeName, kCTForegroundColorAttributeName };
        void const *values[] = { font, color };
        CFDictionaryRef attribs = CFDictionaryCreate(nil, keys, values,
                                                     color? 2 : 1, nil, nil);

        CFStringRef textStr = CFStringCreateWithCharacters(nil, (UniChar *) text.data(), text.size());
        CFAttributedStringRef as = CFAttributedStringCreate(0, textStr, attribs);
        line = CTLineCreateWithAttributedString(as);

        CFRelease(attribs);
        CFRelease(textStr);
        CFRelease(as);
    }
};

} // namespace de

namespace de {

CoreTextNativeFont::CoreTextNativeFont(String const &family)
    : NativeFont(family), d(new Instance(this))
{}

CoreTextNativeFont::CoreTextNativeFont(QFont const &font)
    : NativeFont(font.family()), d(new Instance(this))
{
    setSize  (font.pointSizeF());
    setWeight(font.weight());
    setStyle (font.italic()? Italic : Regular);

    d->xform = (font.capitalization() == QFont::AllUppercase? Instance::Uppercase :
                font.capitalization() == QFont::AllLowercase? Instance::Lowercase :
                                                              Instance::NoTransform);
}

CoreTextNativeFont::CoreTextNativeFont(CoreTextNativeFont const &other)
    : NativeFont(other), d(new Instance(this, *other.d))
{
    // If the other is ready, this will be too.
    setState(other.state());
}

CoreTextNativeFont &CoreTextNativeFont::operator = (CoreTextNativeFont const &other)
{
    NativeFont::operator = (other);
    d.reset(new Instance(this, *other.d));
    // If the other is ready, this will be too.
    setState(other.state());
    return *this;
}

void CoreTextNativeFont::commit() const
{
    d->updateFontAndMetrics();
}

int CoreTextNativeFont::nativeFontAscent() const
{
    return roundi(d->ascent);
}

int CoreTextNativeFont::nativeFontDescent() const
{
    return roundi(d->descent);
}

int CoreTextNativeFont::nativeFontHeight() const
{
    return roundi(d->height);
}

int CoreTextNativeFont::nativeFontLineSpacing() const
{
    return roundi(d->lineSpacing);
}

Rectanglei CoreTextNativeFont::nativeFontMeasure(String const &text) const
{
    d->makeLine(d->applyTransformation(text));

    //CGLineGetImageBounds(d->line, d->gc); // more accurate but slow

    Rectanglei rect(Vector2i(0, -d->ascent),
                    Vector2i(roundi(CTLineGetTypographicBounds(d->line, NULL, NULL, NULL)),
                             d->descent));

    return rect;
}

int CoreTextNativeFont::nativeFontWidth(String const &text) const
{
    d->makeLine(d->applyTransformation(text));
    return roundi(CTLineGetTypographicBounds(d->line, NULL, NULL, NULL));
}

QImage CoreTextNativeFont::nativeFontRasterize(String const &text,
                                               Vector4ub const &foreground,
                                               Vector4ub const &background) const
{
#if 0
    DENG2_ASSERT(fequal(fontCache.fontSize(d->font), size()));
    DENG2_ASSERT(fontCache.fontWeight(d->font) == weight());
#endif

    // Text color.
    Vector4d const fg = foreground.zyxw().toVector4f() / 255.f;
    CGColorRef fgColor = CGColorCreate(fontCache.colorspace(), &fg.x);

    // Ensure the color is used by recreating the attributed line string.
    d->releaseLine();
    d->makeLine(d->applyTransformation(text), fgColor);

    // Set up the bitmap for drawing into.
    Rectanglei const bounds = measure(d->lineText);
    QImage backbuffer(QSize(bounds.width(), bounds.height()), QImage::Format_ARGB32);//_Premultiplied);
    backbuffer.fill(QColor(background.x, background.y, background.z, background.w).rgba());

    CGContextRef gc = CGBitmapContextCreate(backbuffer.bits(),
                                            backbuffer.width(),
                                            backbuffer.height(),
                                            8, 4 * backbuffer.width(),
                                            fontCache.colorspace(),
                                            kCGImageAlphaPremultipliedLast);

    CGContextSetTextPosition(gc, 0, d->descent);
    CTLineDraw(d->line, gc);

    CGColorRelease(fgColor);
    CGContextRelease(gc);

    return backbuffer;
}

} // namespace de
