TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += BSDIFF_EXECUTABLE

SOURCES += \
    ../lzma/7zFile.c \
    ../lzma/7zStream.c \
    ../lzma/Alloc.c \
    ../lzma/LzFind.c \
    ../lzma/LzFindMt.c \
    ../lzma/LzFindOpt.c \
    ../lzma/LzmaDec.c \
    ../lzma/LzmaEnc.c \
    ../lzma/LzmaLib.c \
    ../lzma/LzmaUtil/LzmaUtil.c \
    ../lzma/LzmaUtil/ringbuffer.c \
    ../lzma/Threads.c \
        bsdiff.c \

HEADERS += \
    ../lzma/7zFile.h \
    ../lzma/7zVersion.h \
    ../lzma/Alloc.h \
    ../lzma/CpuArch.h \
    ../lzma/LzFind.h \
    ../lzma/LzFindMt.h \
    ../lzma/LzHash.h \
    ../lzma/LzmaDec.h \
    ../lzma/LzmaEnc.h \
    ../lzma/LzmaLib.h \
    ../lzma/LzmaUtil/LzmaUtil.h \
    ../lzma/LzmaUtil/ringbuffer.h \
    ../lzma/Threads.h
