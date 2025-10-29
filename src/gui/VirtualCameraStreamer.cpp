#include "VirtualCameraStreamer.h"

#include <QImage>
#include <QLoggingCategory>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(VirtualCameraLog, "obsbot.virtualcamera")

namespace {

constexpr const char *kDefaultDevicePath = "/dev/video42";

QString errnoString()
{
    return QString::fromLocal8Bit(strerror(errno));
}

} // namespace

VirtualCameraStreamer::VirtualCameraStreamer(QObject *parent)
    : QObject(parent)
    , m_fd(-1)
    , m_devicePath(QString::fromLatin1(kDefaultDevicePath))
    , m_enabled(false)
    , m_deviceConfigured(false)
    , m_frameWidth(0)
    , m_frameHeight(0)
{
}

VirtualCameraStreamer::~VirtualCameraStreamer()
{
    closeDevice();
}

void VirtualCameraStreamer::setDevicePath(const QString &path)
{
    const QString normalized = path.trimmed().isEmpty()
        ? QString::fromLatin1(kDefaultDevicePath)
        : path.trimmed();

    if (normalized == m_devicePath) {
        return;
    }

    m_devicePath = normalized;
    m_deviceConfigured = false;

    if (m_fd != -1) {
        closeDevice();
    }
}

void VirtualCameraStreamer::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;

    if (!m_enabled) {
        closeDevice();
    }
}

void VirtualCameraStreamer::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (!m_enabled) {
        return;
    }

    QVideoFrame copy(frame);
    if (!copy.isValid()) {
        return;
    }

    QImage image = copy.toImage();
    if (image.isNull()) {
        qCWarning(VirtualCameraLog) << "Failed to convert frame to QImage";
        return;
    }

    if (image.format() != QImage::Format_BGR888) {
        image = image.convertToFormat(QImage::Format_BGR888);
        if (image.isNull()) {
            qCWarning(VirtualCameraLog) << "Failed to convert frame to BGR888 format";
            return;
        }
    }

    const int width = image.width();
    const int height = image.height();

    if (!openDevice(width, height)) {
        return;
    }

    if (!writeFrame(image)) {
        closeDevice();
    }
}

bool VirtualCameraStreamer::openDevice(int width, int height)
{
    if (m_fd == -1) {
        m_fd = ::open(m_devicePath.toLocal8Bit().constData(), O_WRONLY);
        if (m_fd == -1) {
            emit errorOccurred(tr("Cannot open virtual camera device %1: %2")
                .arg(m_devicePath, errnoString()));
            qCWarning(VirtualCameraLog) << "Failed to open device" << m_devicePath << errnoString();
            m_enabled = false;
            return false;
        }
        m_deviceConfigured = false;
    }

    if (!m_deviceConfigured || width != m_frameWidth || height != m_frameHeight) {
        if (!configureFormat(width, height)) {
            closeDevice();
            m_enabled = false;
            return false;
        }
        m_deviceConfigured = true;
        m_frameWidth = width;
        m_frameHeight = height;
    }

    return true;
}

void VirtualCameraStreamer::closeDevice()
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
    m_deviceConfigured = false;
}

bool VirtualCameraStreamer::configureFormat(int width, int height)
{
    if (m_fd == -1) {
        return false;
    }

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
    format.fmt.pix.bytesperline = width * 3;
    format.fmt.pix.sizeimage = format.fmt.pix.bytesperline * height;

    if (ioctl(m_fd, VIDIOC_S_FMT, &format) == -1) {
        emit errorOccurred(tr("Failed to configure virtual camera format: %1")
            .arg(errnoString()));
        qCWarning(VirtualCameraLog) << "VIDIOC_S_FMT failed" << errnoString();
        return false;
    }

    return true;
}

bool VirtualCameraStreamer::writeFrame(const QImage &image)
{
    if (m_fd == -1) {
        return false;
    }

    const int expectedStride = image.width() * 3;
    if (image.bytesPerLine() != expectedStride) {
        emit errorOccurred(tr("Unsupported image stride for virtual camera output"));
        qCWarning(VirtualCameraLog) << "Unexpected stride" << image.bytesPerLine() << "expected" << expectedStride;
        return false;
    }

    const ssize_t frameSize = static_cast<ssize_t>(expectedStride * image.height());
    const ssize_t written = ::write(m_fd, image.constBits(), frameSize);
    if (written != frameSize) {
        emit errorOccurred(tr("Failed to write frame to virtual camera: %1")
            .arg(errnoString()));
        qCWarning(VirtualCameraLog) << "Short write to device" << written << "expected" << frameSize;
        return false;
    }

    return true;
}
