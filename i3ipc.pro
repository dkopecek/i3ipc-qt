QT += core network
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = i3ipc-example
TEMPLATE = app
SOURCES += example.cpp
HEADERS += i3ipc.hpp
