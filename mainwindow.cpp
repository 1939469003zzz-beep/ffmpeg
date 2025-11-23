#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTime>
#include <QDebug>
#include <QAction>
#include <QMenuBar>
#include <QStatusBar>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , formatContext(nullptr)
    , videoCodecContext(nullptr)
    , swsContext(nullptr)
    , videoStreamIndex(-1)
    , videoFrame(nullptr)
    , videoFrameRGB(nullptr)
    , videoBuffer(nullptr)
    , isPlaying(false)
    , isStopped(true)
    , duration(0)
    , currentTime(0)
    , playbackSpeed(1.0)
    , isFullscreen(false)
{
    setupUI();

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateVideo);
}

MainWindow::~MainWindow()
{
    cleanup();
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 视频显示区域
    videoLabel = new QLabel(this);
    videoLabel->setMinimumSize(640, 480);
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setText("请打开一个视频文件");
    videoLabel->setStyleSheet("border: 1px solid gray; background-color: black; color: white;");
    videoLabel->setScaledContents(false);
    mainLayout->addWidget(videoLabel, 1);

    // 信息显示
    infoLabel = new QLabel(this);
    infoLabel->setText("就绪");
    infoLabel->setStyleSheet("color: gray; font-size: 10px;");
    mainLayout->addWidget(infoLabel);

    // 控制按钮区域
    QHBoxLayout *controlLayout = new QHBoxLayout();

    openButton = new QPushButton("打开", this);
    playButton = new QPushButton("播放", this);
    stopButton = new QPushButton("停止", this);
    fullscreenButton = new QPushButton("全屏", this);

    playButton->setEnabled(false);
    stopButton->setEnabled(false);

    seekSlider = new QSlider(Qt::Horizontal, this);
    seekSlider->setEnabled(false);

    timeLabel = new QLabel("00:00:00 / 00:00:00", this);
    timeLabel->setMinimumWidth(120);

    speedComboBox = new QComboBox(this);
    speedComboBox->addItems({"0.5x", "1.0x", "1.5x", "2.0x"});
    speedComboBox->setCurrentIndex(1);
    speedComboBox->setEnabled(false);

    controlLayout->addWidget(openButton);
    controlLayout->addWidget(playButton);
    controlLayout->addWidget(stopButton);
    controlLayout->addWidget(seekSlider);
    controlLayout->addWidget(timeLabel);
    controlLayout->addWidget(new QLabel("速度:", this));
    controlLayout->addWidget(speedComboBox);
    controlLayout->addWidget(fullscreenButton);

    mainLayout->addLayout(controlLayout);

    // 连接信号槽
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(playButton, &QPushButton::clicked, this, &MainWindow::playPause);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stop);
    connect(seekSlider, &QSlider::sliderMoved, this, &MainWindow::seekVideo);
    connect(speedComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::speedChanged);
    connect(fullscreenButton, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);

    setWindowTitle("FFmpeg 视频播放器");
    resize(800, 600);
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "打开视频文件", "",
                                                    "视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv)");

    if (!filename.isEmpty()) {
        if (loadVideo(filename)) {
            playButton->setEnabled(true);
            stopButton->setEnabled(true);
            seekSlider->setEnabled(true);
            speedComboBox->setEnabled(true);
            isPlaying = true;
            isStopped = false;
            timer->start(1000 / (30 * playbackSpeed));
            playButton->setText("暂停");
        } else {
            QMessageBox::warning(this, "错误", "无法加载视频文件");
        }
    }
}

bool MainWindow::loadVideo(const QString &filename)
{
    cleanup();

    qDebug() << "正在加载视频文件:" << filename;

    // 打开视频文件
    if (avformat_open_input(&formatContext, filename.toUtf8().constData(), nullptr, nullptr) != 0) {
        qDebug() << "无法打开视频文件";
        return false;
    }

    // 获取流信息
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        qDebug() << "无法获取流信息";
        return false;
    }

    // 查找视频流
    videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            qDebug() << "找到视频流，索引:" << i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        qDebug() << "未找到视频流";
        return false;
    }

    // 初始化视频解码器
    AVCodecParameters *videoCodecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec *videoCodec = avcodec_find_decoder(videoCodecParameters->codec_id);

    if (!videoCodec) {
        qDebug() << "不支持的视频解码器";
        return false;
    }

    videoCodecContext = avcodec_alloc_context3(videoCodec);
    if (avcodec_parameters_to_context(videoCodecContext, videoCodecParameters) < 0) {
        qDebug() << "无法创建视频解码器上下文";
        return false;
    }

    if (avcodec_open2(videoCodecContext, videoCodec, nullptr) < 0) {
        qDebug() << "无法打开视频解码器";
        return false;
    }

    videoFrame = av_frame_alloc();
    videoFrameRGB = av_frame_alloc();

    qDebug() << "视频解码器初始化成功:" << avcodec_get_name(videoCodecParameters->codec_id);
    qDebug() << "视频尺寸:" << videoCodecContext->width << "x" << videoCodecContext->height;

    // 计算持续时间
    if (formatContext->duration != AV_NOPTS_VALUE) {
        duration = formatContext->duration / AV_TIME_BASE;
    } else {
        duration = formatContext->streams[videoStreamIndex]->duration *
                   av_q2d(formatContext->streams[videoStreamIndex]->time_base);
    }

    qDebug() << "视频时长:" << duration << "秒";

    // 设置滑块范围
    seekSlider->setRange(0, duration * 1000);
    seekSlider->setValue(0);

    // 更新时间显示
    updateTimeDisplay();

    // 显示第一帧
    updateVideo();

    return true;
}

void MainWindow::playPause()
{
    if (!formatContext) return;

    if (!isPlaying) {
        timer->start(1000 / (30 * playbackSpeed));
        playButton->setText("暂停");
        isPlaying = true;
        isStopped = false;
    } else {
        timer->stop();
        playButton->setText("播放");
        isPlaying = false;
    }
}

void MainWindow::stop()
{
    if (!formatContext) return;

    timer->stop();
    playButton->setText("播放");
    isPlaying = false;
    isStopped = true;

    // 重置到开始位置
    av_seek_frame(formatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (videoCodecContext) {
        avcodec_flush_buffers(videoCodecContext);
    }

    currentTime = 0;
    seekSlider->setValue(0);
    updateTimeDisplay();

    // 显示第一帧
    updateVideo();
}

void MainWindow::seekVideo(int position)
{
    if (!formatContext) return;

    double target_seconds = position / 1000.0;
    int64_t target_ts = (int64_t)(target_seconds * AV_TIME_BASE);

    if (av_seek_frame(formatContext, -1, target_ts, AVSEEK_FLAG_BACKWARD) >= 0) {
        if (videoCodecContext) {
            avcodec_flush_buffers(videoCodecContext);
        }
        currentTime = target_seconds;
        updateTimeDisplay();
        updateVideo(); // 立即更新显示
    }
}

void MainWindow::updateVideo()
{
    if (!formatContext || !isPlaying) return;

    AVPacket *packet = av_packet_alloc();
    int ret = 0;
    bool videoDecoded = false;

    // 读取并解码帧，直到成功解码一帧视频
    while (!videoDecoded && (ret = av_read_frame(formatContext, packet)) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            // 视频包
            if (avcodec_send_packet(videoCodecContext, packet) == 0) {
                while (avcodec_receive_frame(videoCodecContext, videoFrame) == 0) {
                    decodeFrame();
                    videoDecoded = true;

                    // 更新当前时间
                    if (videoFrame->pts != AV_NOPTS_VALUE) {
                        currentTime = videoFrame->pts * av_q2d(formatContext->streams[videoStreamIndex]->time_base);
                        seekSlider->setValue(currentTime * 1000);
                        updateTimeDisplay();
                    }
                    break;
                }
            }
        }

        av_packet_unref(packet);

        if (videoDecoded) {
            break;
        }
    }

    av_packet_free(&packet);

    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            qDebug() << "视频播放结束";
            if (isPlaying) {
                stop();
            }
        } else {
            qDebug() << "读取帧错误:" << ret;
        }
    }
}

void MainWindow::decodeFrame()
{
    if (!videoFrame || !videoCodecContext) return;

    // 第一次解码时创建转换上下文和缓冲区
    if (!swsContext) {
        swsContext = sws_getContext(
            videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
            videoCodecContext->width, videoCodecContext->height, AV_PIX_FMT_RGB32,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!swsContext) {
            qDebug() << "无法创建图像转换上下文";
            return;
        }

        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32,
                                                videoCodecContext->width, videoCodecContext->height, 1);
        videoBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

        av_image_fill_arrays(videoFrameRGB->data, videoFrameRGB->linesize, videoBuffer,
                             AV_PIX_FMT_RGB32, videoCodecContext->width, videoCodecContext->height, 1);

        qDebug() << "创建图像转换上下文，缓冲区大小:" << numBytes;
    }

    // 转换帧格式
    sws_scale(swsContext, videoFrame->data, videoFrame->linesize, 0,
              videoCodecContext->height, videoFrameRGB->data, videoFrameRGB->linesize);

    // 创建QImage并显示
    videoImage = QImage(videoFrameRGB->data[0], videoCodecContext->width, videoCodecContext->height,
                        videoFrameRGB->linesize[0], QImage::Format_RGB32);

    if (!videoImage.isNull()) {
        // 缩放图像以适应标签大小，同时保持宽高比
        QPixmap pixmap = QPixmap::fromImage(videoImage);
        QSize labelSize = videoLabel->size();
        QPixmap scaledPixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        videoLabel->setPixmap(scaledPixmap);
    }
}

void MainWindow::speedChanged(int index)
{
    double speeds[] = {0.5, 1.0, 1.5, 2.0};
    playbackSpeed = speeds[index];

    if (isPlaying) {
        timer->stop();
        timer->start(1000 / (30 * playbackSpeed));
    }
}

void MainWindow::toggleFullscreen()
{
    if (isFullscreen) {
        showNormal();
        menuBar()->show();
        statusBar()->show();
        isFullscreen = false;
        fullscreenButton->setText("全屏");
    } else {
        showFullScreen();
        menuBar()->hide();
        statusBar()->hide();
        isFullscreen = true;
        fullscreenButton->setText("退出全屏");
    }
}

void MainWindow::updateTimeDisplay()
{
    QTime current(0, 0, 0);
    current = current.addSecs(static_cast<int>(currentTime));

    QTime total(0, 0, 0);
    total = total.addSecs(static_cast<int>(duration));

    timeLabel->setText(QString("%1 / %2")
                           .arg(current.toString("hh:mm:ss"), total.toString("hh:mm:ss")));
}

void MainWindow::cleanup()
{
    if (timer->isActive()) {
        timer->stop();
    }

    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }

    if (videoBuffer) {
        av_free(videoBuffer);
        videoBuffer = nullptr;
    }

    if (videoFrame) {
        av_frame_free(&videoFrame);
    }

    if (videoFrameRGB) {
        av_frame_free(&videoFrameRGB);
    }

    if (videoCodecContext) {
        avcodec_free_context(&videoCodecContext);
    }

    if (formatContext) {
        avformat_close_input(&formatContext);
    }

    videoStreamIndex = -1;
    isPlaying = false;
    isStopped = true;
    currentTime = 0;
    duration = 0;
    playbackSpeed = 1.0;

    // 清空显示
    videoLabel->clear();
    videoLabel->setText("请打开一个视频文件");
    timeLabel->setText("00:00:00 / 00:00:00");
    seekSlider->setValue(0);
    playButton->setText("播放");
    playButton->setEnabled(false);
    stopButton->setEnabled(false);
    seekSlider->setEnabled(false);
    speedComboBox->setEnabled(false);
    infoLabel->setText("就绪");
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        playPause();
        break;
    case Qt::Key_Escape:
        if (isFullscreen) {
            toggleFullscreen();
        }
        break;
    case Qt::Key_Left:
        seekVideo(qMax(0, seekSlider->value() - 5000)); // 后退5秒
        break;
    case Qt::Key_Right:
        seekVideo(qMin(seekSlider->maximum(), seekSlider->value() + 5000)); // 前进5秒
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}
