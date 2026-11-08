QT += testlib
CONFIG += c++1z console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tst_speechintelligibility

INCLUDEPATH += .. ../src

SOURCES += \
    tst_speechintelligibility.cpp \
    ../src/math/speechintelligibility.cpp

HEADERS += \
    ../src/math/speechintelligibility.h
