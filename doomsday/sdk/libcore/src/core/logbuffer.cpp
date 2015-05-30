/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/LogBuffer"
#include "de/LogSink"
#include "de/SimpleLogFilter"
#include "de/FileLogSink"
#include "de/DebugLogSink"
#include "de/TextStreamLogSink"
#include "de/Writer"
#include "de/FixedByteArray"
#include "de/Guard"
#include "de/App"

#include <stdio.h>
#include <QTextStream>
#include <QCoreApplication>
#include <QList>
#include <QSet>
#include <QTimer>
#include <QDebug>

namespace de {

TimeDelta const FLUSH_INTERVAL = .2; // seconds

DENG2_PIMPL_NOREF(LogBuffer)
{
    typedef QList<LogEntry *> EntryList;
    typedef QSet<LogSink *> Sinks;

    SimpleLogFilter defaultFilter;
    IFilter const *entryFilter;
    dint maxEntryCount;
    bool useStandardOutput;
    bool flushingEnabled;
    File *outputFile;
    FileLogSink *fileLogSink;
#ifndef WIN32
    TextStreamLogSink outSink;
    TextStreamLogSink errSink;
#else
    DebugLogSink outSink;
    DebugLogSink errSink;
#endif
    EntryList entries;
    EntryList toBeFlushed;
    Time lastFlushedAt;
    QTimer *autoFlushTimer;
    Sinks sinks;

    Instance(duint maxEntryCount)
        : entryFilter(&defaultFilter)
        , maxEntryCount(maxEntryCount)
        , useStandardOutput(true)
        , flushingEnabled(true)
        , outputFile(0)
        , fileLogSink(0)
#ifndef WIN32
        , outSink(new QTextStream(stdout))
        , errSink(new QTextStream(stderr))
#else
          // Windows GUI apps don't have stdout/stderr.
        , outSink(QtDebugMsg)
        , errSink(QtWarningMsg)
#endif
        , lastFlushedAt(Time::invalidTime())
        , autoFlushTimer(0)
    {
        // Standard output enabled by default.
        outSink.setMode(LogSink::OnlyNormalEntries);
        errSink.setMode(LogSink::OnlyWarningEntries);

        sinks.insert(&outSink);
        sinks.insert(&errSink);
    }

    ~Instance()
    {
        if(autoFlushTimer) autoFlushTimer->stop();
        delete fileLogSink;
    }

    void enableAutoFlush(bool yes)
    {
        DENG2_ASSERT(qApp);
        if(yes)
        {
            if(!autoFlushTimer->isActive())
            {
                // Every now and then the buffer will be flushed.
                autoFlushTimer->start(int(FLUSH_INTERVAL.asMilliSeconds()));
            }
        }
        else
        {
            autoFlushTimer->stop();
        }
    }

    void disposeFileLogSink()
    {
        if(fileLogSink)
        {
            sinks.remove(fileLogSink);
            delete fileLogSink;
            fileLogSink = 0;
        }
    }
};

LogBuffer *LogBuffer::_appBuffer = 0;

LogBuffer::LogBuffer(duint maxEntryCount) 
    : d(new Instance(maxEntryCount))
{
    d->autoFlushTimer = new QTimer(this);
    connect(d->autoFlushTimer, SIGNAL(timeout()), this, SLOT(flush()));
}

LogBuffer::~LogBuffer()
{
    DENG2_GUARD(this);

    setOutputFile("");
    clear();

    if(_appBuffer == this) _appBuffer = 0;
}

void LogBuffer::clear()
{
    DENG2_GUARD(this);

    // Flush first, we don't want to miss any messages.
    flush();

    DENG2_FOR_EACH(Instance::EntryList, i, d->entries)
    {
        delete *i;
    }
    d->entries.clear();
}

dsize LogBuffer::size() const
{
    DENG2_GUARD(this);
    return d->entries.size();
}

void LogBuffer::latestEntries(Entries &entries, int count) const
{
    DENG2_GUARD(this);
    entries.clear();
    for(int i = d->entries.size() - 1; i >= 0; --i)
    {
        entries.append(d->entries[i]);
        if(count && entries.size() >= count)
        {
            return;
        }
    }
}

void LogBuffer::setEntryFilter(IFilter const *entryFilter)
{
    if(entryFilter)
    {
        d->entryFilter = entryFilter;
    }
    else
    {
        d->entryFilter = &d->defaultFilter;
    }
}

bool LogBuffer::isEnabled(duint32 entryMetadata) const
{
    DENG2_ASSERT(d->entryFilter != 0);
    DENG2_ASSERT(entryMetadata & LogEntry::DomainMask); // must have a domain
    return d->entryFilter->isLogEntryAllowed(entryMetadata);
}

void LogBuffer::setMaxEntryCount(duint maxEntryCount)
{
    d->maxEntryCount = maxEntryCount;
}

void LogBuffer::add(LogEntry *entry)
{       
    DENG2_GUARD(this);

    // We will not flush the new entry as it likely has not yet been given
    // all its arguments.
    if(d->lastFlushedAt.isValid() && d->lastFlushedAt.since() > FLUSH_INTERVAL)
    {
        flush();
    }

    d->entries.push_back(entry);
    d->toBeFlushed.push_back(entry);
}

void LogBuffer::enableStandardOutput(bool yes)
{
    DENG2_GUARD(this);

    d->useStandardOutput = yes;

    d->outSink.setMode(yes? LogSink::OnlyNormalEntries  : LogSink::Disabled);
    d->errSink.setMode(yes? LogSink::OnlyWarningEntries : LogSink::Disabled);
}

void LogBuffer::enableFlushing(bool yes)
{
    d->flushingEnabled = yes;
    d->enableAutoFlush(true);
}

void LogBuffer::setAutoFlushInterval(TimeDelta const &interval)
{
    enableFlushing();

    d->autoFlushTimer->setInterval(interval.asMilliSeconds());
}

void LogBuffer::setOutputFile(String const &path, OutputChangeBehavior behavior)
{
    DENG2_GUARD(this);

    if(behavior == FlushFirstToOldOutputs)
    {
        flush();
    }

    d->disposeFileLogSink();

    if(d->outputFile)
    {
        d->outputFile->audienceForDeletion() -= this;
        d->outputFile = 0;
    }

    if(!path.isEmpty())
    {
        d->outputFile = &App::rootFolder().replaceFile(path);
        d->outputFile->audienceForDeletion() += this;

        // Add a sink for the file.
        d->fileLogSink = new FileLogSink(*d->outputFile);
        d->sinks.insert(d->fileLogSink);
    }
}

String LogBuffer::outputFile() const
{
    if(!d->outputFile) return "";
    return d->outputFile->path();
}

void LogBuffer::addSink(LogSink &sink)
{
    DENG2_GUARD(this);

    d->sinks.insert(&sink);
}

void LogBuffer::removeSink(LogSink &sink)
{
    DENG2_GUARD(this);

    d->sinks.remove(&sink);
}

void LogBuffer::flush()
{
    if(!d->flushingEnabled) return;

    DENG2_GUARD(this);

    if(!d->toBeFlushed.isEmpty())
    {
        DENG2_FOR_EACH(Instance::EntryList, i, d->toBeFlushed)
        {
            DENG2_GUARD_FOR(**i, guardingCurrentLogEntry);
            foreach(LogSink *sink, d->sinks)
            {
                if(sink->willAccept(**i))
                {
                    try
                    {
                        *sink << **i;
                    }
                    catch(Error const &error)
                    {
                        *sink << String("Exception during log flush:\n") +
                                        error.what() + "\n(the entry format is: '" +
                                        (*i)->format() + "')";
                    }
                }
            }
        }

        d->toBeFlushed.clear();

        // Make sure everything really gets written now.
        foreach(LogSink *sink, d->sinks) sink->flush();
    }

    d->lastFlushedAt = Time();

    // Too many entries? Now they can be destroyed since we have flushed everything.
    while(d->entries.size() > d->maxEntryCount)
    {
        LogEntry *old = d->entries.front();
        d->entries.pop_front();
        delete old;
    }
}

void LogBuffer::fileBeingDeleted(File const &file)
{
    DENG2_ASSERT(d->outputFile == &file);
    DENG2_UNUSED(file);

    flush();
    d->disposeFileLogSink();
    d->outputFile = 0;   
}

void LogBuffer::setAppBuffer(LogBuffer &appBuffer)
{
    _appBuffer = &appBuffer;
}

LogBuffer &LogBuffer::get()
{
    DENG2_ASSERT(_appBuffer != 0);
    return *_appBuffer;
}

bool LogBuffer::appBufferExists()
{
    return _appBuffer != 0;
}

} // namespace de
