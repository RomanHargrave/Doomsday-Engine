/** @file monospacelogsinkformatter.cpp  Fixed-width log entry formatter.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/MonospaceLogSinkFormatter"
#include "de/EscapeParser"
#include <de/math.h>

#include <QVector>
#include <de/Vector>

namespace de {

/**
 * Parses the tab escapes in a styled text and fills them using spaces. All lines of the
 * input text are handled, however they must be separated by newline characters.
 */
struct TabFiller
        : DENG2_OBSERVES(EscapeParser, PlainText)
        , DENG2_OBSERVES(EscapeParser, EscapeSequence)
{
    EscapeParser esc;
    QStringList lines;
    String current;
    bool hasTabs;

    TabFiller(String const &text) : hasTabs(false)
    {
        esc.audienceForPlainText() += this;
        esc.audienceForEscapeSequence() += this;

        // Break the entire message into lines, excluding all escape codes
        // except for tabs.
        esc.parse(text);

        if(!current.isEmpty()) lines << current;
    }

    void handlePlainText(Rangei const &range)
    {
        for(int i = range.start; i < range.end; ++i)
        {
            QChar ch = esc.originalText().at(i);
            if(ch == '\n')
            {
                lines << current;
                current.clear();
                current.reserve(80); // Prepare for per-character appending.
            }
            else
            {
                current.append(ch);
            }
        }
    }

    void handleEscapeSequence(Rangei const &range)
    {
        String seq = esc.originalText().substr(range);
        if(seq.at(0) == '\t')
        {
            current.append("\t+");
            hasTabs = true;
        }
        else if(seq.at(0) == 'T')
        {
            current.append('\t');
            current.append(seq.at(1));
            hasTabs = true;
        }
    }

    int highestTabStop() const
    {
        int maxStop = 0;
        foreach(QString const &ln, lines)
        {
            int stop = 0;
            for(int i = 0; i < ln.size(); ++i)
            {
                if(ln.at(i) == '\t')
                {
                    ++i;
                    if(ln.at(i) == '+')
                    {
                        stop++;
                    }
                    else if(ln.at(i) == '`')
                    {
                        stop = 0;
                    }
                    else
                    {
                        stop = ln.at(i).toLatin1() - 'a';
                    }
                    maxStop = max(stop, maxStop);
                }
            }
        }
        return maxStop;
    }

    /// Returns @c true if all tabs were filled.
    bool fillTabs(QStringList &fills, int maxStop, int minIndent) const
    {
        // The T` escape marks the place where tab stops are completely reset.
        Vector2i resetAt(-1, -1);

        for(int stop = 0; stop <= maxStop; ++stop)
        {
            int tabWidth = 0;

            // Find the widest position for this tab stop by checking all lines.
            for(int idx = 0; idx < fills.size(); ++idx)
            {
                String const &ln = fills.at(idx);
                int w = (idx > 0? minIndent : 0);
                for(int i = 0; i < ln.size(); ++i)
                {
                    if(ln.at(i) == '\t')
                    {
                        ++i;
                        if(ln.at(i) == '`')
                        {
                            // Any tabs following this will need re-evaluating;
                            // continue to the tab-replacing phase.
                            goto replaceTabs;
                        }
                        if(ln.at(i) == '+' || ln.at(i).toLatin1() - 'a' == stop)
                        {
                            // This is it.
                            tabWidth = max(tabWidth, w);
                        }
                        continue;
                    }
                    else
                    {
                        ++w;
                    }
                }
            }

replaceTabs:
            // Fill up (replace) the tabs with spaces according to the widest found
            // position.
            for(int idx = 0; idx < fills.size(); ++idx)
            {
                QString &ln = fills[idx];
                int w = (idx > 0? minIndent : 0);
                for(int i = 0; i < ln.size(); ++i)
                {
                    if(ln.at(i) == '\t')
                    {
                        ++i;
                        if(ln.at(i) == '`')
                        {
                            // This T` escape will be removed once we've checked
                            // all the tab stops preceding it.
                            resetAt = Vector2i(idx, i - 1);
                            goto nextStop;
                        }
                        if(ln.at(i) == '+' || ln.at(i).toLatin1() - 'a' == stop)
                        {
                            // Replace this stop with spaces.
                            ln.remove(--i, 2);
                            ln.insert(i, String(max(0, tabWidth - w), QChar(' ')));
                        }
                        continue;
                    }
                    else
                    {
                        ++w;
                    }
                }
            }
nextStop:;
        }

        // Now we can remove the possible T` escape.
        if(resetAt.x >= 0)
        {
            fills[resetAt.x].remove(resetAt.y, 2);
            return false;
        }

        // All tabs removed.
        return true;
    }

    /// Returns the text with tabs replaced with spaces.
    /// @param minIndent  Identation for lines past the first one.
    String filled(int minIndent) const
    {
        // The fast way out.
        if(!hasTabs) return esc.plainText();

        int const maxStop = highestTabStop();

        QStringList fills = lines;
        while(!fillTabs(fills, maxStop, minIndent)) {}

#if 0
#ifdef DENG2_DEBUG
        // No tabs should remain.
        foreach(QString ln, fills) DENG2_ASSERT(!ln.contains('\t'));
#endif
#endif

        return fills.join("\n");
    }
};

MonospaceLogSinkFormatter::MonospaceLogSinkFormatter()
    : _maxLength(89), _minimumIndent(0), _sectionDepthOfPreviousLine(0)
{
#ifdef DENG2_DEBUG
    // Debug builds include a timestamp and msg type indicator.
    _maxLength = 110;
    _minimumIndent = 21;
#endif
}

QList<String> MonospaceLogSinkFormatter::logEntryToTextLines(LogEntry const &entry)
{
    QList<String> resultLines;

    String const &section = entry.section();

    int cutSection = 0;

#ifndef DENG2_DEBUG
    // In a release build we can dispense with the metadata.
    LogEntry::Flags entryFlags = LogEntry::Simple;
#else
    LogEntry::Flags entryFlags;
#endif

    // Compare the current entry's section with the previous one
    // and if there is an opportunity to omit or abbreviate.
    if(!_sectionOfPreviousLine.isEmpty()
            && entry.sectionDepth() >= 1
            && _sectionDepthOfPreviousLine <= entry.sectionDepth())
    {
        if(_sectionOfPreviousLine == section)
        {
            // Previous section is exactly the same, omit completely.
            entryFlags |= LogEntry::SectionSameAsBefore;
        }
        else if(section.startsWith(_sectionOfPreviousLine))
        {
            // Previous section is partially the same, omit the common beginning.
            cutSection = _sectionOfPreviousLine.size();
            entryFlags |= LogEntry::SectionSameAsBefore;
        }
        else
        {
            int prefix = section.commonPrefixLength(_sectionOfPreviousLine);
            if(prefix > 5)
            {
                // Some commonality with previous section, we can abbreviate
                // those parts of the section.
                entryFlags |= LogEntry::AbbreviateSection;
                cutSection = prefix;
            }
        }
    }

    // Fill tabs with space.
    String message = TabFiller(entry.asText(entryFlags, cutSection)).filled(_minimumIndent);

    // Remember for the next line.
    _sectionOfPreviousLine      = section;
    _sectionDepthOfPreviousLine = entry.sectionDepth();

    // The wrap indentation will be determined dynamically based on the content
    // of the line.
    int wrapIndent = -1;
    int nextWrapIndent = -1;

    // Print line by line.
    String::size_type pos = 0;
    while(pos != String::npos)
    {
        // Find the length of the current line.
        String::size_type next = message.indexOf('\n', pos);
        duint lineLen = (next == String::npos? message.size() - pos : next - pos);
        duint const maxLen = (pos > 0? _maxLength - wrapIndent : _maxLength);
        if(lineLen > maxLen)
        {
            // Wrap overly long lines.
            next = pos + maxLen;
            lineLen = maxLen;

            // Maybe there's whitespace we can wrap at.
            int checkPos = pos + maxLen;
            while(checkPos > pos)
            {
                /// @todo remove isPunct() and just check for the breaking chars
                if(message[checkPos].isSpace() ||
                        (message[checkPos].isPunct() && message[checkPos] != '.' &&
                         message[checkPos] != ','    && message[checkPos] != '-' &&
                         message[checkPos] != '\''   && message[checkPos] != '"' &&
                         message[checkPos] != '('    && message[checkPos] != ')' &&
                         message[checkPos] != '['    && message[checkPos] != ']' &&
                         message[checkPos] != '_'))
                {
                    if(!message[checkPos].isSpace())
                    {
                        // Include the punctuation on this line.
                        checkPos++;
                    }

                    // Break here.
                    next = checkPos;
                    lineLen = checkPos - pos;
                    break;
                }
                checkPos--;
            }
        }

        // Crop this line's text out of the entire message.
        String lineText = message.substr(pos, lineLen);

        // For lines other than the first one, print an indentation.
        if(pos > 0)
        {
            lineText = QString(wrapIndent, QChar(' ')) + lineText;
        }

        // The wrap indent for this paragraph depends on the first line's content.
        if(nextWrapIndent < 0)
        {
            int w = _minimumIndent;
            int firstNonSpace = -1;
            for(; w < lineText.size(); ++w)
            {
                if(firstNonSpace < 0 && !lineText[w].isSpace())
                    firstNonSpace = w;

                // Indent to colons automatically (but not too deeply).
                if(lineText[w] == ':' && w < lineText.size() - 1 && lineText[w + 1].isSpace())
                    firstNonSpace = (w < int(_maxLength)*2/3? -1 : _minimumIndent);
            }

            nextWrapIndent = qMax(_minimumIndent, firstNonSpace);
        }

        // Check for formatting symbols.
        lineText.replace(DENG2_ESC("R"), String(maxLen - _minimumIndent, '-'));

        resultLines.append(lineText);

        // Advance to the next line.
        wrapIndent = nextWrapIndent;
        pos = next;
        if(pos != String::npos && message[pos].isSpace())
        {
            // At a forced newline, reset the wrap indentation.
            if(message[pos] == '\n')
            {
                nextWrapIndent = -1;
                wrapIndent = _minimumIndent;
            }
            pos++; // Skip whitespace.
        }
    }

    return resultLines;
}

void MonospaceLogSinkFormatter::setMaxLength(duint maxLength)
{
    _maxLength = de::max(duint(_minimumIndent + 10), maxLength);
}

duint MonospaceLogSinkFormatter::maxLength() const
{
    return _maxLength;
}

} // namespace de
