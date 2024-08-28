TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

# QMAKE_CC += -g
# QMAKE_CXX += -g
# QMAKE_LINK += -g

DEFINES += BSPATCH_EXECUTABLE

SOURCES += \
#    ../lzma/7zFile.c \
#    ../lzma/7zStream.c \
#    ../lzma/Alloc.c \
#    ../lzma/CpuArch.c \
#    ../lzma/LzFind.c \
#    ../lzma/LzFindMt.c \
#    ../lzma/LzFindOpt.c \
    ../lzma/LzmaDec.c \
#    ../lzma/LzmaEnc.c \
#    ../lzma/LzmaLib.c \
    ../lzma/LzmaUtil/ringbuffer.c \
#    ../lzma/Threads.c \
    ../lzma/LzmaUtil/LzmaUtil.c \
    bspatch.c \

HEADERS += \
#    ../lzma/7zFile.h \
#    ../lzma/7zVersion.h \
#    ../lzma/CpuArch.h \
#    ../lzma/LzFind.h \
#    ../lzma/LzFindMt.h \
    ../lzma/7zTypes.h \
    ../lzma/LzmaDec.h \
#    ../lzma/LzmaEnc.h \
#    ../lzma/LzmaLib.h \
    ../lzma/LzmaUtil/LzmaUtil.h \
    ../lzma/LzmaUtil/ringbuffer.h \
#    ../lzma/Threads.h \
    bspatch.h \
