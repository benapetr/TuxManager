# Compile the application widgets into an isolated UI regression test.
include(../src/TuxManager.pro)
QT += testlib
CONFIG += testcase console
CONFIG -= lrelease embed_translations
TARGET = appearance-tests
SOURCES -= main.cpp
for(file, SOURCES): TEST_SOURCES += $$PWD/../src/$$file
for(file, HEADERS): TEST_HEADERS += $$PWD/../src/$$file
for(file, FORMS): TEST_FORMS += $$PWD/../src/$$file
for(file, RESOURCES): TEST_RESOURCES += $$PWD/../src/$$file
SOURCES = $$TEST_SOURCES $$PWD/appearance_test.cpp
HEADERS = $$TEST_HEADERS
FORMS = $$TEST_FORMS
RESOURCES = $$TEST_RESOURCES
TRANSLATIONS =
INCLUDEPATH += $$PWD/../src
