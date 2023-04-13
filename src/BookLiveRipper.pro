QT       += core gui concurrent network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = BookLiveRipper
TEMPLATE = app

SOURCES += main.cpp\
    bookliveripper.cpp \
    decoder.cpp \
    login.cpp \
    bookdownloadandexport.cpp

HEADERS  += bookliveripper.h \
    decoder.h \
    login.h \
    bookdownloadandexport.h \
		Download.h

FORMS    += bookliveripper.ui \
    login.ui \
    bookdownloadandexport.ui


# Copy ssl DLLs to the output directory
# requires "install" to be added as make argument

CONFIG(debug, debug|release) {
    sslDLLs.path = $$OUT_PWD/debug
}

CONFIG(release, debug|release) {
    sslDLLs.path = $$OUT_PWD/release
}

contains(QT_ARCH, i386) {
    sslDLLs.files = openssl_x86/*.dll
} else {
    sslDLLs.files = openssl_x64/*.dll
}


INSTALLS += sslDLLs

RESOURCES += \
    resources.qrc
