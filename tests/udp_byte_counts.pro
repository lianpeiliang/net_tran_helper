QT += widgets network testlib
CONFIG += testcase console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_udp_byte_counts
INCLUDEPATH += .. ../api ../form
include(../api/api.pri)
SOURCES += tst_udp_byte_counts.cpp ../form/frmudpclient.cpp ../form/frmudpserver.cpp
HEADERS += ../form/frmudpclient.h ../form/frmudpserver.h
FORMS += ../form/frmudpclient.ui ../form/frmudpserver.ui
