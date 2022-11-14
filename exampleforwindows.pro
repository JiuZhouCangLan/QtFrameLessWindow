#-------------------------------------------------
#
# Project created by QtCreator 2017-10-20T09:59:00
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = exampleforwindows
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp
HEADERS  += mainwindow.h\
            NativeWindowTemplate.hpp
FORMS    += mainwindow.ui

win32{
        SOURCES +=
}
LIBS += -ldwmapi
LIBS += -lUser32

CONFIG(debug, debug|release) {
message("debug mode")
}else {
message("release mode")
}
