/** @file gltextcomposer.cpp
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

#include "de/GLTextComposer"

#include <QList>

namespace de {

using namespace ui;

static Rangei const MAX_VISIBLE_RANGE(0, 0x7fffffff);

DENG2_PIMPL(GLTextComposer)
{    
    Font const *font = nullptr;
    Atlas *atlas = nullptr;
    String text;
    FontLineWrapping const *wraps = nullptr;
    Font::RichFormat format;
    bool needRedo = false;
    Rangei visibleLineRange { MAX_VISIBLE_RANGE }; ///< Only these lines will be updated/drawn.
    int maxGeneratedWidth = 0;

    struct Line {
        struct Segment {
            Id id;
            Rangei range;
            String text;
            int x;
            int width;
            bool compressed;

            Segment() : id(Id::None), x(0), width(0), compressed(false) {}
            int right() const { return x + width; }
        };
        QList<Segment> segs;       
    };
    typedef QList<Line> Lines;
    Lines lines;

    Instance(Public *i) : Base(i) {}

    ~Instance()
    {
        releaseLines();
    }

    void releaseLines()
    {
        if(atlas)
        {
            for(int i = 0; i < lines.size(); ++i)
            {
                releaseLine(i);
            }
        }
        lines.clear();
    }

    void releaseOutsideRange()
    {
        if(!atlas) return;

        for(int i = 0; i < lines.size(); ++i)
        {
            if(!isLineVisible(i))
            {
                releaseLine(i, ReleaseButKeepSegs);
            }
        }
    }

    enum ReleaseBehavior { ReleaseFully, ReleaseButKeepSegs };

    void releaseLine(int index, ReleaseBehavior behavior = ReleaseFully)
    {
        Line &ln = lines[index];
        for(int i = 0; i < ln.segs.size(); ++i)
        {
            if(!ln.segs[i].id.isNone())
            {
                atlas->release(ln.segs[i].id);
                ln.segs[i].id = Id::None;
            }
        }
        if(behavior == ReleaseFully)
        {
            ln.segs.clear();
        }
    }

    bool isLineVisible(int line) const
    {
        return visibleLineRange.contains(line);
    }

    String segmentText(int seg, FontLineWrapping::LineInfo const &info) const
    {
        return text.substr(info.segs[seg].range);
    }

    bool matchingSegments(int lineIndex, FontLineWrapping::LineInfo const &info) const
    {
        if(info.segs.size() != lines[lineIndex].segs.size())
        {
            //qDebug() << "line" << lineIndex << "number of segs changed";
            return false;
        }
        for(int i = 0; i < info.segs.size(); ++i)
        {
            if(info.segs[i].range != lines[lineIndex].segs[i].range)
            {
                // Range has changed.
                //qDebug() << "line" << lineIndex << "seg" << i << "range change";
                return false;
            }
            if(segmentText(i, info) != lines[lineIndex].segs[i].text)
            {
                // Text has changed.
                //qDebug() << "line" << lineIndex << "seg" << i << "text change";
                return false;
            }
            if(lines[lineIndex].segs[i].id.isNone() && info.segs[i].range.size() > 0)
            {
                // This segment has previously failed allocation.
                //qDebug() << "line" << lineIndex << "seg" << i << "not alloced before, len" <<
                //            info.segs[i].range.size();
                return false;
            }
        }
        return true;
    }

    bool allocLines()
    {
        bool changed = false;

        for(int i = 0; i < wraps->height(); ++i)
        {
            FontLineWrapping::LineInfo const &info = wraps->lineInfo(i);

            if(i < lines.size())
            {
                // Is the rasterized copy up to date?
                if(/*!isLineVisible(i) ||*/ matchingSegments(i, info))
                {
                    // This line can be kept as is.
                    continue;
                }

                // Needs to be redone.
                releaseLine(i);
            }

            changed = true;

            if(i >= lines.size())
            {
                // Need another line.
                lines << Line();
            }

            DENG2_ASSERT(i < lines.size());
            DENG2_ASSERT(lines[i].segs.isEmpty());

            Line &line = lines[i];
            for(int k = 0; k < info.segs.size(); ++k)
            {
                Line::Segment seg;
                seg.range = info.segs[k].range;
                seg.text = segmentText(k, info);
                if(isLineVisible(i) && seg.range.size() > 0)
                {
                    // The color is white unless a style is defined.
                    Vector4ub fgColor(255, 255, 255, 255);

                    if(format.hasStyle())
                    {
                        fgColor = format.style().richStyleColor(Font::RichFormat::NormalColor);
                    }

                    // Set up the background color to be transparent with no
                    // change of color in the alphablended smooth edges.
                    Vector4ub bgColor = fgColor;
                    bgColor.w = 0;

                    seg.id = atlas->alloc(font->rasterize(seg.text, format.subRange(seg.range),
                                                          fgColor, bgColor));
                }
                line.segs << seg;
            }

            DENG2_ASSERT(line.segs.size() == info.segs.size());
        }

        // Remove the excess lines.
        while(lines.size() > wraps->height())
        {
            releaseLine(lines.size() - 1);
            lines.removeLast();
            changed = true;
        }

        DENG2_ASSERT(wraps->height() == lines.size());

        return changed;
    }

    void updateLineLayout(Rangei const &lineRange)
    {
        if(lineRange.isEmpty()) return;

        Rangei current = lineRange;
        forever
        {
            int end = updateLineLayoutUntilUntabbed(current);
            if(end == lineRange.end)
            {
                break; // Whole range done.
            }
            current = Rangei(end, lineRange.end);
        }
    }

    /**
     * Attempts to update lines in the specified range, but stops if an
     * untabbed line is encountered. This ensures that each distinct tabbed
     * content subrange uses its own alignment.
     *
     * @param lineRange  Range of lines to update.
     *
     * @return The actual end of the updated range.
     */
    inline int updateLineLayoutUntilUntabbed(Rangei const &lineRange)
    {
        bool includesTabbedLines = false;
        int rangeEnd = lineRange.end;

        // Find the highest tab in use and initialize seg widths.
        int highestTab = 0;
        for(int i = lineRange.start; i < lineRange.end; ++i)
        {
            int lineStop = wraps->lineInfo(i).highestTabStop();
            if(lineStop >= 0)
            {
                // The range now includes at least one tabbed line.
                includesTabbedLines = true;
            }
            if(lineStop < 0)
            {
                // This is an untabbed line.
                if(!includesTabbedLines)
                {
                    // We can do many untabbed lines in the range as long as
                    // there are no tabbed ones.
                    rangeEnd = i + 1;
                }
                else
                {
                    // An untabbed line will halt the process for now.
                    rangeEnd = de::max(i, lineRange.start + 1);
                    break;
                }
            }
            highestTab = de::max(highestTab, lineStop);

            // Initialize the segments with indentation.
            for(int k = 0; k < lines[i].segs.size(); ++k)
            {
                lines[i].segs[k].width = wraps->lineInfo(i).segs[k].width;
            }
        }

        DENG2_ASSERT(rangeEnd > lineRange.start);

        // Set segment X coordinates by stacking them left-to-right on each line.
        for(int i = lineRange.start; i < rangeEnd; ++i)
        {
            if(lines[i].segs.isEmpty() || i >= visibleLineRange.end)
                continue;

            lines[i].segs[0].x = wraps->lineInfo(i).indent;

            for(int k = 1; k < lines[i].segs.size(); ++k)
            {
                Instance::Line::Segment &seg = lines[i].segs[k];
                seg.x = lines[i].segs[k - 1].right();
            }
        }

        // Align each tab stop with other matching stops on the other lines.
        for(int tab = 1; tab <= highestTab; ++tab)
        {
            int maxRight = 0;

            // Find the maximum right edge for this spot.
            for(int i = lineRange.start; i < rangeEnd; ++i)
            {
                if(i >= visibleLineRange.end) break;

                FontLineWrapping::LineInfo const &info = wraps->lineInfo(i);                                               

                DENG2_ASSERT(info.segs.size() == lines[i].segs.size());
                for(int k = 0; k < info.segs.size(); ++k)
                {
                    Instance::Line::Segment &seg = lines[i].segs[k];
                    if(info.segs[k].tabStop >= 0 && info.segs[k].tabStop < tab)
                    {
                        maxRight = de::max(maxRight, seg.right());
                    }
                }
            }

            // Move the segments to this position.
            for(int i = lineRange.start; i < rangeEnd; ++i)
            {
                if(i >= visibleLineRange.end) break;

                int localRight = maxRight;

                FontLineWrapping::LineInfo const &info = wraps->lineInfo(i);
                for(int k = 0; k < info.segs.size(); ++k)
                {
                    if(info.segs[k].tabStop == tab)
                    {
                        lines[i].segs[k].x = localRight;
                        localRight += info.segs[k].width;
                    }
                }
            }
        }

        return rangeEnd;
    }
};

GLTextComposer::GLTextComposer() : d(new Instance(this))
{}

void GLTextComposer::release()
{
    d->releaseLines();
    d->visibleLineRange = MAX_VISIBLE_RANGE;
    setState(false);
}

void GLTextComposer::releaseLinesOutsideRange()
{
    d->releaseOutsideRange();
}

void GLTextComposer::setAtlas(Atlas &atlas)
{
    d->atlas = &atlas;
}

void GLTextComposer::setWrapping(FontLineWrapping const &wrappedLines)
{
    if(d->wraps != &wrappedLines)
    {
        d->wraps = &wrappedLines;
        forceUpdate();
    }
}

void GLTextComposer::setText(String const &text)
{
    setText(text, Font::RichFormat::fromPlainText(text));
}

void GLTextComposer::setStyledText(String const &styledText)
{
    d->format.clear();
    d->text = d->format.initFromStyledText(styledText);
    setState(false);
}

void GLTextComposer::setText(String const &text, Font::RichFormat const &format)
{
    d->text = text;
    d->format = format;
    setState(false);
}

void GLTextComposer::setRange(Rangei const &visibleLineRange)
{
    d->visibleLineRange = visibleLineRange;
}

Rangei GLTextComposer::range() const
{
    return d->visibleLineRange;
}

bool GLTextComposer::update()
{
    DENG2_ASSERT(d->wraps != 0);

    // If a font hasn't been defined, there isn't much to do.
    if(!d->wraps->hasFont()) return false;

    if(d->font != &d->wraps->font())
    {
        d->font = &d->wraps->font();
        forceUpdate();
    }

    if(d->needRedo)
    {
        d->releaseLines();
        d->needRedo = false;
    }

    setState(true);

    return d->allocLines();
}

void GLTextComposer::forceUpdate()
{
    d->needRedo = true;
}

void GLTextComposer::makeVertices(Vertices &triStrip,
                                  Vector2i const &topLeft,
                                  Alignment const &lineAlign,
                                  Vector4f const &color)
{
    makeVertices(triStrip, Rectanglei(topLeft, topLeft), AlignTopLeft, lineAlign, color);
}

void GLTextComposer::makeVertices(Vertices &triStrip,
                                  Rectanglei const &rect,
                                  Alignment const &alignInRect,
                                  Alignment const &lineAlign,
                                  Vector4f const &color)
{
    if(!isReady()) return;

    DENG2_ASSERT(d->wraps != 0);
    DENG2_ASSERT(d->font != 0);

    Vector2i const contentSize(d->wraps->width(), d->wraps->totalHeightInPixels());

    // Apply alignment within the provided rectangle.
    Vector2f p = applyAlignment(alignInRect, contentSize, rect);

    DENG2_ASSERT(d->wraps->height() == d->lines.size());

    // Align segments based on tab stops.
    d->updateLineLayout(Rangei(0, d->lines.size()));

    // Compress lines to fit into the maximum allowed width.
    for(int i = 0; i < d->lines.size(); ++i)
    {
        Instance::Line &line = d->lines[i];
        if(!d->isLineVisible(i) || line.segs.isEmpty()) continue;

        /*
        if(!d->wraps->lineInfo(i).segs.last().tabStop)
            continue;
            */

        Instance::Line::Segment &seg = line.segs.last();
        int const leeway = 3;
        if(seg.right() > d->wraps->maximumWidth() + leeway)
        {
            // Needs compressing (up to 10%).
            seg.compressed = true;
            seg.width = de::max(int(seg.width * .9f), d->wraps->maximumWidth() + leeway - seg.x);
        }
    }

    d->maxGeneratedWidth = 0;

    // Generate vertices for each line.
    for(int i = 0; i < d->wraps->height(); ++i)
    {
        Instance::Line const &line = d->lines[i];

        if(d->isLineVisible(i))
        {
            FontLineWrapping::LineInfo const &info = d->wraps->lineInfo(i);
            Vector2f linePos = p;

            for(int k = 0; k < info.segs.size(); ++k)
            {
                Instance::Line::Segment const &seg = line.segs[k];

                // Empty lines are skipped.
                if(seg.id.isNone()) continue;

                Vector2ui size = d->atlas->imageRect(seg.id).size();
                if(seg.compressed)
                {
                    size.x = seg.width;
                }

                // Line alignment.
                /// @todo How to center/right-align text that uses tab stops?
                if(line.segs.size() == 1 && d->wraps->lineInfo(0).segs[0].tabStop < 0)
                {
                    if(lineAlign.testFlag(AlignRight))
                    {
                        linePos.x += int(contentSize.x) - int(size.x);
                    }
                    else if(!lineAlign.testFlag(AlignLeft))
                    {
                        linePos.x += (int(contentSize.x) - int(size.x)) / 2;
                    }
                }

                Rectanglef const uv = d->atlas->imageRectf(seg.id);

                auto const segRect = Rectanglef::fromSize(linePos + Vector2f(seg.x, 0), size);
                triStrip.makeQuad(segRect, color, uv);

                // Keep track of how wide the geometry really is.
                d->maxGeneratedWidth = de::max(d->maxGeneratedWidth, int(segRect.right() - p.x));
            }
        }

        p.y += d->font->lineSpacing().value();
    }
}

int GLTextComposer::verticesMaxWidth() const
{
    return d->maxGeneratedWidth;
}

} // namespace de
