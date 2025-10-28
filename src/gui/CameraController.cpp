#include "CameraController.h"
#include <QThread>

CameraController::CameraController(QObject *parent)
    : QObject(parent)
    , m_connected(false)
    , m_settlingTimer(nullptr)
{
    m_currentState = {};
    m_cachedState = {};

    // Create settling timer
    m_settlingTimer = new QTimer(this);
    m_settlingTimer->setSingleShot(true);
}

CameraController::~CameraController()
{
}

void CameraController::connectToCamera()
{
    // Setup device detection callback
    auto onDevChanged = [this](std::string dev_sn, bool connected, void *param) {
        if (connected) {
            // Device connected
            auto dev_list = Devices::get().getDevList();
            if (!dev_list.empty()) {
                m_device = dev_list.front();
                m_connected = true;

                m_cameraInfo.name = QString::fromStdString(m_device->devName());
                m_cameraInfo.serialNumber = QString::fromStdString(m_device->devSn());
                m_cameraInfo.version = QString::fromStdString(m_device->devVersion());
                m_cameraInfo.productType = m_device->productType();
                m_cameraInfo.connected = true;

                emit cameraConnected(m_cameraInfo);
                updateState();
            }
        } else {
            m_connected = false;
            m_cameraInfo.connected = false;
            emit cameraDisconnected();
        }
    };

    Devices::get().setDevChangedCallback(onDevChanged, nullptr);
    Devices::get().setEnableMdnsScan(false);  // USB only

    // Actively check for existing devices (handles reconnection scenario)
    // The callback only fires on connect/disconnect events, so if the device
    // is already connected (e.g., after window restore), we need to connect directly
    auto dev_list = Devices::get().getDevList();
    if (!dev_list.empty() && !m_connected) {
        m_device = dev_list.front();
        m_connected = true;

        m_cameraInfo.name = QString::fromStdString(m_device->devName());
        m_cameraInfo.serialNumber = QString::fromStdString(m_device->devSn());
        m_cameraInfo.version = QString::fromStdString(m_device->devVersion());
        m_cameraInfo.productType = m_device->productType();
        m_cameraInfo.connected = true;

        emit cameraConnected(m_cameraInfo);
        updateState();
    }
}

void CameraController::disconnectFromCamera()
{
    if (m_connected) {
        // Release our device handle - this allows other apps to access the camera
        m_device.reset();
        m_connected = false;
        m_cameraInfo.connected = false;

        emit cameraDisconnected();
    }
}

CameraController::CameraState CameraController::getCurrentState()
{
    if (m_connected && !isSettling()) {
        updateState();
    }
    // Return cached state during settling, actual state otherwise
    return isSettling() ? m_cachedState : m_currentState;
}

bool CameraController::hasTiny2Capabilities() const
{
    return isTiny2Family();
}

bool CameraController::enableAutoFraming(bool enabled)
{
    if (!m_connected) return false;

    if (enabled) {
        // Step 1: Set MediaMode to AutoFrame
        if (!executeCommand("Set MediaMode to AutoFrame", [this]() {
            return m_device->cameraSetMediaModeU(Device::MediaModeAutoFrame);
        })) {
            return false;
        }

        // Step 2: Set auto-framing mode after a brief delay (non-blocking)
        QTimer::singleShot(500, [this]() {
            executeCommand("Set AutoFraming mode", [this]() {
                return m_device->cameraSetAutoFramingModeU(Device::AutoFrmSingle, Device::AutoFrmUpperBody);
            });
        });

        m_currentState.autoFramingEnabled = true;
        emit stateChanged(m_currentState);
        return true;  // First command succeeded, second is pending
    } else {
        bool success = executeCommand("Disable AutoFraming", [this]() {
            return m_device->cameraSetMediaModeU(Device::MediaModeNormal);
        });
        if (success) {
            m_currentState.autoFramingEnabled = false;
            emit stateChanged(m_currentState);
        }
        return success;
    }
}

bool CameraController::setAiMode(int mode, int subMode)
{
    if (!m_connected) return false;

    auto workMode = static_cast<Device::AiWorkModeType>(mode);
    bool success = executeCommand("Set AI Mode", [this, workMode, subMode]() {
        return m_device->cameraSetAiModeU(workMode, subMode);
    });

    if (success) {
        m_currentState.aiMode = mode;
        m_currentState.aiSubMode = subMode;
        m_currentState.autoFramingEnabled = (mode != Device::AiWorkModeNone);
        emit stateChanged(m_currentState);
    }

    return success;
}

bool CameraController::setAutoZoom(bool enabled)
{
    if (!m_connected) return false;

    bool success = executeCommand(enabled ? "Enable Auto Zoom" : "Disable Auto Zoom", [this, enabled]() {
        return m_device->aiSetAiAutoZoomR(enabled);
    });

    if (success) {
        m_currentState.autoZoomEnabled = enabled;
        emit stateChanged(m_currentState);
    }

    return success;
}

bool CameraController::setTrackSpeed(int speedMode)
{
    if (!m_connected) return false;

    auto speed = static_cast<Device::AiTrackSpeedType>(speedMode);
    bool success = executeCommand("Set Tracking Speed", [this, speed]() {
        return m_device->aiSetTrackSpeedTypeR(speed);
    });

    if (success) {
        m_currentState.trackSpeedMode = speedMode;
        emit stateChanged(m_currentState);
    }

    return success;
}

bool CameraController::setAudioAutoGain(bool enabled)
{
    if (!m_connected) return false;

    bool success = executeCommand(enabled ? "Enable Audio Auto Gain" : "Disable Audio Auto Gain", [this, enabled]() {
        return m_device->cameraSetAudioAutoGainU(enabled);
    });

    if (success) {
        m_currentState.audioAutoGainEnabled = enabled;
        emit stateChanged(m_currentState);
    }

    return success;
}

bool CameraController::setPanTilt(double pan, double tilt)
{
    if (!m_connected) return false;

    // Clamp values
    pan = qBound(-1.0, pan, 1.0);
    tilt = qBound(-1.0, tilt, 1.0);

    bool success = executeCommand("Set Pan/Tilt", [this, pan, tilt]() {
        return m_device->cameraSetPanTiltAbsolute(pan, tilt);
    });

    if (success) {
        m_currentState.pan = pan;
        m_currentState.tilt = tilt;
        emit stateChanged(m_currentState);
    }

    return success;
}

bool CameraController::adjustPan(double delta)
{
    double newPan = m_currentState.pan + delta;
    return setPanTilt(newPan, m_currentState.tilt);
}

bool CameraController::adjustTilt(double delta)
{
    double newTilt = m_currentState.tilt + delta;
    return setPanTilt(m_currentState.pan, newTilt);
}

bool CameraController::setZoom(double zoom)
{
    if (!m_connected) return false;

    // Clamp to valid range (1.0 - 2.0)
    zoom = qBound(1.0, zoom, 2.0);

    bool success = executeCommand("Set Zoom", [this, zoom]() {
        return m_device->cameraSetZoomAbsoluteR(zoom);
    });

    if (success) {
        m_currentState.zoom = zoom;
        emit stateChanged(m_currentState);
    }

    return success;
}

bool CameraController::centerView()
{
    return setPanTilt(0.0, 0.0);
}

bool CameraController::setHDR(bool enabled)
{
    if (!m_connected) return false;

    return executeCommand(enabled ? "Enable HDR" : "Disable HDR", [this, enabled]() {
        return m_device->cameraSetWdrR(enabled ? Device::DevWdrModeDol2TO1 : Device::DevWdrModeNone);
    });
}

bool CameraController::setFOV(int fovMode)
{
    if (!m_connected) return false;

    Device::FovType fov;
    switch (fovMode) {
        case 0: fov = Device::FovType86; break;
        case 1: fov = Device::FovType78; break;
        case 2: fov = Device::FovType65; break;
        default: return false;
    }

    return executeCommand("Set FOV", [this, fov]() {
        return m_device->cameraSetFovU(fov);
    });
}

bool CameraController::setFaceAE(bool enabled)
{
    if (!m_connected) return false;

    return executeCommand(enabled ? "Enable Face AE" : "Disable Face AE", [this, enabled]() {
        return m_device->cameraSetFaceAER(enabled);
    });
}

bool CameraController::setFaceFocus(bool enabled)
{
    if (!m_connected) return false;

    return executeCommand(enabled ? "Enable Face Focus" : "Disable Face Focus", [this, enabled]() {
        return m_device->cameraSetFaceFocusR(enabled);
    });
}

bool CameraController::setBrightness(int value)
{
    if (!m_connected) return false;

    // Don't send command if in auto mode
    if (m_currentState.brightnessAuto) {
        return true;
    }

    return executeCommand("Set Brightness", [this, value]() {
        return m_device->cameraSetImageBrightnessR(value);
    });
}

bool CameraController::setContrast(int value)
{
    if (!m_connected) return false;

    // Don't send command if in auto mode
    if (m_currentState.contrastAuto) {
        return true;
    }

    return executeCommand("Set Contrast", [this, value]() {
        return m_device->cameraSetImageContrastR(value);
    });
}

bool CameraController::setSaturation(int value)
{
    if (!m_connected) return false;

    // Don't send command if in auto mode
    if (m_currentState.saturationAuto) {
        return true;
    }

    return executeCommand("Set Saturation", [this, value]() {
        return m_device->cameraSetImageSaturationR(value);
    });
}

bool CameraController::setWhiteBalance(int mode)
{
    if (!m_connected) return false;

    return executeCommand("Set White Balance", [this, mode]() {
        // Map our mode to SDK enum
        Device::DevWhiteBalanceType wbType = static_cast<Device::DevWhiteBalanceType>(mode);
        return m_device->cameraSetWhiteBalanceR(wbType, 0);  // param=0 for preset modes
    });
}

bool CameraController::executeCommand(const QString &description, std::function<int32_t()> command)
{
    int32_t ret = command();
    if (ret != 0) {
        emit commandFailed(description, ret);
        return false;
    }
    return true;
}

void CameraController::updateState()
{
    if (!m_connected) return;

    // Don't update from camera during settling period
    if (isSettling()) {
        return;
    }

    auto status = m_device->cameraStatus();

    m_currentState.aiMode = status.tiny.ai_mode;
    m_currentState.aiSubMode = status.tiny.ai_sub_mode;
    m_currentState.zoomRatio = status.tiny.zoom_ratio;
    m_currentState.hdrEnabled = status.tiny.hdr;
    m_currentState.faceAEEnabled = status.tiny.face_ae;
    m_currentState.faceFocusEnabled = status.tiny.face_auto_focus;
    m_currentState.autoFocusEnabled = status.tiny.auto_focus;
    m_currentState.fovMode = status.tiny.fov;
    m_currentState.devStatus = status.tiny.dev_status;
    m_currentState.autoFramingEnabled = (m_currentState.aiMode != Device::AiWorkModeNone);
    m_currentState.trackSpeedMode = status.tiny.ai_tracker_speed;
    m_currentState.audioAutoGainEnabled = status.tiny.audio_auto_gain;

    // Image controls - read current values from camera
    // Note: Preserve auto mode flags - camera doesn't have concept of "auto" for these
    bool preservedBrightnessAuto = m_currentState.brightnessAuto;
    bool preservedContrastAuto = m_currentState.contrastAuto;
    bool preservedSaturationAuto = m_currentState.saturationAuto;

    int32_t brightness, contrast, saturation;
    Device::DevWhiteBalanceType wbType;
    int32_t wbParam;

    if (m_device->cameraGetImageBrightnessR(brightness) == 0) {
        m_currentState.brightness = brightness;
    }
    if (m_device->cameraGetImageContrastR(contrast) == 0) {
        m_currentState.contrast = contrast;
    }
    if (m_device->cameraGetImageSaturationR(saturation) == 0) {
        m_currentState.saturation = saturation;
    }
    if (m_device->cameraGetWhiteBalanceR(wbType, wbParam) == 0) {
        m_currentState.whiteBalance = static_cast<int>(wbType);
    }

    // Restore auto mode flags (not stored in camera)
    m_currentState.brightnessAuto = preservedBrightnessAuto;
    m_currentState.contrastAuto = preservedContrastAuto;
    m_currentState.saturationAuto = preservedSaturationAuto;

    emit stateChanged(m_currentState);
}

void CameraController::beginSettling(int durationMs)
{
    // Cache the current intended state
    m_cachedState = m_currentState;
    m_settlingTimer->start(durationMs);
}

bool CameraController::loadConfig(std::vector<Config::ValidationError> &errors)
{
    return m_config.load(errors);
}

bool CameraController::saveConfig()
{
    // Update config with current camera state before saving
    saveCurrentStateToConfig();
    return m_config.save();
}

void CameraController::applyConfigToCamera()
{
    if (!m_connected) return;

    auto settings = m_config.getSettings();

    // Initialize auto mode flags from config
    m_currentState.brightnessAuto = settings.brightnessAuto;
    m_currentState.contrastAuto = settings.contrastAuto;
    m_currentState.saturationAuto = settings.saturationAuto;

    // Apply all settings to the camera
    enableAutoFraming(settings.faceTracking);
    setHDR(settings.hdr);
    setFOV(settings.fov);
    setFaceAE(settings.faceAE);
    setFaceFocus(settings.faceFocus);
    setZoom(settings.zoom);
    setPanTilt(settings.pan, settings.tilt);

    if (isTiny2Family()) {
        setAiMode(settings.aiMode, settings.aiSubMode);
        setAutoZoom(settings.autoZoom);
        setTrackSpeed(settings.trackSpeed);
        setAudioAutoGain(settings.audioAutoGain);
    }

    // Image controls
    setBrightness(settings.brightness);
    setContrast(settings.contrast);
    setSaturation(settings.saturation);
    setWhiteBalance(settings.whiteBalance);

    emit configLoaded();
}

void CameraController::applyCurrentStateToCamera(const CameraState &uiState)
{
    if (!m_connected) return;

    // Update current state with UI state (including auto mode flags)
    m_currentState.brightnessAuto = uiState.brightnessAuto;
    m_currentState.contrastAuto = uiState.contrastAuto;
    m_currentState.saturationAuto = uiState.saturationAuto;

    // Cache the intended state
    m_cachedState = uiState;

    // Begin settling period - block status updates for 2 seconds
    beginSettling(2000);

    // Apply the current UI state to camera (respects user changes)
    enableAutoFraming(uiState.autoFramingEnabled);
    if (isTiny2Family()) {
        setAiMode(uiState.aiMode, uiState.aiSubMode);
        setAutoZoom(uiState.autoZoomEnabled);
        setTrackSpeed(uiState.trackSpeedMode);
        setAudioAutoGain(uiState.audioAutoGainEnabled);
    }
    setHDR(uiState.hdrEnabled);
    setFOV(uiState.fovMode);
    setFaceAE(uiState.faceAEEnabled);
    setFaceFocus(uiState.faceFocusEnabled);
    setZoom(uiState.zoom);
    setPanTilt(uiState.pan, uiState.tilt);

    // Image controls
    setBrightness(uiState.brightness);
    setContrast(uiState.contrast);
    setSaturation(uiState.saturation);
    setWhiteBalance(uiState.whiteBalance);
}

void CameraController::saveCurrentStateToConfig()
{
    // Get current settings to preserve app settings (like startMinimized)
    Config::CameraSettings settings = m_config.getSettings();

    // Update only camera-related settings from current state
    settings.faceTracking = m_currentState.autoFramingEnabled;
    settings.hdr = m_currentState.hdrEnabled;
    settings.fov = m_currentState.fovMode;
    settings.faceAE = m_currentState.faceAEEnabled;
    settings.faceFocus = m_currentState.faceFocusEnabled;
    settings.zoom = m_currentState.zoom;
    settings.pan = m_currentState.pan;
    settings.tilt = m_currentState.tilt;
    settings.aiMode = m_currentState.aiMode;
    settings.aiSubMode = m_currentState.aiSubMode;
    settings.autoZoom = m_currentState.autoZoomEnabled;
    settings.trackSpeed = m_currentState.trackSpeedMode;
    settings.audioAutoGain = m_currentState.audioAutoGainEnabled;

    // Image controls
    settings.brightnessAuto = m_currentState.brightnessAuto;
    settings.brightness = m_currentState.brightness;
    settings.contrastAuto = m_currentState.contrastAuto;
    settings.contrast = m_currentState.contrast;
    settings.saturationAuto = m_currentState.saturationAuto;
    settings.saturation = m_currentState.saturation;
    settings.whiteBalance = m_currentState.whiteBalance;

    m_config.setSettings(settings);
}

bool CameraController::isTiny2Family() const
{
    return m_cameraInfo.productType == ObsbotProdTiny2 ||
           m_cameraInfo.productType == ObsbotProdTiny2Lite ||
           m_cameraInfo.productType == ObsbotProdTinySE;
}
