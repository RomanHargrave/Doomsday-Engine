/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Info"
#include "de/ScriptLex"
#include "de/Log"
#include "de/LogBuffer"
#include "de/Zeroed"
#include "de/App"
#include <QFile>

namespace de {

static QString const WHITESPACE = " \t\r\n";
static QString const WHITESPACE_OR_COMMENT = " \t\r\n#";
static QString const TOKEN_BREAKING_CHARS = "#:=$(){}<>,\"" + WHITESPACE;
static QString const INCLUDE_TOKEN = "@include";

DENG2_PIMPL(Info)
{
    DENG2_ERROR(OutOfElements);
    DENG2_ERROR(EndOfFile);

    struct DefaultIncludeFinder : public IIncludeFinder
    {
        String findIncludedInfoSource(String const &includeName, Info const &info,
                                      String *sourcePath) const
        {
            String path = info.sourcePath().fileNamePath() / includeName;
            if(sourcePath) *sourcePath = path;
            return String::fromUtf8(Block(App::rootFolder().locate<File const>(path)));
        }
    };

    QStringList scriptBlockTypes;
    QStringList allowDuplicateBlocksOfType;
    String sourcePath; ///< May be unknown (empty).
    String content;
    int currentLine;
    int cursor; ///< Index of the next character from the source.
    QChar currentChar;
    int tokenStartOffset;
    String currentToken;
    BlockElement rootBlock;
    DefaultIncludeFinder defaultFinder;
    IIncludeFinder const *finder;

    typedef Info::Element::Value InfoValue;

    Instance(Public *i)
        : Base(i)
        , currentLine(0)
        , cursor(0)
        , tokenStartOffset(0)
        , rootBlock("", "", *i)
        , finder(&defaultFinder)
    {
        scriptBlockTypes << "script";
    }

    /**
     * Initialize the parser for reading a block of source content.
     * @param source  Text to be parsed.
     */
    void init(String const &source)
    {
        rootBlock.clear();

        // The source data. Add an extra newline so the character reader won't
        // get confused.
        content = source + "\n";
        currentLine = 1;

        currentChar = '\0';
        cursor = 0;
        nextChar();
        tokenStartOffset = 0;

        // When nextToken() is called and the current token is empty,
        // it is deduced that the source file has ended. We must
        // therefore set a dummy token that will be discarded
        // immediately.
        currentToken = " ";
        nextToken();
    }

    /**
     * Returns the next character from the source file.
     */
    QChar peekChar()
    {
        return currentChar;
    }

    /**
     * Move to the next character in the source file.
     */
    void nextChar()
    {
        if(cursor >= content.size())
        {
            // No more characters to read.
            throw EndOfFile(QString("EOF on line %1").arg(currentLine));
        }
        if(currentChar == '\n')
        {
            currentLine++;
        }
        currentChar = content[cursor];
        cursor++;
    }

    /**
     * Read a line of text from the content and return it.
     */
    String readLine()
    {
        String line;
        nextChar();
        while(currentChar != '\n')
        {
            line += currentChar;
            nextChar();
        }
        return line;
    }

    /**
     * Read until a newline is encountered. Returns the contents of the line.
     */
    String readToEOL()
    {
        cursor = tokenStartOffset;
        String line = readLine();
        try
        {
            nextChar();
        }
        catch(EndOfFile const &)
        {
            // If the file ends right after the line, we'll get the EOF
            // exception.  We can safely ignore it for now.
        }
        return line;
    }

    String peekToken()
    {
        return currentToken;
    }

    /**
     * Returns the next meaningful token from the source file.
     */
    String nextToken()
    {
        // Already drawn a blank?
        if(currentToken.isEmpty()) throw EndOfFile("out of tokens");

        currentToken = "";

        try
        {
            // Skip over any whitespace.
            while(WHITESPACE_OR_COMMENT.contains(peekChar()))
            {
                // Comments are considered whitespace.
                if(peekChar() == '#') readLine();
                nextChar();
            }

            // Store the offset where the token begins.
            tokenStartOffset = cursor;

            // The first nonwhite is accepted.
            currentToken += peekChar();
            nextChar();

            // Token breakers are tokens all by themselves.
            if(TOKEN_BREAKING_CHARS.contains(currentToken[0]))
                return currentToken;

            while(!TOKEN_BREAKING_CHARS.contains(peekChar()))
            {
                currentToken += peekChar();
                nextChar();
            }
        }
        catch(EndOfFile const &)
        {}

        return currentToken;
    }

    /**
     * This is the method that the user calls to retrieve the next element from
     * the source file. If there are no more elements to return, a
     * OutOfElements exception is thrown.
     *
     * @return Parsed element. Caller gets owernship.
     */
    Element *get()
    {
        Element *e = parseElement();
        if(!e) throw OutOfElements("");
        return e;
    }

    /**
     * Returns the next element from the source file.
     * @return An instance of one of the Element classes, or @c NULL if there are none.
     */
    Element *parseElement()
    {
        String key;
        String next;
        try
        {
            key = peekToken();

            // The next token decides what kind of element we have here.
            next = nextToken();
        }
        catch(EndOfFile const &)
        {
            // The file ended.
            return 0;
        }

        int const elementLine = currentLine;
        Element *result = 0;

        if(next == ":" || next == "=" || next == "$")
        {
            result = parseKeyElement(key);
        }
        else if(next == "<")
        {
            result = parseListElement(key);
        }
        else
        {
            // It must be a block element.
            result = parseBlockElement(key);
        }

        DENG2_ASSERT(result != 0);

        result->setSourceLocation(sourcePath, elementLine);
        return result;
    }

    /**
     * Parse a string literal. Returns the string sans the quotation marks in
     * the beginning and the end.
     */
    String parseString()
    {
        if(peekToken() != "\"")
        {
            throw SyntaxError("Info::parseString",
                              QString("Expected string to begin with '\"', but '%1' found instead (on line %2).")
                              .arg(peekToken()).arg(currentLine));
        }

        // The collected characters.
        String chars;

        while(peekChar() != '"')
        {
            if(peekChar() == '\'')
            {
                // Double single quotes form a double quote ('' => ").
                nextChar();
                if(peekChar() == '\'')
                {
                    chars.append("\"");
                }
                else
                {
                    chars.append("'");
                    continue;
                }
            }
            else
            {
                // Other characters are appended as-is, even newlines.
                chars.append(peekChar());
            }
            nextChar();
        }

        // Move the parser to the next token.
        nextChar();
        nextToken();
        return chars;
    }

    /**
     * Parse a value from the source file. The current token should be on the
     * first token of the value. Values come in different flavours:
     * - single token
     * - string literal (can be split)
     */
    InfoValue parseValue()
    {
        InfoValue value;

        if(peekToken() == "$")
        {
            // Marks a script value.
            value.flags |= InfoValue::Script;
            nextToken();
        }

        // Check if it is the beginning of a string literal.
        if(peekToken() == "\"")
        {
            try
            {
               // The value will be composed of any number of sub-strings.
               forever { value.text += parseString(); }
            }
            catch(de::Error const &)
            {
                // No more strings to append.
                return value;
            }
        }        

        // Then it must be a single token.
        value = peekToken();
        nextToken();
        return value;
    }

    InfoValue parseScript(int numStatements = 0)
    {
        int startPos = cursor - 1;
        String remainder = content.substr(startPos);
        ScriptLex lex(remainder);
        try
        {
            TokenBuffer tokens;
            int count = 0;

            // Read an appropriate number of statements.
            while(lex.getStatement(tokens))
            {
                if(numStatements > 0 && ++count == numStatements)
                    goto success;
            }
            throw SyntaxError("Info::parseScript",
                              QString("Unexpected end of script starting at line %1").arg(currentLine));
success:;
        }
        catch(ScriptLex::MismatchedBracketError const &)
        {
            // A mismatched bracket signals the end of the script block.
        }

        // Continue parsing normally from here.
        int endPos = startPos + lex.pos();
        do { nextChar(); } while(cursor < endPos); // fast-forward

        // Update the current token.
        currentToken = QString(peekChar());
        nextChar();

        if(currentToken != ")" && currentToken != "}")
        {
            // When parsing just a statement, we might stop at something else
            // than a bracket; if so, skip to the next valid token.
            nextToken();
        }

        //qDebug() << "now at" << content.substr(endPos - 15, endPos) << "^" << content.substr(endPos);

        return InfoValue(content.substr(startPos, lex.pos() - 1), InfoValue::Script);
    }

    /**
     * Parse a key element.
     * @param name Name of the parsed key element.
     */
    KeyElement *parseKeyElement(String const &name)
    {
        InfoValue value;

        if(peekToken() == "$")
        {
            // This is a script value.
            value.flags |= InfoValue::Script;
            nextToken();
        }

        // A colon means that that the rest of the line is the value of
        // the key element.
        if(peekToken() == ":")
        {
            value.text = readToEOL().trimmed();
            nextToken();
        }
        else if(peekToken() == "=")
        {
            if(value.flags.testFlag(InfoValue::Script))
            {
                // Parse one script statement.
                value = parseScript(1);
                value.text = value.text.trimmed();
            }
            else
            {
                /**
                 * Key =
                 *   "This is a long string "
                 *   "that spans multiple lines."
                 */
                nextToken();
                value.text = parseValue();
            }
        }
        else
        {
            throw SyntaxError("Info::parseKeyElement",
                              QString("Expected either '=' or ':', but '%1' found instead (on line %2).")
                              .arg(peekToken()).arg(currentLine));
        }
        return new KeyElement(name, value);
    }

    /**
     * Parse a list element, identified by the given name.
     */
    ListElement *parseListElement(String const &name)
    {
        if(peekToken() != "<")
        {
            throw SyntaxError("Info::parseListElement",
                              QString("List must begin with a '<', but '%1' found instead (on line %2).")
                              .arg(peekToken()).arg(currentLine));
        }

        QScopedPointer<ListElement> element(new ListElement(name));

        /// List syntax:
        /// list ::= list-identifier '<' [value {',' value}] '>'
        /// list-identifier ::= token

        // Move past the opening angle bracket.
        nextToken();

        forever
        {
            element->add(parseValue());

            // List elements are separated explicitly.
            String separator = peekToken();
            nextToken();

            // The closing bracket?
            if(separator == ">") break;

            // There should be a comma here.
            if(separator != ",")
            {
                throw SyntaxError("Info::parseListElement",
                                  QString("List values must be separated with a comma, but '%1' found instead (on line %2).")
                                  .arg(separator).arg(currentLine));
            }
        }
        return element.take();
    }

    /**
     * Parse a block element, identified by the given name.
     * @param blockType Identifier of the block.
     */
    BlockElement *parseBlockElement(String const &blockType)
    {
        DENG2_ASSERT(blockType != "}");
        DENG2_ASSERT(blockType != ")");

        String blockName;
        if(peekToken() != "(" && peekToken() != "{")
        {
            blockName = parseValue();
        }

        QScopedPointer<BlockElement> block(new BlockElement(blockType, blockName, self));
        int startLine = currentLine;

        String endToken;

        try
        {
            // How about some attributes?
            // Syntax: {token value} '('|'{'

            while(peekToken() != "(" && peekToken() != "{")
            {
                String keyName = peekToken();
                nextToken();
                InfoValue value = parseValue();

                // This becomes a key element inside the block but it's
                // flagged as Attribute.
                block->add(new KeyElement(keyName, value, KeyElement::Attribute));
            }

            endToken = (peekToken() == "("? ")" : "}");

            // Parse the contents of the block.
            if(scriptBlockTypes.contains(blockType))
            {
                // Parse as Doomsday Script.
                block->add(new KeyElement("script", parseScript()));
            }
            else
            {
                // Move past the opening parentheses.
                nextToken();

                // Parse normally as Info.
                while(peekToken() != endToken)
                {
                    Element *element = parseElement();
                    if(!element)
                    {
                        throw SyntaxError("Info::parseBlockElement",
                                          QString("Block element was never closed, end of file encountered before '%1' was found (on line %2).")
                                          .arg(endToken).arg(currentLine));
                    }
                    block->add(element);
                }
            }
        }
        catch(EndOfFile const &)
        {
            throw SyntaxError("Info::parseBlockElement",
                              QString("End of file encountered unexpectedly while parsing a block element (block started on line %1).")
                              .arg(startLine));
        }

        DENG2_ASSERT(peekToken() == endToken);

        // Move past the closing parentheses.
        nextToken();

        return block.take();
    }

    void includeFrom(String const &includeName)
    {
        try
        {
            DENG2_ASSERT(finder != 0);

            String includePath;
            String content = finder->findIncludedInfoSource(includeName, self, &includePath);

            Info included;
            included.setFinder(*finder); // use ours
            included.setSourcePath(includePath);
            included.parse(content);

            // Move the contents of the resulting root block to our root block.
            included.d->rootBlock.moveContents(rootBlock);
        }
        catch(Error const &er)
        {
            throw IIncludeFinder::NotFoundError("Info::includeFrom",
                    QString("Cannot include '%1': %2")
                    .arg(includeName)
                    .arg(er.asText()));
        }
    }

    void parse(String const &source)
    {
        init(source);
        forever
        {
            Element *e = parseElement();
            if(!e) break;

            // If this is an include directive, try to acquire the inclusion and parse it
            // instead. Inclusions are only possible at the root level.
            if(e->isList() && e->name() == INCLUDE_TOKEN)
            {
                foreach(Element::Value const &val, e->as<ListElement>().values())
                {
                    includeFrom(val);
                }
            }

            rootBlock.add(e);
        }
    }

    void parse(File const &file)
    {
        sourcePath = file.path();
        parse(String::fromUtf8(Block(file)));
    }
};

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(Info::Element)
{
    Type type;
    String name;
    Zeroed<BlockElement *> parent;
    String sourcePath;
    Zeroed<int> lineNumber;
};

Info::Element::Element(Type type, String const &name)
    : d(new Instance)
{
    d->type = type;
    setName(name);
}

Info::Element::~Element()
{}

void Info::Element::setParent(BlockElement *parent)
{
    d->parent = parent;
}

Info::BlockElement *Info::Element::parent() const
{
    return d->parent;
}

void Info::Element::setSourceLocation(String const &sourcePath, int line)
{
    d->sourcePath = sourcePath;
    d->lineNumber = line;
}

String Info::Element::sourcePath() const
{
    return d->sourcePath;
}

int Info::Element::lineNumber() const
{
    return d->lineNumber;
}

String Info::Element::sourceLocation() const
{
    return String("%1:%2").arg(d->sourcePath).arg(d->lineNumber);
}

Info::Element::Type Info::Element::type() const
{
    return d->type;
}

String const &Info::Element::name() const
{
    return d->name;
}

void Info::Element::setName(String const &name)
{
    d->name = name;
}

Info::BlockElement::~BlockElement()
{
    clear();
}

void Info::BlockElement::clear()
{
    for(ContentsInOrder::iterator i = _contentsInOrder.begin(); i != _contentsInOrder.end(); ++i)
    {
        delete *i;
    }
    _contents.clear();
    _contentsInOrder.clear();
}

void Info::BlockElement::add(Info::Element *elem)
{
    DENG2_ASSERT(elem != 0);

    elem->setParent(this);
    _contentsInOrder.append(elem); // owned
    if(!elem->name().isEmpty())
    {
        _contents.insert(elem->name().toLower(), elem); // not owned (name may be empty)
    }
}

Info::Element *Info::BlockElement::find(String const &name) const
{
    Contents::const_iterator found = _contents.find(name.toLower());
    if(found == _contents.end()) return 0;
    return found.value();
}

Info::Element::Value Info::BlockElement::keyValue(String const &name) const
{
    Element *e = find(name);
    if(!e || !e->isKey()) return Value();
    return e->as<KeyElement>().value();
}

Info::Element *Info::BlockElement::findByPath(String const &path) const
{
    String name;
    String remainder;
    int pos = path.indexOf(':');
    if(pos >= 0)
    {
        name = path.left(pos);
        remainder = path.mid(pos + 1);
    }
    else
    {
        name = path;
    }
    name = name.trimmed();

    // Does this element exist?
    Element *e = find(name);
    if(!e) return 0;

    if(e->isBlock())
    {
        // Descend into sub-blocks.
        return e->as<BlockElement>().findByPath(remainder);
    }
    return e;
}

void Info::BlockElement::moveContents(BlockElement &destination)
{
    foreach(Element *e, _contentsInOrder)
    {
        destination.add(e);
    }
    _contentsInOrder.clear();
    _contents.clear();
}

//---------------------------------------------------------------------------------------

Info::Info() : d(new Instance(this))
{}

Info::Info(String const &source)
{
    QScopedPointer<Instance> inst(new Instance(this)); // parsing may throw exception
    inst->parse(source);
    d.reset(inst.take());
}

Info::Info(File const &file)
{
    QScopedPointer<Instance> inst(new Instance(this)); // parsing may throw exception
    inst->parse(file);
    d.reset(inst.take());
}

Info::Info(String const &source, IIncludeFinder const &finder)
{
    QScopedPointer<Instance> inst(new Instance(this)); // parsing may throw exception
    inst->finder = &finder;
    inst->parse(source);
    d.reset(inst.take());
}

void Info::setFinder(IIncludeFinder const &finder)
{
    d->finder = &finder;
}

void Info::useDefaultFinder()
{
    d->finder = &d->defaultFinder;
}

void Info::setScriptBlocks(QStringList const &blocksToParseAsScript)
{
    d->scriptBlockTypes = blocksToParseAsScript;
}

void Info::setAllowDuplicateBlocksOfType(QStringList const &duplicatesAllowed)
{
    d->allowDuplicateBlocksOfType = duplicatesAllowed;
}

void Info::parse(String const &infoSource)
{
    d->parse(infoSource);
}

void Info::parse(File const &file)
{
    d->parse(file);
}

void Info::parseNativeFile(NativePath const &nativePath)
{
    QFile file(nativePath);
    if(file.open(QFile::ReadOnly | QFile::Text))
    {
        parse(file.readAll().constData());
    }
}

void Info::clear()
{
    d->sourcePath.clear();
    parse("");
}

void Info::setSourcePath(String const &path)
{
    d->sourcePath = path;
}

String Info::sourcePath() const
{
    return d->sourcePath;
}

Info::BlockElement const &Info::root() const
{
    return d->rootBlock;
}

Info::Element const *Info::findByPath(String const &path) const
{
    if(path.isEmpty()) return &d->rootBlock;
    return d->rootBlock.findByPath(path);
}

bool Info::findValueForKey(String const &key, String &value) const
{
    Element const *element = findByPath(key);
    if(element && element->isKey())
    {
        value = element->as<KeyElement>().value();
        return true;
    }
    return false;
}

} // namespace de
