QT += testlib widgets
CONFIG += c++1z console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tst_weightingcurve

INCLUDEPATH += .. ../src

SOURCES += \
    tst_weightingcurve.cpp \
    ../src/common/settings.cpp \
    ../src/common/weightingcurvestore.cpp \
    ../src/common/workingfolder.cpp \
    ../src/math/weightingcurve.cpp

HEADERS += \
    ../src/common/settings.h \
    ../src/common/weightingcurvestore.h \
    ../src/common/workingfolder.h \
    ../src/math/weightingcurve.h
