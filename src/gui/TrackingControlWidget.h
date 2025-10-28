#ifndef TRACKINGCONTROLWIDGET_H
#define TRACKINGCONTROLWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTimer>
#include <QSignalBlocker>
#include "CameraController.h"

/**
 * @brief Simple widget for enabling/disabling face tracking
 * Shown in: Simple, Advanced, Expert modes
 */
class TrackingControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrackingControlWidget(CameraController *controller, QWidget *parent = nullptr);

    void updateFromState(const CameraController::CameraState &state);
    bool isTrackingEnabled() const { return m_trackingCheckBox->isChecked(); }
    void setTrackingEnabled(bool enabled) {
        m_trackingCheckBox->blockSignals(true);
        m_trackingCheckBox->setChecked(enabled);
        m_trackingCheckBox->blockSignals(false);
    }
    void setAiMode(int mode);
    void setHumanSubMode(int subMode);
    void setAutoZoomEnabled(bool enabled);
    void setTrackSpeed(int speedMode);
    void setAudioAutoGain(bool enabled);
    int currentAiMode() const { return m_modeCombo->currentData().toInt(); }
    int currentHumanSubMode() const { return m_humanSubModeCombo->currentData().toInt(); }
    bool isAutoZoomEnabled() const { return m_autoZoomCheckBox->isChecked(); }
    int currentTrackSpeed() const { return m_speedCombo->currentData().toInt(); }
    bool isAudioAutoGainEnabled() const { return m_audioGainCheckBox->isChecked(); }

private slots:
    void onTrackingToggled(bool checked);
    void onModeChanged(int index);
    void onHumanSubModeChanged(int index);
    void onAutoZoomToggled(bool checked);
    void onSpeedChanged(int index);
    void onAudioGainToggled(bool checked);

private:
    CameraController *m_controller;
    QCheckBox *m_trackingCheckBox;
    QComboBox *m_modeCombo;
    QComboBox *m_humanSubModeCombo;
    QCheckBox *m_autoZoomCheckBox;
    QComboBox *m_speedCombo;
    QCheckBox *m_audioGainCheckBox;
    QWidget *m_advancedContainer;
    bool m_userInitiated;  // Track if change was user-initiated
    QTimer *m_commandTimer;  // Debounce timer for command completion
    bool m_tiny2Capabilities; // flag for advanced tracking features

    void updateTiny2Visibility();
};

#endif // TRACKINGCONTROLWIDGET_H
