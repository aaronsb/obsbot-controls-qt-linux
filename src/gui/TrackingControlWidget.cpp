#include "TrackingControlWidget.h"
#include <QLabel>
#include <dev/dev.hpp>

TrackingControlWidget::TrackingControlWidget(CameraController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_userInitiated(false)
{
    // Create debounce timer for command completion
    m_commandTimer = new QTimer(this);
    m_commandTimer->setSingleShot(true);
    m_tiny2Capabilities = m_controller->hasTiny2Capabilities();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *groupBox = new QGroupBox("Face Tracking", this);
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

    QLabel *descLabel = new QLabel(
        "Enable automatic face tracking to keep you centered in the frame.", this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 11px;");
    groupLayout->addWidget(descLabel);

    m_trackingCheckBox = new QCheckBox("Enable Auto-Framing", this);
    m_trackingCheckBox->setStyleSheet("font-weight: bold; font-size: 14px;");
    connect(m_trackingCheckBox, &QCheckBox::toggled, this, &TrackingControlWidget::onTrackingToggled);
    groupLayout->addWidget(m_trackingCheckBox);

    // Advanced controls container (Tiny 2 family)
    m_advancedContainer = new QWidget(this);
    QVBoxLayout *advancedLayout = new QVBoxLayout(m_advancedContainer);
    advancedLayout->setContentsMargins(0, 8, 0, 0);
    advancedLayout->setSpacing(6);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    QLabel *modeLabel = new QLabel("Tracking Mode:", this);
    modeLabel->setStyleSheet("font-size: 11px;");
    m_modeCombo = new QComboBox(this);
    m_modeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_modeCombo->addItem("Off", Device::AiWorkModeNone);
    m_modeCombo->addItem("Group", Device::AiWorkModeGroup);
    m_modeCombo->addItem("Human (Auto)", Device::AiWorkModeHuman);
    m_modeCombo->addItem("Hand Tracking", Device::AiWorkModeHand);
    m_modeCombo->addItem("Whiteboard", Device::AiWorkModeWhiteBoard);
    m_modeCombo->addItem("Desk", Device::AiWorkModeDesk);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackingControlWidget::onModeChanged);
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_modeCombo, 1);
    advancedLayout->addLayout(modeLayout);

    QHBoxLayout *subModeLayout = new QHBoxLayout();
    QLabel *subModeLabel = new QLabel("Human Sub-Mode:", this);
    subModeLabel->setStyleSheet("font-size: 11px;");
    m_humanSubModeCombo = new QComboBox(this);
    m_humanSubModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_humanSubModeCombo->addItem("Normal", Device::AiSubModeNormal);
    m_humanSubModeCombo->addItem("Upper Body", Device::AiSubModeUpperBody);
    m_humanSubModeCombo->addItem("Close Up", Device::AiSubModeCloseUp);
    m_humanSubModeCombo->addItem("Headless", Device::AiSubModeHeadHide);
    m_humanSubModeCombo->addItem("Lower Body", Device::AiSubModeLowerBody);
    connect(m_humanSubModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackingControlWidget::onHumanSubModeChanged);
    subModeLayout->addWidget(subModeLabel);
    subModeLayout->addWidget(m_humanSubModeCombo, 1);
    advancedLayout->addLayout(subModeLayout);

    m_autoZoomCheckBox = new QCheckBox("Enable AI Auto Zoom", this);
    connect(m_autoZoomCheckBox, &QCheckBox::toggled, this, &TrackingControlWidget::onAutoZoomToggled);
    m_autoZoomCheckBox->setStyleSheet("font-size: 11px;");
    advancedLayout->addWidget(m_autoZoomCheckBox);

    QHBoxLayout *speedLayout = new QHBoxLayout();
    QLabel *speedLabel = new QLabel("Tracking Speed:", this);
    speedLabel->setStyleSheet("font-size: 11px;");
    m_speedCombo = new QComboBox(this);
    m_speedCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_speedCombo->addItem("Lazy", Device::AiTrackSpeedLazy);
    m_speedCombo->addItem("Slow", Device::AiTrackSpeedSlow);
    m_speedCombo->addItem("Standard", Device::AiTrackSpeedStandard);
    m_speedCombo->addItem("Fast", Device::AiTrackSpeedFast);
    m_speedCombo->addItem("Crazy", Device::AiTrackSpeedCrazy);
    m_speedCombo->addItem("Auto", Device::AiTrackSpeedAuto);
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackingControlWidget::onSpeedChanged);
    speedLayout->addWidget(speedLabel);
    speedLayout->addWidget(m_speedCombo, 1);
    advancedLayout->addLayout(speedLayout);

    m_audioGainCheckBox = new QCheckBox("Enable Audio Auto Gain", this);
    m_audioGainCheckBox->setStyleSheet("font-size: 11px;");
    connect(m_audioGainCheckBox, &QCheckBox::toggled, this, &TrackingControlWidget::onAudioGainToggled);
    advancedLayout->addWidget(m_audioGainCheckBox);

    groupLayout->addWidget(m_advancedContainer);
    updateTiny2Visibility();

    layout->addWidget(groupBox);
}

void TrackingControlWidget::setAiMode(int mode)
{
    int idx = m_modeCombo->findData(mode);
    if (idx >= 0) {
        QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(idx);
    }
    updateTiny2Visibility();
}

void TrackingControlWidget::setHumanSubMode(int subMode)
{
    int idx = m_humanSubModeCombo->findData(subMode);
    if (idx >= 0) {
        QSignalBlocker blocker(m_humanSubModeCombo);
        m_humanSubModeCombo->setCurrentIndex(idx);
    }
}

void TrackingControlWidget::setAutoZoomEnabled(bool enabled)
{
    QSignalBlocker blocker(m_autoZoomCheckBox);
    m_autoZoomCheckBox->setChecked(enabled);
}

void TrackingControlWidget::setTrackSpeed(int speedMode)
{
    int idx = m_speedCombo->findData(speedMode);
    if (idx >= 0) {
        QSignalBlocker blocker(m_speedCombo);
        m_speedCombo->setCurrentIndex(idx);
    }
}

void TrackingControlWidget::setAudioAutoGain(bool enabled)
{
    QSignalBlocker blocker(m_audioGainCheckBox);
    m_audioGainCheckBox->setChecked(enabled);
}

void TrackingControlWidget::onTrackingToggled(bool checked)
{
    m_userInitiated = true;

    if (m_tiny2Capabilities) {
        int modeValue = m_modeCombo->currentData().toInt();
        if (checked && modeValue == Device::AiWorkModeNone) {
            int humanIndex = m_modeCombo->findData(Device::AiWorkModeHuman);
            if (humanIndex >= 0) {
                m_modeCombo->blockSignals(true);
                m_modeCombo->setCurrentIndex(humanIndex);
                m_modeCombo->blockSignals(false);
                modeValue = Device::AiWorkModeHuman;
            }
        }

        int subMode = m_humanSubModeCombo->currentData().toInt();
        if (modeValue != Device::AiWorkModeHuman) {
            subMode = 0;
        }

        m_controller->setAiMode(checked ? modeValue : Device::AiWorkModeNone, subMode);
    }

    m_controller->enableAutoFraming(checked);

    m_commandTimer->start(1000);
}

void TrackingControlWidget::updateFromState(const CameraController::CameraState &state)
{
    // Only update if:
    // 1. State differs from what's shown AND
    // 2. Not during user action AND
    // 3. Command completion timer has expired AND
    // 4. Camera is not settling
    bool shouldBeChecked = (state.aiMode != Device::AiWorkModeNone);
    bool commandInFlight = m_commandTimer->isActive();
    bool isSettling = m_controller->isSettling();

    if (m_trackingCheckBox->isChecked() != shouldBeChecked && !m_userInitiated && !commandInFlight && !isSettling) {
        m_trackingCheckBox->blockSignals(true);
        m_trackingCheckBox->setChecked(shouldBeChecked);
        m_trackingCheckBox->blockSignals(false);
    }

    bool tiny2 = m_controller->hasTiny2Capabilities();
    if (tiny2 != m_tiny2Capabilities) {
        m_tiny2Capabilities = tiny2;
        updateTiny2Visibility();
    }

    if (m_tiny2Capabilities && !m_userInitiated && !commandInFlight && !isSettling) {
        int modeIdx = m_modeCombo->findData(state.aiMode);
        if (modeIdx >= 0 && m_modeCombo->currentIndex() != modeIdx) {
            m_modeCombo->blockSignals(true);
            m_modeCombo->setCurrentIndex(modeIdx);
            m_modeCombo->blockSignals(false);
        }

        updateTiny2Visibility();

        if (state.aiMode == Device::AiWorkModeHuman) {
            int subIdx = m_humanSubModeCombo->findData(state.aiSubMode);
            if (subIdx >= 0 && m_humanSubModeCombo->currentIndex() != subIdx) {
                m_humanSubModeCombo->blockSignals(true);
                m_humanSubModeCombo->setCurrentIndex(subIdx);
                m_humanSubModeCombo->blockSignals(false);
            }
        }

        if (m_autoZoomCheckBox->isChecked() != state.autoZoomEnabled) {
            m_autoZoomCheckBox->blockSignals(true);
            m_autoZoomCheckBox->setChecked(state.autoZoomEnabled);
            m_autoZoomCheckBox->blockSignals(false);
        }

        int speedIdx = m_speedCombo->findData(state.trackSpeedMode);
        if (speedIdx >= 0 && m_speedCombo->currentIndex() != speedIdx) {
            m_speedCombo->blockSignals(true);
            m_speedCombo->setCurrentIndex(speedIdx);
            m_speedCombo->blockSignals(false);
        }

        if (m_audioGainCheckBox->isChecked() != state.audioAutoGainEnabled) {
            m_audioGainCheckBox->blockSignals(true);
            m_audioGainCheckBox->setChecked(state.audioAutoGainEnabled);
            m_audioGainCheckBox->blockSignals(false);
        }
    }

    // If command completed and timer expired, we can now accept state updates
    if (!commandInFlight && m_userInitiated) {
        m_userInitiated = false;
    }
}

void TrackingControlWidget::onModeChanged(int index)
{
    Q_UNUSED(index);
    if (!m_tiny2Capabilities) return;

    int modeValue = m_modeCombo->currentData().toInt();
    m_humanSubModeCombo->setEnabled(modeValue == Device::AiWorkModeHuman);

    m_userInitiated = true;
    int subMode = m_humanSubModeCombo->currentData().toInt();
    if (modeValue != Device::AiWorkModeHuman) {
        subMode = 0;
    }

    m_controller->setAiMode(modeValue, subMode);

    bool shouldCheck = modeValue != Device::AiWorkModeNone;
    if (m_trackingCheckBox->isChecked() != shouldCheck) {
        m_trackingCheckBox->blockSignals(true);
        m_trackingCheckBox->setChecked(shouldCheck);
        m_trackingCheckBox->blockSignals(false);
    }

    m_commandTimer->start(1000);
}

void TrackingControlWidget::onHumanSubModeChanged(int index)
{
    Q_UNUSED(index);
    if (!m_tiny2Capabilities) return;

    if (m_modeCombo->currentData().toInt() != Device::AiWorkModeHuman) {
        return;
    }

    m_userInitiated = true;
    m_controller->setAiMode(Device::AiWorkModeHuman, m_humanSubModeCombo->currentData().toInt());
    m_commandTimer->start(1000);
}

void TrackingControlWidget::onAutoZoomToggled(bool checked)
{
    if (!m_tiny2Capabilities) return;
    m_userInitiated = true;
    m_controller->setAutoZoom(checked);
    m_commandTimer->start(1000);
}

void TrackingControlWidget::onSpeedChanged(int index)
{
    Q_UNUSED(index);
    if (!m_tiny2Capabilities) return;
    m_userInitiated = true;
    m_controller->setTrackSpeed(m_speedCombo->currentData().toInt());
    m_commandTimer->start(1000);
}

void TrackingControlWidget::onAudioGainToggled(bool checked)
{
    if (!m_tiny2Capabilities) return;
    m_userInitiated = true;
    m_controller->setAudioAutoGain(checked);
    m_commandTimer->start(1000);
}

void TrackingControlWidget::updateTiny2Visibility()
{
    if (!m_advancedContainer) {
        return;
    }

    m_advancedContainer->setVisible(m_tiny2Capabilities);
    m_humanSubModeCombo->setEnabled(m_tiny2Capabilities && m_modeCombo->currentData().toInt() == Device::AiWorkModeHuman);
}
