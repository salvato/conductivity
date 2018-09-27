#-------------------------------------------------
#
# Project created by QtCreator 2017-11-07T13:22:49
#
#-------------------------------------------------

#Copyright (C) 2016  Gabriele Salvato

#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>.


QT += core
QT += gui
QT += widgets
QT += charts


TARGET = conductivity
TEMPLATE = app


SOURCES += main.cpp
SOURCES += mainwindow.cpp
SOURCES += cornerstone130.cpp
SOURCES += keithley236.cpp
SOURCES += lakeshore330.cpp
SOURCES +=
SOURCES +=
SOURCES +=
SOURCES +=
SOURCES +=
SOURCES += DataSetProperties.cpp
SOURCES += configureIvsVdialog.cpp
SOURCES += configureRvsTdialog.cpp
SOURCES += utility.cpp

HEADERS += mainwindow.h
HEADERS += cornerstone130.h
HEADERS += keithley236.h
HEADERS += lakeshore330.h
HEADERS +=
HEADERS +=
HEADERS +=
HEADERS +=
HEADERS +=
HEADERS += DataSetProperties.h
HEADERS += configureIvsVdialog.h
HEADERS += configureRvsTdialog.h
HEADERS += utility.h

FORMS   += mainwindow.ui
FORMS   += configureIvsVdialog.ui
FORMS   += configureRvsTdialog.ui
FORMS   +=

# For National Instruments GPIB Boards
windows {
  message("Running on Windows")
  INCLUDEPATH += "C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/include"
  LIBS += "C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/lib32/msvc/gpib-32.obj"
}
linux {
  message("Running on Linux")
  LIBS += -L"/usr/local/lib" -lgpib # To include libgpib.so from /usr/local/lib
}


DISTFILES += doc/linux_Gpib_HowTo.txt
DISTFILES += doc/GPIBProgrammingReferenceManual.pdf
DISTFILES += doc/Oriel-Cornerstone-130-User-Manual-RevA.pdf
DISTFILES += doc/oriel_cornerstone_manual.pdf
DISTFILES += doc/Tracq32Manual.doc
DISTFILES += doc/Keithley236Manual.pdf
DISTFILES += doc/LakeShore330_Manual.pdf
DISTFILES += doc/Readme.txt

RESOURCES += \
    resources.qrc
