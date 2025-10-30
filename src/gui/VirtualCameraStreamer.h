#ifndef VIRTUALCAMERASTREAMER_H
#define VIRTUALCAMERASTREAMER_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QSize>

/**
 * @brief Streams preview frames into a v4l2loopback virtual camera device.
 *
 * The streamer opens the requested V4L2 video output device and writes
 * frames in YUYV (YUY2) format. An optional forced resolution keeps the
 * virtual camera output stable for conferencing apps that dislike runtime
 * format changes.
 */
class VirtualCameraStreamer : public QObject
{
    Q_OBJECT

public:
    explicit VirtualCameraStreamer(QObject *parent = nullptr);
    ~VirtualCameraStreamer() override;

    void setDevicePath(const QString &path);
    QString devicePath() const { return m_devicePath; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    void setForcedResolution(const QSize &resolution);
    QSize forcedResolution() const { return m_forcedResolution; }

public slots:
    void onProcessedFrameReady(const QImage &frame);

signals:
    void errorOccurred(const QString &message);

private:
    bool openDevice(int width, int height);
    void closeDevice();
    bool configureFormat(int width, int height);
    bool writeFrame(const QImage &image);

    int m_fd;
    QString m_devicePath;
    bool m_enabled;
    bool m_deviceConfigured;
    int m_frameWidth;
    int m_frameHeight;
    QSize m_forcedResolution;
};

#endif // VIRTUALCAMERASTREAMER_H
