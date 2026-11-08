QT += testlib
CONFIG += c++1z console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tst_ltwwindows

INCLUDEPATH += .. ../src

SOURCES += \
    tst_ltwwindows.cpp \
    ../src/math/complex.cpp \
    ../src/math/fouriertransform.cpp \
    ../src/math/windowfunction.cpp

HEADERS += \
    ../src/math/fouriertransform.h \
    ../src/math/windowfunction.h
