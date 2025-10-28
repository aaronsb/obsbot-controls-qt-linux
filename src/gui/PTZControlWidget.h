#ifndef PTZCONTROLWIDGET_H
#define PTZCONTROLWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <array>
#include "CameraController.h"

/**
 * @brief Widget for manual Pan/Tilt/Zoom control
 * Shown in: Advanced, Expert modes
 */
class PTZControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PTZControlWidget(CameraController *controller, QWidget *parent = nullptr);

    void updateFromState(const CameraController::CameraState &state);
    struct PresetState {
        bool defined;
        double pan;
        double tilt;
        double zoom;
    };
    void applyPresetStates(const std::array<PresetState, 3> &presets);
    std::array<PresetState, 3> currentPresets() const;

signals:
    void presetUpdated(int index, double pan, double tilt, double zoom, bool defined);

private slots:
    void onPanLeftClicked();
    void onPanRightClicked();
    void onTiltUpClicked();
    void onTiltDownClicked();
    void onCenterClicked();
    void onZoomChanged(int value);
    void onRecallPreset();
    void onStorePreset();

private:
    CameraController *m_controller;

    // PTZ buttons
    QPushButton *m_panLeftBtn;
    QPushButton *m_panRightBtn;
    QPushButton *m_tiltUpBtn;
    QPushButton *m_tiltDownBtn;
    QPushButton *m_centerBtn;

    // Zoom
    QSlider *m_zoomSlider;
    QLabel *m_zoomLabel;
    QLabel *m_positionLabel;

    struct PresetUi {
        QPushButton *recallButton;
        QPushButton *saveButton;
        QLabel *statusLabel;
        bool defined;
        double pan;
        double tilt;
        double zoom;
    };

    std::array<PresetUi, 3> m_presets;

    void updatePresetLabel(int index);
};

#endif // PTZCONTROLWIDGET_H
