/**
 * @file concurrency.cpp
 * Concurrency: threads, mutexes, semaphores. @ingroup system
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de/concurrency.h"
#include <QMutex>
#include <QCoreApplication>
#include <QDebug>
#include <de/Time>
#include <de/Log>
#include <de/Garbage>
#include <assert.h>

static uint mainThreadId = 0; ///< ID of the main thread.

CallbackThread::CallbackThread(systhreadfunc_t func, void *param)
    : _callback(func), _parm(param), _returnValue(0),
      _exitStatus(DENG_THREAD_STOPPED_NORMALLY),
      _terminationFunc(0)
{
    //qDebug() << "CallbackThread:" << this << "created.";

    // Only used if the thread needs to be shut down forcibly.
    setTerminationEnabled(true);

    // Cleanup at app exit time for threads whose exit value hasn't been checked.
    connect(qApp, SIGNAL(destroyed()), this, SLOT(deleteNow()));
}

CallbackThread::~CallbackThread()
{
    if(isRunning())
    {
        //qDebug() << "CallbackThread:" << this << "forcibly stopping, deleting.";
        terminate();
        wait(1000);
    }
    else
    {
        //qDebug() << "CallbackThread:" << this << "deleted.";
    }
}

void CallbackThread::deleteNow()
{
    delete this;
}

void CallbackThread::run()
{
    _exitStatus = DENG_THREAD_STOPPED_WITH_FORCE;

    try
    {
        if(_callback)
        {
            _returnValue = _callback(_parm);
        }
        _exitStatus = DENG_THREAD_STOPPED_NORMALLY;
    }
    catch(std::exception const &error)
    {
        LOG_AS("CallbackThread");
        LOG_ERROR(QString("Uncaught exception: ") + error.what());
        _returnValue = -1;
        _exitStatus = DENG_THREAD_STOPPED_WITH_EXCEPTION;
    }

    if(_terminationFunc)
    {
        _terminationFunc(_exitStatus);
    }

    Garbage_ClearForThread();

    // No more log output from this thread.
    de::Log::disposeThreadLog();
}

int CallbackThread::exitValue() const
{
    return _returnValue;
}

systhreadexitstatus_t CallbackThread::exitStatus() const
{
    return _exitStatus;
}

void CallbackThread::setTerminationFunc(void (*func)(systhreadexitstatus_t))
{
    _terminationFunc = func;
}

void Sys_MarkAsMainThread(void)
{
    // This is the main thread.
    mainThreadId = Sys_CurrentThreadId();
}

dd_bool Sys_InMainThread(void)
{
    return mainThreadId == Sys_CurrentThreadId();
}

void Thread_Sleep(int milliseconds)
{
    de::TimeDelta::fromMilliSeconds(milliseconds).sleep();
}

thread_t Sys_StartThread(systhreadfunc_t startpos, void *parm)
{
    CallbackThread *t = new CallbackThread(startpos, parm);
    t->start();
    return t;
}

void Thread_KillAbnormally(thread_t handle)
{
    QThread *t = reinterpret_cast<QThread *>(handle);
    if(!handle)
    {
        t = QThread::currentThread();
    }
    assert(t);
    t->terminate();
}

void Thread_SetCallback(thread_t thread, void (*terminationFunc)(systhreadexitstatus_t))
{
    CallbackThread *t = reinterpret_cast<CallbackThread *>(thread);
    DENG_ASSERT(t);
    if(!t) return;

    t->setTerminationFunc(terminationFunc);
}

int Sys_WaitThread(thread_t handle, int timeoutMs, systhreadexitstatus_t *exitStatus)
{
    CallbackThread *t = reinterpret_cast<CallbackThread *>(handle);
    assert(static_cast<QThread *>(t) != QThread::currentThread());
    t->wait(timeoutMs);
    if(!t->isFinished())
    {
        LOG_WARNING("Thread did not stop in time, forcibly killing it.");
        if(exitStatus) *exitStatus = DENG_THREAD_STOPPED_WITH_FORCE;
    }
    else
    {
        if(exitStatus) *exitStatus = t->exitStatus();
    }
    t->deleteLater(); // get rid of it
    return t->exitValue();
}

uint32_t Sys_ThreadId(thread_t handle)
{
    QThread *t = reinterpret_cast<QThread *>(handle);
    if(!t) t = QThread::currentThread();
    return uint32_t(PTR2INT(t));
}

uint32_t Sys_CurrentThreadId(void)
{
    return Sys_ThreadId(NULL /*this thread*/);
}

/// @todo remove the name parameter
mutex_t Sys_CreateMutex(const char *)
{
    return new QMutex(QMutex::Recursive);
}

void Sys_DestroyMutex(mutex_t handle)
{
    if(handle)
    {
        delete reinterpret_cast<QMutex *>(handle);
    }
}

void Sys_Lock(mutex_t handle)
{
    QMutex *m = reinterpret_cast<QMutex *>(handle);
    assert(m != 0);
    if(m)
    {
        m->lock();
    }
}

void Sys_Unlock(mutex_t handle)
{
    QMutex *m = reinterpret_cast<QMutex *>(handle);
    assert(m != 0);
    if(m)
    {
        m->unlock();
    }
}
