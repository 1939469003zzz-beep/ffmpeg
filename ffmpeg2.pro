QT += core gui widgets multimedia multimediawidgets

CONFIG += c++11

TARGET = ffmpeg2
TEMPLATE = app

SOURCES += main.cpp \
           mainwindow.cpp

HEADERS += mainwindow.h

# FFmpeg 配置
win32 {
    # 包含路径
    INCLUDEPATH += C:/ffmpeg/include

    # 库路径
    LIBS += -LC:/ffmpeg/lib

    # 库文件
    LIBS += -lavcodec
    LIBS += -lavformat
    LIBS += -lavutil
    LIBS += -lswscale
    LIBS += -lswresample

    # 拷贝 DLL
    QMAKE_POST_LINK += $$quote(cmd /c xcopy /Y "C:\ffmpeg\bin\*.dll" "$(OUT_PWD)" 2>nul)
}

# 禁用警告
QMAKE_CXXFLAGS += -Wno-deprecated-declarations
