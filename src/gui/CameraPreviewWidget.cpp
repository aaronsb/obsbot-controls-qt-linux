#include "CameraPreviewWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMediaDevices>
#include <QSignalBlocker>
#include <algorithm>
#include <QtMath>
#include <QSet>

namespace {
QString formatIdFor(const QCameraFormat &format)
{
    if (format.isNull()) {
        return QStringLiteral("auto");
    }

    const QSize resolution = format.resolution();
    const int fps = qRound(format.maxFrameRate());
    return QStringLiteral("%1x%2@%3").arg(resolution.width()).arg(resolution.height()).arg(fps);
}
}

CameraPreviewWidget::CameraPreviewWidget(QWidget *parent)
    : QWidget(parent)
    , m_camera(nullptr)
    , m_captureSession(nullptr)
    , m_videoWidget(nullptr)
    , m_formatCombo(nullptr)
    , m_selectedFormatId("auto")
    , m_previewEnabled(false)
{
    setupUI();
}

CameraPreviewWidget::~CameraPreviewWidget()
{
    stopPreview();
}

void CameraPreviewWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->setContentsMargins(6, 4, 6, 4);
    controlLayout->setSpacing(8);
    QLabel *formatLabel = new QLabel("Preview Quality:", this);
    formatLabel->setStyleSheet("font-size: 11px;");
    m_formatCombo = new QComboBox(this);
    m_formatCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_formatCombo->addItem("Auto", "auto");
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraPreviewWidget::onFormatSelectionChanged);
    controlLayout->addWidget(formatLabel);
    controlLayout->addWidget(m_formatCombo, 1);
    layout->addLayout(controlLayout);

    // Video widget fills entire preview area
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoWidget->setStyleSheet("background-color: black;");
    layout->addWidget(m_videoWidget);
}

bool CameraPreviewWidget::isPreviewEnabled() const
{
    return m_previewEnabled;
}

void CameraPreviewWidget::enablePreview(bool enabled)
{
    if (enabled == m_previewEnabled) {
        return;
    }

    if (enabled) {
        startPreview();
    } else {
        stopPreview();
    }
}

void CameraPreviewWidget::setPreferredFormatId(const QString &formatId)
{
    const QString requestedId = formatId.isEmpty() ? QStringLiteral("auto") : formatId;
    if (m_formatCombo) {
        QSignalBlocker blocker(m_formatCombo);
        int idx = m_formatCombo->findData(requestedId);
        if (idx < 0) {
            idx = 0;
        }
        m_formatCombo->setCurrentIndex(idx);
        m_selectedFormatId = m_formatCombo->itemData(idx).toString();
    } else {
        m_selectedFormatId = requestedId;
    }
}

void CameraPreviewWidget::refreshFormatOptions(const QCameraDevice &device)
{
    if (!m_formatCombo) {
        return;
    }

    m_availableFormats = device.videoFormats();
    std::sort(m_availableFormats.begin(), m_availableFormats.end(), [](const QCameraFormat &a, const QCameraFormat &b) {
        const QSize ar = a.resolution();
        const QSize br = b.resolution();
        const int aPixels = ar.width() * ar.height();
        const int bPixels = br.width() * br.height();
        if (aPixels == bPixels) {
            return a.maxFrameRate() > b.maxFrameRate();
        }
        return aPixels > bPixels;
    });

    QSignalBlocker blocker(m_formatCombo);
    m_formatCombo->clear();
    m_formatCombo->addItem("Auto", "auto");

    QSet<QString> seen;
    for (const QCameraFormat &format : m_availableFormats) {
        const QString id = formatIdFor(format);
        if (seen.contains(id)) {
            continue;
        }
        seen.insert(id);

        const QSize res = format.resolution();
        const int fps = qRound(format.maxFrameRate());
        const QString label = QString("%1 x %2 @ %3 fps").arg(res.width()).arg(res.height()).arg(fps);
        m_formatCombo->addItem(label, id);
    }

    int idx = m_formatCombo->findData(m_selectedFormatId);
    if (idx < 0) {
        idx = 0;
    }
    m_formatCombo->setCurrentIndex(idx);
    m_selectedFormatId = m_formatCombo->itemData(idx).toString();
}

QCameraFormat CameraPreviewWidget::findFormatById(const QString &id) const
{
    if (id == QStringLiteral("auto")) {
        return QCameraFormat();
    }

    for (const QCameraFormat &format : m_availableFormats) {
        if (formatIdFor(format) == id) {
            return format;
        }
    }
    return QCameraFormat();
}

void CameraPreviewWidget::startPreview()
{
    if (m_previewEnabled) {
        return;
    }

    // Find OBSBOT camera
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    QCameraDevice selectedCamera;

    for (const QCameraDevice &cameraDevice : cameras) {
        if (cameraDevice.description().contains("OBSBOT", Qt::CaseInsensitive) ||
            cameraDevice.description().contains("Meet", Qt::CaseInsensitive)) {
            selectedCamera = cameraDevice;
            break;
        }
    }

    if (selectedCamera.isNull() && !cameras.isEmpty()) {
        // Fallback to first camera
        selectedCamera = cameras.first();
    }

    if (selectedCamera.isNull()) {
        emit previewFailed("No camera found");
        return;
    }

    refreshFormatOptions(selectedCamera);

    // Create camera
    m_camera = new QCamera(selectedCamera, this);
    connect(m_camera, &QCamera::errorOccurred, this, &CameraPreviewWidget::onCameraError);

    if (m_selectedFormatId != QStringLiteral("auto")) {
        QCameraFormat desired = findFormatById(m_selectedFormatId);
        if (!desired.isNull()) {
            m_camera->setCameraFormat(desired);
        }
    }

    // Create capture session
    m_captureSession = new QMediaCaptureSession(this);
    m_captureSession->setCamera(m_camera);
    m_captureSession->setVideoOutput(m_videoWidget);

    // Start camera
    m_camera->start();

    // Check if camera actually started
    if (m_camera->error() != QCamera::NoError) {
        emit previewFailed(m_camera->errorString());
        stopPreview();
        return;
    }

    m_previewEnabled = true;

    // Get camera resolution to calculate aspect ratio
    QCameraFormat format = m_camera->cameraFormat();
    QSize resolution = format.resolution();
    if (resolution.width() > 0 && resolution.height() > 0) {
        double aspectRatio = static_cast<double>(resolution.width()) / static_cast<double>(resolution.height());
        emit aspectRatioChanged(aspectRatio);
    } else {
        // Fallback to 16:9 if we can't get resolution
        emit aspectRatioChanged(16.0 / 9.0);
    }

    emit previewStateChanged(true);
    emit previewStarted();
}

void CameraPreviewWidget::stopPreview()
{
    if (!m_previewEnabled) {
        return;
    }

    if (m_camera) {
        m_camera->stop();
        delete m_camera;
        m_camera = nullptr;
    }

    if (m_captureSession) {
        delete m_captureSession;
        m_captureSession = nullptr;
    }

    m_previewEnabled = false;
    emit previewStateChanged(false);
}

void CameraPreviewWidget::onCameraError(QCamera::Error error)
{
    if (error != QCamera::NoError && m_camera) {
        emit previewFailed(m_camera->errorString());
        stopPreview();
    }
}

void CameraPreviewWidget::onFormatSelectionChanged(int index)
{
    Q_UNUSED(index);

    if (!m_formatCombo) {
        return;
    }

    QString id = m_formatCombo->currentData().toString();
    if (id.isEmpty()) {
        id = QStringLiteral("auto");
    }

    if (id == m_selectedFormatId) {
        return;
    }

    m_selectedFormatId = id;
    emit preferredFormatChanged(m_selectedFormatId);

    if (m_previewEnabled) {
        stopPreview();
        startPreview();
    }
}
