#include "MainWindow.h"
#include <QMessageBox>
#include <QWidget>
#include <QIcon>
#include <QEvent>
#include <QResizeEvent>
#include <QWindowStateChangeEvent>
#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QCloseEvent>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_previewAspectRatio(16.0 / 9.0)  // Default to 16:9
    , m_previewStateBeforeMinimize(false)
{
    setWindowTitle("OBSBOT Control");
    setWindowIcon(QIcon(":/icons/camera.svg"));

    // Create controller
    m_controller = new CameraController(this);

    // Connect signals
    connect(m_controller, &CameraController::cameraConnected,
            this, &MainWindow::onCameraConnected);
    connect(m_controller, &CameraController::cameraDisconnected,
            this, &MainWindow::onCameraDisconnected);
    connect(m_controller, &CameraController::stateChanged,
            this, &MainWindow::onStateChanged);
    connect(m_controller, &CameraController::commandFailed,
            this, &MainWindow::onCommandFailed);

    setupUI();
    setupTrayIcon();

    // Load configuration
    loadConfiguration();

    // Check if we should start minimized
    if (m_controller->getConfig().getSettings().startMinimized) {
        // Start hidden in tray
        QTimer::singleShot(0, this, [this]() {
            hide();
        });
    }

    // Start connecting to camera
    m_controller->connectToCamera();

    // Update status periodically
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatus);
    m_statusTimer->start(2000);
}

MainWindow::~MainWindow()
{
    // Save config on exit
    if (m_controller->isConnected()) {
        m_controller->saveConfig();
    }
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    m_mainLayout = new QHBoxLayout(centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    // Left: Preview drawer (collapsible, starts hidden, not in layout initially)
    m_previewWidget = new CameraPreviewWidget(this);
    m_previewWidget->setVisible(false);
    connect(m_previewWidget, &CameraPreviewWidget::aspectRatioChanged,
            this, &MainWindow::onPreviewAspectRatioChanged);
    connect(m_previewWidget, &CameraPreviewWidget::previewStarted,
            this, &MainWindow::onPreviewStarted);
    connect(m_previewWidget, &CameraPreviewWidget::previewFailed,
            this, &MainWindow::onPreviewFailed);
    connect(m_previewWidget, &CameraPreviewWidget::preferredFormatChanged,
            this, &MainWindow::onPreviewFormatChanged);
    // Don't add to layout yet - will be added when preview is enabled

    // Right: Controls sidebar (auto-sizes to content)
    m_sidebar = new QWidget(this);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(10, 10, 10, 10);
    sidebarLayout->setSpacing(10);

    // Preview toggle button
    m_previewToggleButton = new QPushButton("Show Camera Preview", this);
    m_previewToggleButton->setCheckable(true);
    connect(m_previewToggleButton, &QPushButton::toggled, this, &MainWindow::onTogglePreview);
    sidebarLayout->addWidget(m_previewToggleButton);

    // Device info
    m_deviceInfoLabel = new QLabel("Connecting to camera...", this);
    m_deviceInfoLabel->setStyleSheet("font-weight: bold; padding: 5px 0px;");
    m_deviceInfoLabel->setWordWrap(true);
    sidebarLayout->addWidget(m_deviceInfoLabel);

    // Camera warning (for "camera in use" messages)
    m_cameraWarningLabel = new QLabel("", this);
    m_cameraWarningLabel->setStyleSheet("font-weight: bold; padding: 5px 0px; color: #FFA500;");  // Orange color
    m_cameraWarningLabel->setWordWrap(true);
    m_cameraWarningLabel->setVisible(false);
    sidebarLayout->addWidget(m_cameraWarningLabel);

    // Create all control widgets (always visible)
    m_trackingWidget = new TrackingControlWidget(m_controller, this);
    m_ptzWidget = new PTZControlWidget(m_controller, this);
    m_settingsWidget = new CameraSettingsWidget(m_controller, this);

    sidebarLayout->addWidget(m_trackingWidget);
    sidebarLayout->addWidget(m_ptzWidget);
    sidebarLayout->addWidget(m_settingsWidget);
    sidebarLayout->addStretch();

    // Application settings
    m_startMinimizedCheckbox = new QCheckBox("Launch minimized / Close to tray", this);
    m_startMinimizedCheckbox->setStyleSheet("padding: 5px; font-size: 11px;");
    connect(m_startMinimizedCheckbox, &QCheckBox::toggled, this, &MainWindow::onStartMinimizedToggled);
    sidebarLayout->addWidget(m_startMinimizedCheckbox);

    // Bottom: Status bar
    m_statusLabel = new QLabel("Status: Initializing...", this);
    m_statusLabel->setStyleSheet("padding: 10px; font-size: 11px; border-top: 1px solid palette(mid);");
    m_statusLabel->setWordWrap(true);
    sidebarLayout->addWidget(m_statusLabel);

    // Set sidebar to natural size based on content
    m_sidebar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    m_mainLayout->addWidget(m_sidebar, 0);  // No stretch, sizes to content
}

void MainWindow::onTogglePreview(bool enabled)
{
    if (enabled) {
        // Find which video device is the OBSBOT camera
        QString devicePath = findObsbotVideoDevice();

        if (devicePath.isEmpty()) {
            // Could not detect OBSBOT camera device
            QString warningText = "⚠ Cannot detect camera device\n(OBSBOT camera not found in video devices)";
            m_cameraWarningLabel->setText(warningText);
            m_cameraWarningLabel->setVisible(true);
            m_previewToggleButton->setChecked(false);
            return;
        }

        // Check if camera is already in use BEFORE doing any layout changes
        QString process = getProcessUsingCamera(devicePath);
        if (!process.isEmpty()) {
            // Camera is in use - show warning and abort
            QString warningText = "⚠ Cannot open camera preview\n(In use by: " + process + ")";
            m_cameraWarningLabel->setText(warningText);
            m_cameraWarningLabel->setVisible(true);

            // Uncheck the button
            m_previewToggleButton->setChecked(false);
            return;
        }

        // Try to enable preview - will emit previewStarted() or previewFailed()
        m_previewWidget->enablePreview(true);

    } else {
        m_previewWidget->enablePreview(false);
        m_previewWidget->setVisible(false);

        // Remove widget from layout completely
        m_mainLayout->removeWidget(m_previewWidget);

        m_previewToggleButton->setText("Show Camera Preview");

        // Force fresh layout calculation as if starting from scratch
        m_mainLayout->invalidate();
        m_mainLayout->activate();
        m_sidebar->updateGeometry();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        // Get actual sidebar width and force window to shrink
        int sidebarWidth = m_sidebar->sizeHint().width();
        setMinimumWidth(sidebarWidth);
        setMaximumWidth(sidebarWidth);
        resize(sidebarWidth, height());

        // Clear constraints after a moment
        QTimer::singleShot(100, this, [this]() {
            setMinimumWidth(0);
            setMaximumWidth(QWIDGETSIZE_MAX);
        });
    }
}

void MainWindow::onPreviewAspectRatioChanged(double ratio)
{
    m_previewAspectRatio = ratio;

    // Update preview width if it's currently visible, based on sidebar height
    if (m_previewWidget->isVisible()) {
        int previewWidth = static_cast<int>(m_sidebar->height() * m_previewAspectRatio);
        m_previewWidget->setFixedWidth(previewWidth);
    }
}

void MainWindow::onCameraConnected(const CameraController::CameraInfo &info)
{
    QString deviceText = QString("✓ Connected:\n%1\n(v%2)")
        .arg(info.name)
        .arg(info.version);

    m_deviceInfoLabel->setText(deviceText);
    m_deviceInfoLabel->setStyleSheet("font-weight: bold; padding: 5px 0px; color: green;");

    // Apply current UI state to camera asynchronously (respects user changes before connection)
    // Use a short delay to let the connection stabilize
    QTimer::singleShot(100, this, [this]() {
        auto uiState = getUIState();
        m_controller->applyCurrentStateToCamera(uiState);
    });

    updateStatus();

    // Recalculate window width if preview is visible
    // (connection status makes sidebar wider)
    if (m_previewWidget->isVisible()) {
        QTimer::singleShot(50, this, [this]() {
            m_sidebar->updateGeometry();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            int previewWidth = m_previewWidget->width();
            int sidebarWidth = m_sidebar->sizeHint().width();
            int totalWidth = previewWidth + sidebarWidth;
            resize(totalWidth, height());
        });
    }
}

void MainWindow::onCameraDisconnected()
{
    m_deviceInfoLabel->setText("❌ Camera Disconnected");
    m_deviceInfoLabel->setStyleSheet("font-weight: bold; padding: 10px; color: red;");
    m_statusLabel->setText("Status: Not connected");
}

void MainWindow::onStateChanged(const CameraController::CameraState &state)
{
    // Update all widgets with new state
    m_trackingWidget->updateFromState(state);
    m_ptzWidget->updateFromState(state);
    m_settingsWidget->updateFromState(state);
}

void MainWindow::onCommandFailed(const QString &description, int errorCode)
{
    QMessageBox::warning(this, "Command Failed",
        QString("%1 failed with error code: %2").arg(description).arg(errorCode));
}

void MainWindow::updateStatus()
{
    if (!m_controller->isConnected()) {
        return;
    }

    auto state = m_controller->getCurrentState();

    QStringList statusParts;
    statusParts << QString("AI: %1").arg(state.aiMode == 0 ? "Off" : "On");
    statusParts << QString("Zoom: %1%").arg(state.zoomRatio);
    statusParts << QString("HDR: %1").arg(state.hdrEnabled ? "On" : "Off");
    statusParts << QString("Face AE: %1").arg(state.faceAEEnabled ? "On" : "Off");
    statusParts << QString("Focus: %1").arg(state.autoFocusEnabled ? "Auto" : "Manual");

    m_statusLabel->setText("Status: " + statusParts.join(" | "));
}

void MainWindow::loadConfiguration()
{
    std::vector<Config::ValidationError> errors;
    if (!m_controller->loadConfig(errors)) {
        // Config has validation errors
        handleConfigErrors(errors);
    }

    // Initialize UI widgets from config
    auto settings = m_controller->getConfig().getSettings();
    m_trackingWidget->setTrackingEnabled(settings.faceTracking);
    m_settingsWidget->setHDREnabled(settings.hdr);
    m_settingsWidget->setFOVMode(settings.fov);
    m_settingsWidget->setFaceAEEnabled(settings.faceAE);
    m_settingsWidget->setFaceFocusEnabled(settings.faceFocus);

    // Image controls
    m_settingsWidget->setBrightnessAuto(settings.brightnessAuto);
    m_settingsWidget->setBrightness(settings.brightness);
    m_settingsWidget->setContrastAuto(settings.contrastAuto);
    m_settingsWidget->setContrast(settings.contrast);
    m_settingsWidget->setSaturationAuto(settings.saturationAuto);
    m_settingsWidget->setSaturation(settings.saturation);
    m_settingsWidget->setWhiteBalance(settings.whiteBalance);
    m_previewWidget->setPreferredFormatId(QString::fromStdString(settings.previewFormat));

    // Application settings - block signals to prevent saving during initialization
    m_startMinimizedCheckbox->blockSignals(true);
    m_startMinimizedCheckbox->setChecked(settings.startMinimized);
    m_startMinimizedCheckbox->blockSignals(false);
}

void MainWindow::handleConfigErrors(const std::vector<Config::ValidationError> &errors)
{
    QString errorMsg = "Configuration file has errors:\n\n";
    for (const auto &err : errors) {
        if (err.lineNumber > 0) {
            errorMsg += QString("Line %1: %2\n")
                .arg(err.lineNumber)
                .arg(QString::fromStdString(err.message));
        } else {
            errorMsg += QString::fromStdString(err.message) + "\n";
        }
    }

    errorMsg += "\nWhat would you like to do?";

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Config Error");
    msgBox.setText(errorMsg);
    msgBox.setIcon(QMessageBox::Warning);

    QPushButton *ignoreBtn = msgBox.addButton("Ignore (Don't Save)", QMessageBox::RejectRole);
    QPushButton *defaultsBtn = msgBox.addButton("Reset to Defaults", QMessageBox::AcceptRole);
    QPushButton *retryBtn = msgBox.addButton("Try Again", QMessageBox::ActionRole);

    msgBox.exec();

    if (msgBox.clickedButton() == ignoreBtn) {
        // User chose to ignore - disable saving
        m_controller->getConfig().disableSaving();
    } else if (msgBox.clickedButton() == defaultsBtn) {
        // User chose to reset to defaults
        m_controller->getConfig().resetToDefaults(true);
    } else if (msgBox.clickedButton() == retryBtn) {
        // User chose to try again - reload config
        loadConfiguration();
    }
}

void MainWindow::onStateChangedSaveConfig()
{
    // Debounced auto-save - only save if config saving is enabled
    // This prevents saving during transitional states
    if (m_controller->getConfig().isSavingEnabled()) {
        m_controller->saveConfig();
    }
}

CameraController::CameraState MainWindow::getUIState() const
{
    // Construct a state object from current UI widget values
    CameraController::CameraState state = {};

    // Get state from widgets
    state.autoFramingEnabled = m_trackingWidget->isTrackingEnabled();
    state.hdrEnabled = m_settingsWidget->isHDREnabled();
    state.fovMode = m_settingsWidget->getFOVMode();
    state.faceAEEnabled = m_settingsWidget->isFaceAEEnabled();
    state.faceFocusEnabled = m_settingsWidget->isFaceFocusEnabled();

    // Image controls
    state.brightnessAuto = m_settingsWidget->isBrightnessAuto();
    state.brightness = m_settingsWidget->getBrightness();
    state.contrastAuto = m_settingsWidget->isContrastAuto();
    state.contrast = m_settingsWidget->getContrast();
    state.saturationAuto = m_settingsWidget->isSaturationAuto();
    state.saturation = m_settingsWidget->getSaturation();
    state.whiteBalance = m_settingsWidget->getWhiteBalance();

    // Get PTZ state from controller (defaults from config)
    auto currentState = m_controller->getCurrentState();
    state.pan = currentState.pan;
    state.tilt = currentState.tilt;
    state.zoom = currentState.zoom;

    return state;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Update preview width when window is manually resized, based on sidebar height
    if (m_previewWidget->isVisible()) {
        int previewWidth = static_cast<int>(m_sidebar->height() * m_previewAspectRatio);
        m_previewWidget->setFixedWidth(previewWidth);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *stateEvent = static_cast<QWindowStateChangeEvent*>(event);

        if (windowState() & Qt::WindowMinimized) {
            // Window is being minimized

            // Save preview state
            m_previewStateBeforeMinimize = m_previewWidget->isPreviewEnabled();

            // Disable preview if it's on
            if (m_previewStateBeforeMinimize) {
                m_previewToggleButton->setChecked(false);
            }

            // Disconnect from camera to free resources
            m_controller->disconnectFromCamera();

        } else if (stateEvent->oldState() & Qt::WindowMinimized) {
            // Window is being restored from minimized state

            // ALWAYS reconnect to camera (regardless of preview state)
            m_controller->connectToCamera();

            // ONLY restore preview if it was enabled before minimize
            if (m_previewStateBeforeMinimize) {
                QTimer::singleShot(1000, this, [this]() {
                    m_previewToggleButton->setChecked(true);
                });
            }
        }
    }
}

void MainWindow::onPreviewStarted()
{
    // Clear warning when preview successfully starts
    m_cameraWarningLabel->setVisible(false);
    m_cameraWarningLabel->setText("");

    // NOW do the layout expansion (camera successfully opened)
    // Force fresh layout calculation
    m_sidebar->updateGeometry();
    m_mainLayout->invalidate();
    m_mainLayout->activate();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // Add preview widget to layout (at position 0, before sidebar)
    m_mainLayout->insertWidget(0, m_previewWidget, 1);  // Stretch factor 1

    // Calculate preview width based on actual sidebar height
    int previewWidth = static_cast<int>(m_sidebar->height() * m_previewAspectRatio);
    m_previewWidget->setFixedWidth(previewWidth);

    m_previewWidget->setVisible(true);
    m_previewToggleButton->setText("Hide Camera Preview");

    // Force layout recalculation after adding preview
    m_mainLayout->invalidate();
    m_mainLayout->activate();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // Expand window to fit preview + sidebar
    // Use sizeHint() to get intended width, not current width
    int sidebarWidth = m_sidebar->sizeHint().width();
    int totalWidth = previewWidth + sidebarWidth;
    setMinimumWidth(totalWidth);
    resize(totalWidth, height());

    // Clear constraints after a moment
    QTimer::singleShot(100, this, [this]() {
        setMinimumWidth(0);
    });
}

void MainWindow::onPreviewFailed(const QString &error)
{
    // Show warning when preview fails
    QString warningText = "⚠ Cannot open camera preview";

    // Try to detect which process is using the camera
    QString process = getProcessUsingCamera("/dev/video0");
    if (!process.isEmpty()) {
        warningText += "\n(In use by: " + process + ")";
    } else {
        warningText += "\n(In use by another application)";
    }

    m_cameraWarningLabel->setText(warningText);
    m_cameraWarningLabel->setVisible(true);

    // Uncheck the toggle button since preview failed
    m_previewToggleButton->setChecked(false);
}

void MainWindow::onPreviewFormatChanged(const QString &formatId)
{
    auto settings = m_controller->getConfig().getSettings();
    settings.previewFormat = formatId.toStdString();
    m_controller->getConfig().setSettings(settings);
    m_controller->saveConfig();
}

QString MainWindow::findObsbotVideoDevice()
{
    // Find which /dev/video* device is the OBSBOT camera
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();

    for (const QCameraDevice &cameraDevice : cameras) {
        if (cameraDevice.description().contains("OBSBOT", Qt::CaseInsensitive) ||
            cameraDevice.description().contains("Meet", Qt::CaseInsensitive)) {
            // Try to extract device path from ID
            // QCameraDevice ID on Linux is typically the device path like "/dev/video0"
            QString id = cameraDevice.id();
            if (id.startsWith("/dev/video")) {
                return id;
            }
        }
    }

    // Not found - return empty string
    return QString();
}

QString MainWindow::getProcessUsingCamera(const QString &devicePath)
{
    // Use lsof to find which process has the camera device open
    QProcess process;
    process.start("lsof", QStringList() << devicePath);
    process.waitForFinished(1000);

    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n');

    // Get our own PID to filter ourselves out
    qint64 ourPid = QCoreApplication::applicationPid();

    // lsof output format:
    // COMMAND   PID USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
    // chrome  12345 user   42u   CHR  81,0      0t0  123 /dev/video0

    for (int i = 1; i < lines.size(); ++i) {  // Skip header line
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() >= 2) {
            QString command = parts[0];
            QString pidStr = parts[1];
            qint64 pid = pidStr.toLongLong();

            // Skip our own process - we already have the camera for control
            if (pid == ourPid) {
                continue;
            }

            return QString("%1 (PID: %2)").arg(command).arg(pidStr);
        }
    }

    return QString();  // Not found (or only we're using it)
}

void MainWindow::setupTrayIcon()
{
    // Create system tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/camera.svg"));
    m_trayIcon->setToolTip("OBSBOT Control");

    // Create context menu
    m_trayMenu = new QMenu(this);
    QAction *showHideAction = m_trayMenu->addAction("Show/Hide");
    m_trayMenu->addSeparator();
    QAction *quitAction = m_trayMenu->addAction("Quit");

    connect(showHideAction, &QAction::triggered, this, &MainWindow::onShowHideAction);
    connect(quitAction, &QAction::triggered, this, &MainWindow::onQuitAction);

    m_trayIcon->setContextMenu(m_trayMenu);

    // Connect activation (click) signal
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);

    // Show the tray icon
    m_trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Single click - toggle visibility
        onShowHideAction();
    }
}

void MainWindow::onShowHideAction()
{
    if (isVisible()) {
        // Save preview state
        m_previewStateBeforeMinimize = m_previewWidget->isPreviewEnabled();

        // Disable preview if it's on
        if (m_previewStateBeforeMinimize) {
            m_previewToggleButton->setChecked(false);
        }

        // Disconnect from camera to free resources
        m_controller->disconnectFromCamera();

        hide();
    } else {
        // ALWAYS reconnect to camera when restoring from tray
        m_controller->connectToCamera();

        // ONLY restore preview if it was enabled before hiding
        if (m_previewStateBeforeMinimize) {
            QTimer::singleShot(1000, this, [this]() {
                m_previewToggleButton->setChecked(true);
            });
        }

        show();
        activateWindow();
        raise();
    }
}

void MainWindow::onQuitAction()
{
    // Save config before quitting
    if (m_controller->isConnected()) {
        m_controller->saveConfig();
    }
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Check if we should minimize to tray or quit
    auto settings = m_controller->getConfig().getSettings();
    std::cout << "[MainWindow] closeEvent: startMinimized = " << settings.startMinimized << std::endl;

    if (settings.startMinimized && m_trayIcon && m_trayIcon->isVisible()) {
        std::cout << "[MainWindow] closeEvent: Minimizing to tray" << std::endl;
        // Minimize to tray instead of closing
        // Save preview state
        m_previewStateBeforeMinimize = m_previewWidget->isPreviewEnabled();

        // Disable preview if it's on
        if (m_previewStateBeforeMinimize) {
            m_previewToggleButton->setChecked(false);
        }

        // Disconnect from camera to free resources
        m_controller->disconnectFromCamera();

        hide();
        event->ignore();

        // Show notification on first minimize
        static bool firstTime = true;
        if (firstTime) {
            m_trayIcon->showMessage(
                "OBSBOT Control",
                "Application minimized to system tray. Click the tray icon to restore.",
                QSystemTrayIcon::Information,
                3000
            );
            firstTime = false;
        }
    } else {
        std::cout << "[MainWindow] closeEvent: Quitting application" << std::endl;
        // Actually quit the application
        // Save config before quitting
        if (m_controller->isConnected()) {
            m_controller->saveConfig();
        }
        event->accept();
        QApplication::quit();
    }
}

void MainWindow::onStartMinimizedToggled(bool checked)
{
    // Update config and save
    std::cout << "[MainWindow] Checkbox toggled to: " << checked << std::endl;
    auto settings = m_controller->getConfig().getSettings();
    std::cout << "[MainWindow] Before: startMinimized = " << settings.startMinimized << std::endl;
    settings.startMinimized = checked;
    m_controller->getConfig().setSettings(settings);
    std::cout << "[MainWindow] Calling saveConfig()..." << std::endl;
    bool saved = m_controller->saveConfig();
    std::cout << "[MainWindow] saveConfig() returned: " << saved << std::endl;
}
