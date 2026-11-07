QT += testlib qml widgets
CONFIG += c++1z console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tst_coherenceexponent

INCLUDEPATH += .. ../src

SOURCES += \
    tst_coherenceexponent.cpp \
    ../src/common/settings.cpp \
    ../src/common/workingfolder.cpp \
    ../src/math/windowfunction.cpp \
    ../src/meta/metabase.cpp \
    ../src/meta/metameasurement.cpp

HEADERS += \
    ../src/common/settings.h \
    ../src/common/workingfolder.h \
    ../src/math/bessellpf.h \
    ../src/math/windowfunction.h \
    ../src/meta/metabase.h \
    ../src/meta/metameasurement.h
