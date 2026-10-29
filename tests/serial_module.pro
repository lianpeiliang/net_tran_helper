QT += widgets network serialport testlib
CONFIG += testcase console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_serial_module
INCLUDEPATH += .. ../api ../form
include(../api/api.pri)
SOURCES += tst_serial_module.cpp ../form/frmserial.cpp
HEADERS += ../form/frmserial.h
