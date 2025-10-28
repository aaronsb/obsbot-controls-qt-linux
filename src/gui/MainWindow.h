#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QSystemTrayIcon>
#include <QMenu>
#include "CameraController.h"
#include "TrackingControlWidget.h"
#include "PTZControlWidget.h"
#include "CameraSettingsWidget.h"
#include "CameraPreviewWidget.h"

/**
 * @brief Main application window
 *
 * Shows all camera controls with optional collapsible preview drawer
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCameraConnected(const CameraController::CameraInfo &info);
    void onCameraDisconnected();
    void onStateChanged(const CameraController::CameraState &state);
    void onCommandFailed(const QString &description, int errorCode);
    void updateStatus();
    void onStateChangedSaveConfig();
    void onTogglePreview(bool enabled);
    void onPreviewAspectRatioChanged(double ratio);
    void onPreviewStarted();
    void onPreviewFailed(const QString &error);
    void onPreviewFormatChanged(const QString &formatId);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowHideAction();
    void onQuitAction();
    void onStartMinimizedToggled(bool checked);

private:
    void setupUI();
    void setupTrayIcon();
    void loadConfiguration();
    void handleConfigErrors(const std::vector<Config::ValidationError> &errors);
    CameraController::CameraState getUIState() const;  // Get current UI state
    QString getProcessUsingCamera(const QString &devicePath);  // Detect which process is using camera
    QString findObsbotVideoDevice();  // Find which /dev/video* device is the OBSBOT camera

    // Controller
    CameraController *m_controller;

    // UI
    QPushButton *m_previewToggleButton;
    QLabel *m_deviceInfoLabel;
    QLabel *m_cameraWarningLabel;  // Warning for camera in use
    QLabel *m_statusLabel;
    QCheckBox *m_startMinimizedCheckbox;
    QWidget *m_sidebar;
    QHBoxLayout *m_mainLayout;

    // Control widgets
    TrackingControlWidget *m_trackingWidget;
    PTZControlWidget *m_ptzWidget;
    CameraSettingsWidget *m_settingsWidget;
    CameraPreviewWidget *m_previewWidget;

    // Status timer
    QTimer *m_statusTimer;

    // Preview aspect ratio (width/height)
    double m_previewAspectRatio;

    // Track preview state before minimize
    bool m_previewStateBeforeMinimize;

    // System tray
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
