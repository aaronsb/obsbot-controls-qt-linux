#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QObject>
#include <QTimer>
#include <memory>
#include <functional>
#include <dev/devs.hpp>
#include "Config.h"

/**
 * @brief Handles all camera communication and state management
 *
 * This class encapsulates the OBSBOT SDK and provides a clean Qt-friendly
 * interface for controlling the camera.
 */
class CameraController : public QObject
{
    Q_OBJECT

public:
    struct CameraInfo {
        QString name;
        QString serialNumber;
        QString version;
        int productType;
        bool connected;
    };

    struct CameraState {
        // Tracking
        bool autoFramingEnabled;
        int aiMode;
        int aiSubMode;
        bool autoZoomEnabled;
        int trackSpeedMode;
        bool audioAutoGainEnabled;

        // PTZ
        double pan;
        double tilt;
        double zoom;

        // Image settings
        bool hdrEnabled;
        int fovMode;
        bool faceAEEnabled;
        bool faceFocusEnabled;
        bool autoFocusEnabled;

        // Image controls
        bool brightnessAuto; // Auto mode for brightness
        int brightness;      // 0-255
        bool contrastAuto;   // Auto mode for contrast
        int contrast;        // 0-255
        bool saturationAuto; // Auto mode for saturation
        int saturation;      // 0-255
        int whiteBalance;    // 0=Auto, 1=Daylight, etc.

        // Status
        int zoomRatio;
        int devStatus;
    };

    explicit CameraController(QObject *parent = nullptr);
    ~CameraController();

    // Connection
    bool isConnected() const { return m_connected; }
    CameraInfo getCameraInfo() const { return m_cameraInfo; }
    void connectToCamera();
    void disconnectFromCamera();

    // State
    CameraState getCurrentState();
    bool hasTiny2Capabilities() const;

    // Tracking controls
    bool enableAutoFraming(bool enabled);
    bool setAiMode(int mode, int subMode);
    bool setAutoZoom(bool enabled);
    bool setTrackSpeed(int speedMode);
    bool setAudioAutoGain(bool enabled);

    // PTZ controls
    bool setPanTilt(double pan, double tilt);
    bool adjustPan(double delta);
    bool adjustTilt(double delta);
    bool setZoom(double zoom);
    bool centerView();

    // Camera settings
    bool setHDR(bool enabled);
    bool setFOV(int fovMode);  // 0=Wide, 1=Medium, 2=Narrow
    bool setFaceAE(bool enabled);
    bool setFaceFocus(bool enabled);

    // Image controls
    void setBrightnessAuto(bool enabled) { m_currentState.brightnessAuto = enabled; }
    bool setBrightness(int value);  // 0-255
    void setContrastAuto(bool enabled) { m_currentState.contrastAuto = enabled; }
    bool setContrast(int value);    // 0-255
    void setSaturationAuto(bool enabled) { m_currentState.saturationAuto = enabled; }
    bool setSaturation(int value);  // 0-255
    bool setWhiteBalance(int mode); // 0=Auto, 1=Daylight, etc.

    // Configuration
    bool loadConfig(std::vector<Config::ValidationError> &errors);
    bool saveConfig();
    void applyConfigToCamera();  // Apply loaded config settings to camera
    void applyCurrentStateToCamera(const CameraState &uiState);  // Apply UI state to camera
    Config& getConfig() { return m_config; }

    // Settling state
    bool isSettling() const { return m_settlingTimer && m_settlingTimer->isActive(); }
    void beginSettling(int durationMs = 2000);  // Start settling period

signals:
    void cameraConnected(const CameraInfo &info);
    void cameraDisconnected();
    void stateChanged(const CameraState &state);
    void commandFailed(const QString &description, int errorCode);
    void configLoaded();  // Emitted after config is successfully loaded

private:
    std::shared_ptr<Device> m_device;
    bool m_connected;
    CameraInfo m_cameraInfo;
    CameraState m_currentState;
    CameraState m_cachedState;  // Cache intended state during settling
    Config m_config;
    QTimer *m_settlingTimer;  // Timer for settling period after config apply
    bool isTiny2Family() const;

    // Helper
    bool executeCommand(const QString &description, std::function<int32_t()> command);
    void updateState();
    void saveCurrentStateToConfig();  // Update config with current camera state
};

#endif // CAMERACONTROLLER_H
