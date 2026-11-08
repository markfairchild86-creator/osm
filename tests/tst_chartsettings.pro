QT += testlib qml quick opengl widgets
CONFIG += c++1z console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tst_chartsettings

DEFINES += GRAPH_OPENGL

INCLUDEPATH += \
    .. \
    ../src \
    ../src/chart \
    ../src/common \
    ../src/meta \
    ../src/remote \
    ../src/generator \
    ../src/source \
    ../src/model \
    ../src/chart/opengl

SOURCES += \
    tst_chartsettings.cpp \
    chartsettingsstubs.cpp \
    ../src/abstract/data.cpp \
    ../src/abstract/levelsdata.cpp \
    ../src/abstract/source.cpp \
    ../src/chart/axis.cpp \
    ../src/chart/coherenceplot.cpp \
    ../src/chart/crestfactorplot.cpp \
    ../src/chart/cursorhelper.cpp \
    ../src/chart/frequencybasedplot.cpp \
    ../src/chart/groupdelayplot.cpp \
    ../src/chart/impulseplot.cpp \
    ../src/chart/levelobject.cpp \
    ../src/chart/levelplot.cpp \
    ../src/chart/magnitudeplot.cpp \
    ../src/chart/nyquistplot.cpp \
    ../src/chart/painteditem.cpp \
    ../src/chart/palette.cpp \
    ../src/chart/phasedelayplot.cpp \
    ../src/chart/phaseplot.cpp \
    ../src/chart/plot.cpp \
    ../src/chart/rtaplot.cpp \
    ../src/chart/speechplot.cpp \
    ../src/chart/spectrogramplot.cpp \
    ../src/chart/stepplot.cpp \
    ../src/chart/variablechart.cpp \
    ../src/chart/xyplot.cpp \
    ../src/common/settings.cpp \
    ../src/common/weightingcurvestore.cpp \
    ../src/common/workingfolder.cpp \
    ../src/math/biquad.cpp \
    ../src/math/integration_tree.cpp \
    ../src/math/leq.cpp \
    ../src/math/meter.cpp \
    ../src/math/speechintelligibility.cpp \
    ../src/math/weighting.cpp \
    ../src/math/weightingcurve.cpp \
    ../src/shared/source_shared.cpp \
    ../src/targettrace.cpp

HEADERS += \
    ../src/abstract/data.h \
    ../src/abstract/source.h \
    ../src/chart/axis.h \
    ../src/chart/coherenceplot.h \
    ../src/chart/crestfactorplot.h \
    ../src/chart/cursorhelper.h \
    ../src/chart/frequencybasedplot.h \
    ../src/chart/groupdelayplot.h \
    ../src/chart/impulseplot.h \
    ../src/chart/levelobject.h \
    ../src/chart/levelplot.h \
    ../src/chart/magnitudeplot.h \
    ../src/chart/nyquistplot.h \
    ../src/chart/painteditem.h \
    ../src/chart/palette.h \
    ../src/chart/phasedelayplot.h \
    ../src/chart/phaseplot.h \
    ../src/chart/plot.h \
    ../src/chart/rtaplot.h \
    ../src/chart/speechplot.h \
    ../src/chart/seriesesitem.h \
    ../src/chart/spectrogramplot.h \
    ../src/chart/stepplot.h \
    ../src/chart/variablechart.h \
    ../src/chart/xyplot.h \
    ../src/common/settings.h \
    ../src/common/weightingcurvestore.h \
    ../src/math/weightingcurve.h \
    ../src/math/speechintelligibility.h \
    ../src/shared/source_shared.h \
    ../src/targettrace.h
