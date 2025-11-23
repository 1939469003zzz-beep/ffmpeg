#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QImage>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void playPause();
    void stop();
    void seekVideo(int position);
    void updateVideo();
    void speedChanged(int index);
    void toggleFullscreen();

private:
    void setupUI();
    void cleanup();
    bool loadVideo(const QString &filename);
    void decodeFrame();
    void updateTimeDisplay();
    void keyPressEvent(QKeyEvent *event) override;

    // UI 组件
    QLabel *videoLabel;
    QPushButton *openButton;
    QPushButton *playButton;
    QPushButton *stopButton;
    QPushButton *fullscreenButton;
    QSlider *seekSlider;
    QLabel *timeLabel;
    QComboBox *speedComboBox;
    QLabel *infoLabel;

    // FFmpeg 相关变量
    AVFormatContext *formatContext;
    AVCodecContext *videoCodecContext;
    SwsContext *swsContext;
    int videoStreamIndex;
    AVFrame *videoFrame;
    AVFrame *videoFrameRGB;
    uint8_t *videoBuffer;

    // 播放控制
    QTimer *timer;
    bool isPlaying;
    bool isStopped;
    qint64 duration;
    qint64 currentTime;
    double playbackSpeed;

    // 图像显示
    QImage videoImage;

    // 全屏控制
    bool isFullscreen;
};

#endif // MAINWINDOW_H
