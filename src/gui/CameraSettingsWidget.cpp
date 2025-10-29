#include "CameraSettingsWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

CameraSettingsWidget::CameraSettingsWidget(CameraController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_userInitiated(false)
{
    // Create debounce timer for command completion
    m_commandTimer = new QTimer(this);
    m_commandTimer->setSingleShot(true);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 14, 8, 14);
    layout->setSpacing(14);

    QGroupBox *groupBox = new QGroupBox("Advanced Camera Settings", this);
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);
    groupLayout->setContentsMargins(16, 16, 16, 16);
    groupLayout->setSpacing(10);

    // HDR
    m_hdrCheckBox = new QCheckBox("HDR (High Dynamic Range)", this);
    m_hdrCheckBox->setToolTip("Enhance image quality in high-contrast scenes");
    connect(m_hdrCheckBox, &QCheckBox::toggled, this, &CameraSettingsWidget::onHDRToggled);
    groupLayout->addWidget(m_hdrCheckBox);

    // FOV
    QHBoxLayout *fovLayout = new QHBoxLayout();
    fovLayout->addWidget(new QLabel("Field of View:", this));
    m_fovComboBox = new QComboBox(this);
    m_fovComboBox->addItem("Wide (86°)", 0);
    m_fovComboBox->addItem("Medium (78°)", 1);
    m_fovComboBox->addItem("Narrow (65°)", 2);
    m_fovComboBox->setToolTip("Change the camera's field of view");
    connect(m_fovComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraSettingsWidget::onFOVChanged);
    fovLayout->addWidget(m_fovComboBox);
    fovLayout->addStretch();
    groupLayout->addLayout(fovLayout);

    // Face AE
    m_faceAECheckBox = new QCheckBox("Face-based Auto Exposure", this);
    m_faceAECheckBox->setToolTip("Optimize exposure for faces");
    connect(m_faceAECheckBox, &QCheckBox::toggled, this, &CameraSettingsWidget::onFaceAEToggled);
    groupLayout->addWidget(m_faceAECheckBox);

    // Face Focus
    m_faceFocusCheckBox = new QCheckBox("Face-based Auto Focus", this);
    m_faceFocusCheckBox->setToolTip("Optimize focus for faces");
    connect(m_faceFocusCheckBox, &QCheckBox::toggled, this, &CameraSettingsWidget::onFaceFocusToggled);
    groupLayout->addWidget(m_faceFocusCheckBox);

    layout->addWidget(groupBox);

    // Image Controls Group
    QGroupBox *imageGroupBox = new QGroupBox("Image Controls", this);
    QVBoxLayout *imageLayout = new QVBoxLayout(imageGroupBox);
    imageLayout->setContentsMargins(16, 16, 16, 16);
    imageLayout->setSpacing(10);

    // Brightness
    QHBoxLayout *brightnessLayout = new QHBoxLayout();
    m_brightnessAutoCheckBox = new QCheckBox("Auto", this);
    m_brightnessAutoCheckBox->setToolTip("Enable automatic brightness (disables manual control)");
    connect(m_brightnessAutoCheckBox, &QCheckBox::toggled, this, &CameraSettingsWidget::onBrightnessAutoToggled);
    brightnessLayout->addWidget(m_brightnessAutoCheckBox);
    brightnessLayout->addWidget(new QLabel("Brightness:", this));
    m_brightnessSlider = new QSlider(Qt::Horizontal, this);
    m_brightnessSlider->setRange(0, 255);
    m_brightnessSlider->setToolTip("Adjust image brightness (0-255)");
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &CameraSettingsWidget::onBrightnessChanged);
    brightnessLayout->addWidget(m_brightnessSlider);
    imageLayout->addLayout(brightnessLayout);

    // Contrast
    QHBoxLayout *contrastLayout = new QHBoxLayout();
    m_contrastAutoCheckBox = new QCheckBox("Auto", this);
    m_contrastAutoCheckBox->setToolTip("Enable automatic contrast (disables manual control)");
    connect(m_contrastAutoCheckBox, &QCheckBox::toggled, this, &CameraSettingsWidget::onContrastAutoToggled);
    contrastLayout->addWidget(m_contrastAutoCheckBox);
    contrastLayout->addWidget(new QLabel("Contrast:", this));
    m_contrastSlider = new QSlider(Qt::Horizontal, this);
    m_contrastSlider->setRange(0, 255);
    m_contrastSlider->setToolTip("Adjust image contrast (0-255)");
    connect(m_contrastSlider, &QSlider::valueChanged, this, &CameraSettingsWidget::onContrastChanged);
    contrastLayout->addWidget(m_contrastSlider);
    imageLayout->addLayout(contrastLayout);

    // Saturation
    QHBoxLayout *saturationLayout = new QHBoxLayout();
    m_saturationAutoCheckBox = new QCheckBox("Auto", this);
    m_saturationAutoCheckBox->setToolTip("Enable automatic saturation (disables manual control)");
    connect(m_saturationAutoCheckBox, &QCheckBox::toggled, this, &CameraSettingsWidget::onSaturationAutoToggled);
    saturationLayout->addWidget(m_saturationAutoCheckBox);
    saturationLayout->addWidget(new QLabel("Saturation:", this));
    m_saturationSlider = new QSlider(Qt::Horizontal, this);
    m_saturationSlider->setRange(0, 255);
    m_saturationSlider->setToolTip("Adjust color saturation (0-255)");
    connect(m_saturationSlider, &QSlider::valueChanged, this, &CameraSettingsWidget::onSaturationChanged);
    saturationLayout->addWidget(m_saturationSlider);
    imageLayout->addLayout(saturationLayout);

    // White Balance
    QHBoxLayout *wbLayout = new QHBoxLayout();
    wbLayout->addWidget(new QLabel("White Balance:", this));
    m_whiteBalanceComboBox = new QComboBox(this);
    m_whiteBalanceComboBox->addItem("Auto", 0);
    m_whiteBalanceComboBox->addItem("Daylight", 1);
    m_whiteBalanceComboBox->addItem("Fluorescent", 2);
    m_whiteBalanceComboBox->addItem("Tungsten", 3);
    m_whiteBalanceComboBox->addItem("Flash", 4);
    m_whiteBalanceComboBox->addItem("Fine", 5);
    m_whiteBalanceComboBox->addItem("Cloudy", 6);
    m_whiteBalanceComboBox->addItem("Shade", 7);
    m_whiteBalanceComboBox->setToolTip("Adjust white balance for lighting conditions");
    connect(m_whiteBalanceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraSettingsWidget::onWhiteBalanceChanged);
    wbLayout->addWidget(m_whiteBalanceComboBox);
    wbLayout->addStretch();
    imageLayout->addLayout(wbLayout);

    layout->addWidget(imageGroupBox);
    layout->addStretch();
}

void CameraSettingsWidget::onHDRToggled(bool checked)
{
    m_userInitiated = true;
    m_controller->setHDR(checked);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onFOVChanged(int index)
{
    m_userInitiated = true;
    m_controller->setFOV(index);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onFaceAEToggled(bool checked)
{
    m_userInitiated = true;
    m_controller->setFaceAE(checked);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onFaceFocusToggled(bool checked)
{
    m_userInitiated = true;
    m_controller->setFaceFocus(checked);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onBrightnessAutoToggled(bool checked)
{
    m_userInitiated = true;
    m_controller->setBrightnessAuto(checked);
    m_brightnessSlider->setEnabled(!checked);
    // When switching to manual, send current slider value
    if (!checked) {
        m_controller->setBrightness(m_brightnessSlider->value());
    }
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onBrightnessChanged(int value)
{
    m_userInitiated = true;
    m_controller->setBrightness(value);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onContrastAutoToggled(bool checked)
{
    m_userInitiated = true;
    m_controller->setContrastAuto(checked);
    m_contrastSlider->setEnabled(!checked);
    // When switching to manual, send current slider value
    if (!checked) {
        m_controller->setContrast(m_contrastSlider->value());
    }
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onContrastChanged(int value)
{
    m_userInitiated = true;
    m_controller->setContrast(value);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onSaturationAutoToggled(bool checked)
{
    m_userInitiated = true;
    m_controller->setSaturationAuto(checked);
    m_saturationSlider->setEnabled(!checked);
    // When switching to manual, send current slider value
    if (!checked) {
        m_controller->setSaturation(m_saturationSlider->value());
    }
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onSaturationChanged(int value)
{
    m_userInitiated = true;
    m_controller->setSaturation(value);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::onWhiteBalanceChanged(int index)
{
    m_userInitiated = true;
    m_controller->setWhiteBalance(index);
    m_commandTimer->start(1000);
}

void CameraSettingsWidget::updateFromState(const CameraController::CameraState &state)
{
    // Only update if state differs, not user-initiated, command timer expired, and not settling
    bool commandInFlight = m_commandTimer->isActive();
    bool isSettling = m_controller->isSettling();

    if (!m_userInitiated && !commandInFlight && !isSettling) {
        if (m_hdrCheckBox->isChecked() != state.hdrEnabled) {
            m_hdrCheckBox->blockSignals(true);
            m_hdrCheckBox->setChecked(state.hdrEnabled);
            m_hdrCheckBox->blockSignals(false);
        }

        if (m_fovComboBox->currentIndex() != state.fovMode) {
            m_fovComboBox->blockSignals(true);
            m_fovComboBox->setCurrentIndex(state.fovMode);
            m_fovComboBox->blockSignals(false);
        }

        if (m_faceAECheckBox->isChecked() != state.faceAEEnabled) {
            m_faceAECheckBox->blockSignals(true);
            m_faceAECheckBox->setChecked(state.faceAEEnabled);
            m_faceAECheckBox->blockSignals(false);
        }

        if (m_faceFocusCheckBox->isChecked() != state.faceFocusEnabled) {
            m_faceFocusCheckBox->blockSignals(true);
            m_faceFocusCheckBox->setChecked(state.faceFocusEnabled);
            m_faceFocusCheckBox->blockSignals(false);
        }

        // Image controls - update auto checkboxes
        if (m_brightnessAutoCheckBox->isChecked() != state.brightnessAuto) {
            m_brightnessAutoCheckBox->blockSignals(true);
            m_brightnessAutoCheckBox->setChecked(state.brightnessAuto);
            m_brightnessAutoCheckBox->blockSignals(false);
            m_brightnessSlider->setEnabled(!state.brightnessAuto);
        }

        if (m_contrastAutoCheckBox->isChecked() != state.contrastAuto) {
            m_contrastAutoCheckBox->blockSignals(true);
            m_contrastAutoCheckBox->setChecked(state.contrastAuto);
            m_contrastAutoCheckBox->blockSignals(false);
            m_contrastSlider->setEnabled(!state.contrastAuto);
        }

        if (m_saturationAutoCheckBox->isChecked() != state.saturationAuto) {
            m_saturationAutoCheckBox->blockSignals(true);
            m_saturationAutoCheckBox->setChecked(state.saturationAuto);
            m_saturationAutoCheckBox->blockSignals(false);
            m_saturationSlider->setEnabled(!state.saturationAuto);
        }
    }

    // Always update slider values when in auto mode (to show polled values)
    // When not in auto mode, only update if not user-initiated
    if (!m_userInitiated && !commandInFlight && !isSettling) {
        if (m_brightnessSlider->value() != state.brightness) {
            m_brightnessSlider->blockSignals(true);
            m_brightnessSlider->setValue(state.brightness);
            m_brightnessSlider->blockSignals(false);
        }

        if (m_contrastSlider->value() != state.contrast) {
            m_contrastSlider->blockSignals(true);
            m_contrastSlider->setValue(state.contrast);
            m_contrastSlider->blockSignals(false);
        }

        if (m_saturationSlider->value() != state.saturation) {
            m_saturationSlider->blockSignals(true);
            m_saturationSlider->setValue(state.saturation);
            m_saturationSlider->blockSignals(false);
        }

        if (m_whiteBalanceComboBox->currentIndex() != state.whiteBalance) {
            m_whiteBalanceComboBox->blockSignals(true);
            m_whiteBalanceComboBox->setCurrentIndex(state.whiteBalance);
            m_whiteBalanceComboBox->blockSignals(false);
        }
    }

    // Clear user-initiated flag when command timer expires
    if (!commandInFlight && m_userInitiated) {
        m_userInitiated = false;
    }
}
