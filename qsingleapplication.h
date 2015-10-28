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

#ifndef QSINGLEAPPLICATION_H
#define QSINGLEAPPLICATION_H

#include <QGuiApplication>
#include <QSharedMemory>

/**
 * @brief QGuiApplication that tracks its instances to allow only a single instance.
 */
class QSingleApplication : public QGuiApplication
{
Q_OBJECT
public:
    /**
     * @brief A constructor
     * @param argc argument count
     * @param argv command line arguments
     * @param key unique key used in the application that is used to track instances.
     *            To make the application single instance application, always create
     *            the class with the same key. To determine whether the application
     *            already exists, use exists().
     */
    QSingleApplication(int& argc, char** argv, QString key);
    /**
     * @brief A destructor.
     */
    virtual ~QSingleApplication();

    /**
     * @brief Check whether this is a first instance of QSingleApplication.
     * @return true if the object is a first created instance of a QSingleApplication
     *         with the specified key.
     */
    bool isFirstInstance();
private:
    bool m_firstInstance;
};

#endif // QSINGLEAPPLICATION_H
