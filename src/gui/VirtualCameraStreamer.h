#ifndef VIRTUALCAMERASTREAMER_H
#define VIRTUALCAMERASTREAMER_H

#include <QObject>
#include <QVideoFrame>
#include <QString>

/**
 * @brief Streams preview frames into a v4l2loopback virtual camera device.
 *
 * The streamer opens the requested V4L2 video output device and writes
 * frames in BGR24 format, enabling other applications to consume the
 * processed camera feed.
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

public slots:
    void onVideoFrameChanged(const QVideoFrame &frame);

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
};

#endif // VIRTUALCAMERASTREAMER_H
