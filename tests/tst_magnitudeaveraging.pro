QT += testlib
CONFIG += c++1z console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tst_magnitudeaveraging

INCLUDEPATH += .. ../src
SOURCES += \
    tst_magnitudeaveraging.cpp \
    ../src/math/averaging.cpp \
    ../src/math/complex.cpp
HEADERS += \
    ../src/math/averaging.h \
    ../src/math/complex.h
