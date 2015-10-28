/*
 * A single-instance GUI application
 * Copyright (C) 2015 Lukáš Jirkovský <l.jirkovsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <signal.h>
#include <unistd.h>
#include "qsingleapplication.h"

QSharedMemory* sharedMem = 0;

static void removeInstance()
{
    if (sharedMem != 0 && sharedMem->isAttached()) {
        sharedMem->detach();
        sharedMem = 0;
    }
}

static void signalHandler(int sig) {
    removeInstance();
    // reinstall the default handlers
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    // signal again
    kill(getpid(), sig);
}

QSingleApplication::QSingleApplication(int& argc, char** argv, QString key):
    QGuiApplication(argc, argv)
{
    if (sharedMem == 0) {
        sharedMem = new QSharedMemory(key, this);
    }
    if (sharedMem->create(1, QSharedMemory::ReadWrite)) {
        // this migh not be necessary
        sharedMem->lock();
        *static_cast<char*>(sharedMem->data()) = 1;
        sharedMem->unlock();

        m_firstInstance = true;
    } else {
        sharedMem->attach(QSharedMemory::ReadOnly);
        m_firstInstance = false;
    }

    // install signal handlers to remove instance before quitting
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGQUIT, signalHandler);
    // install atexit function
    atexit(removeInstance);
}

QSingleApplication::~QSingleApplication()
{
    removeInstance();
}

bool QSingleApplication::isFirstInstance()
{
    return m_firstInstance;
}
