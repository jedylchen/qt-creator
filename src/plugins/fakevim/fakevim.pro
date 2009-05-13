TEMPLATE = lib
TARGET = FakeVim

# CONFIG += single
include(../../libs/cplusplus/cplusplus.pri)
include(../../qtcreatorplugin.pri)
include(../../plugins/projectexplorer/projectexplorer.pri)
include(../../plugins/coreplugin/coreplugin.pri)
include(../../plugins/texteditor/texteditor.pri)
include(../../plugins/texteditor/cppeditor.pri)
include(../../shared/indenter/indenter.pri)

# DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
QT += gui

SOURCES += \
    fakevimactions.cpp \
    fakevimhandler.cpp \
    fakevimplugin.cpp

HEADERS += \
    fakevimactions.h \
    fakevimhandler.h \
    fakevimplugin.h

FORMS += \
    fakevimoptions.ui

OTHER_FILES += FakeVim.pluginspec
