QT += testlib
CONFIG += c++1z console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tst_longimpulse

INCLUDEPATH += .. ../src

SOURCES += \
    tst_longimpulse.cpp \
    ../src/math/complex.cpp \
    ../src/math/fouriertransform.cpp \
    ../src/math/windowfunction.cpp \
    ../src/math/deconvolution.cpp \
    ../src/math/speechintelligibility.cpp

HEADERS += \
    ../src/math/fouriertransform.h \
    ../src/math/windowfunction.h \
    ../src/math/deconvolution.h \
    ../src/math/speechintelligibility.h
