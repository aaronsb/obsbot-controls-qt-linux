#include "PTZControlWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

PTZControlWidget::PTZControlWidget(CameraController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 14, 8, 14);
    layout->setSpacing(14);

    QGroupBox *groupBox = new QGroupBox("Pan/Tilt/Zoom Controls", this);
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);
    groupLayout->setContentsMargins(16, 16, 16, 16);
    groupLayout->setSpacing(12);

    // Pan/Tilt buttons in a grid
    QGridLayout *panTiltGrid = new QGridLayout();

    m_tiltUpBtn = new QPushButton("↑", this);
    m_tiltUpBtn->setFixedSize(60, 60);
    m_tiltUpBtn->setToolTip("Tilt Up");

    m_tiltDownBtn = new QPushButton("↓", this);
    m_tiltDownBtn->setFixedSize(60, 60);
    m_tiltDownBtn->setToolTip("Tilt Down");

    m_panLeftBtn = new QPushButton("←", this);
    m_panLeftBtn->setFixedSize(60, 60);
    m_panLeftBtn->setToolTip("Pan Left");

    m_panRightBtn = new QPushButton("→", this);
    m_panRightBtn->setFixedSize(60, 60);
    m_panRightBtn->setToolTip("Pan Right");

    m_centerBtn = new QPushButton("⊙", this);
    m_centerBtn->setFixedSize(60, 60);
    m_centerBtn->setToolTip("Center View");

    panTiltGrid->addWidget(m_tiltUpBtn, 0, 1, Qt::AlignCenter);
    panTiltGrid->addWidget(m_panLeftBtn, 1, 0, Qt::AlignCenter);
    panTiltGrid->addWidget(m_centerBtn, 1, 1, Qt::AlignCenter);
    panTiltGrid->addWidget(m_panRightBtn, 1, 2, Qt::AlignCenter);
    panTiltGrid->addWidget(m_tiltDownBtn, 2, 1, Qt::AlignCenter);

    connect(m_tiltUpBtn, &QPushButton::clicked, this, &PTZControlWidget::onTiltUpClicked);
    connect(m_tiltDownBtn, &QPushButton::clicked, this, &PTZControlWidget::onTiltDownClicked);
    connect(m_panLeftBtn, &QPushButton::clicked, this, &PTZControlWidget::onPanLeftClicked);
    connect(m_panRightBtn, &QPushButton::clicked, this, &PTZControlWidget::onPanRightClicked);
    connect(m_centerBtn, &QPushButton::clicked, this, &PTZControlWidget::onCenterClicked);

    QWidget *panTiltWidget = new QWidget(this);
    panTiltWidget->setLayout(panTiltGrid);
    groupLayout->addWidget(panTiltWidget, 0, Qt::AlignCenter);

    // Position label
    m_positionLabel = new QLabel("Position: Pan 0.00, Tilt 0.00", this);
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_positionLabel->setStyleSheet("color: palette(mid); font-size: 11px;");
    groupLayout->addWidget(m_positionLabel);

    // Zoom slider
    QHBoxLayout *zoomLayout = new QHBoxLayout();
    zoomLayout->addWidget(new QLabel("Zoom:", this));
    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setMinimum(10);  // 1.0x
    m_zoomSlider->setMaximum(20);  // 2.0x
    m_zoomSlider->setValue(10);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &PTZControlWidget::onZoomChanged);
    zoomLayout->addWidget(m_zoomSlider);
    m_zoomLabel = new QLabel("1.0x", this);
    m_zoomLabel->setMinimumWidth(40);
    zoomLayout->addWidget(m_zoomLabel);

    groupLayout->addLayout(zoomLayout);

    // Presets section
    QGroupBox *presetGroup = new QGroupBox("PTZ Presets", this);
    QVBoxLayout *presetLayout = new QVBoxLayout(presetGroup);
    presetLayout->setContentsMargins(16, 16, 16, 16);
    presetLayout->setSpacing(8);

    for (int i = 0; i < 3; ++i) {
        PresetUi presetUi{};
        presetUi.defined = false;
        presetUi.pan = 0.0;
        presetUi.tilt = 0.0;
        presetUi.zoom = 1.0;

        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(8);
        QLabel *titleLabel = new QLabel(QString("Preset %1").arg(i + 1), this);
        titleLabel->setStyleSheet("font-weight: 600; font-size: 11px;");
        row->addWidget(titleLabel);

        presetUi.statusLabel = new QLabel("Empty", this);
        presetUi.statusLabel->setStyleSheet("color: palette(mid); font-size: 11px;");
        row->addWidget(presetUi.statusLabel, 1);

        presetUi.recallButton = new QPushButton("Recall", this);
        presetUi.recallButton->setProperty("presetIndex", i);
        presetUi.recallButton->setEnabled(false);
        connect(presetUi.recallButton, &QPushButton::clicked, this, &PTZControlWidget::onRecallPreset);
        row->addWidget(presetUi.recallButton);

        presetUi.saveButton = new QPushButton("Save", this);
        presetUi.saveButton->setProperty("presetIndex", i);
        connect(presetUi.saveButton, &QPushButton::clicked, this, &PTZControlWidget::onStorePreset);
        row->addWidget(presetUi.saveButton);

        presetLayout->addLayout(row);
        m_presets[static_cast<size_t>(i)] = presetUi;
        updatePresetLabel(i);
    }

    groupLayout->addWidget(presetGroup);

    layout->addWidget(groupBox);
    layout->addStretch();
}
// 1.0 moves Tiny Lite 4K too much for fine control; updated to .05 to give finer control of PTZ adjustments
void PTZControlWidget::onPanLeftClicked()
{
    m_controller->adjustPan(-0.05);
    auto state = m_controller->getCurrentState();
    m_positionLabel->setText(QString("Position: Pan %1, Tilt %2")
        .arg(state.pan, 0, 'f', 2)
        .arg(state.tilt, 0, 'f', 2));
}

void PTZControlWidget::onPanRightClicked()
{
    m_controller->adjustPan(0.05);
    auto state = m_controller->getCurrentState();
    m_positionLabel->setText(QString("Position: Pan %1, Tilt %2")
        .arg(state.pan, 0, 'f', 2)
        .arg(state.tilt, 0, 'f', 2));
}

void PTZControlWidget::onTiltUpClicked()
{
    m_controller->adjustTilt(0.05);
    auto state = m_controller->getCurrentState();
    m_positionLabel->setText(QString("Position: Pan %1, Tilt %2")
        .arg(state.pan, 0, 'f', 2)
        .arg(state.tilt, 0, 'f', 2));
}

void PTZControlWidget::onTiltDownClicked()
{
    m_controller->adjustTilt(-0.05);
    auto state = m_controller->getCurrentState();
    m_positionLabel->setText(QString("Position: Pan %1, Tilt %2")
        .arg(state.pan, 0, 'f', 2)
        .arg(state.tilt, 0, 'f', 2));
}

void PTZControlWidget::onCenterClicked()
{
    m_controller->centerView();
    m_positionLabel->setText("Position: Pan 0.00, Tilt 0.00");
}

void PTZControlWidget::onZoomChanged(int value)
{
    double zoom = value / 10.0;  // 10-20 -> 1.0-2.0
    m_controller->setZoom(zoom);
    m_zoomLabel->setText(QString("%1x").arg(zoom, 0, 'f', 1));
}

void PTZControlWidget::updateFromState(const CameraController::CameraState &state)
{
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(static_cast<int>(state.zoom * 10));
    m_zoomLabel->setText(QString("%1x").arg(state.zoom, 0, 'f', 1));
    m_zoomSlider->blockSignals(false);

    m_positionLabel->setText(QString("Position: Pan %1, Tilt %2")
        .arg(state.pan, 0, 'f', 2)
        .arg(state.tilt, 0, 'f', 2));
}

void PTZControlWidget::applyPresetStates(const std::array<PresetState, 3> &presets)
{
    for (int i = 0; i < 3; ++i) {
        auto &ui = m_presets[static_cast<size_t>(i)];
        const auto &preset = presets[static_cast<size_t>(i)];
        ui.defined = preset.defined;
        ui.pan = preset.pan;
        ui.tilt = preset.tilt;
        ui.zoom = preset.zoom;
        updatePresetLabel(i);
    }
}

std::array<PTZControlWidget::PresetState, 3> PTZControlWidget::currentPresets() const
{
    std::array<PresetState, 3> out{};
    for (int i = 0; i < 3; ++i) {
        const auto &ui = m_presets[static_cast<size_t>(i)];
        out[static_cast<size_t>(i)] = {ui.defined, ui.pan, ui.tilt, ui.zoom};
    }
    return out;
}

void PTZControlWidget::onRecallPreset()
{
    auto *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    int index = button->property("presetIndex").toInt();
    if (index < 0 || index >= static_cast<int>(m_presets.size())) {
        return;
    }

    auto &preset = m_presets[static_cast<size_t>(index)];
    if (!preset.defined) {
        return;
    }

    m_controller->setPanTilt(preset.pan, preset.tilt);
    m_controller->setZoom(preset.zoom);

    auto state = m_controller->getCurrentState();
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(static_cast<int>(state.zoom * 10));
    m_zoomSlider->blockSignals(false);
    m_zoomLabel->setText(QString("%1x").arg(state.zoom, 0, 'f', 1));
    m_positionLabel->setText(QString("Position: Pan %1, Tilt %2")
        .arg(state.pan, 0, 'f', 2)
        .arg(state.tilt, 0, 'f', 2));
}

void PTZControlWidget::onStorePreset()
{
    auto *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    int index = button->property("presetIndex").toInt();
    if (index < 0 || index >= static_cast<int>(m_presets.size())) {
        return;
    }

    auto state = m_controller->getCurrentState();
    auto &preset = m_presets[static_cast<size_t>(index)];
    preset.defined = true;
    preset.pan = state.pan;
    preset.tilt = state.tilt;
    preset.zoom = state.zoom;
    updatePresetLabel(index);

    emit presetUpdated(index, preset.pan, preset.tilt, preset.zoom, true);
}

void PTZControlWidget::updatePresetLabel(int index)
{
    if (index < 0 || index >= static_cast<int>(m_presets.size())) {
        return;
    }
    auto &preset = m_presets[static_cast<size_t>(index)];
    if (!preset.statusLabel) {
        return;
    }

    if (preset.defined) {
        preset.statusLabel->setText(
            QString("Pan %1, Tilt %2, Zoom %3x")
                .arg(preset.pan, 0, 'f', 2)
                .arg(preset.tilt, 0, 'f', 2)
                .arg(preset.zoom, 0, 'f', 1));
    } else {
        preset.statusLabel->setText("Empty");
    }

    if (preset.recallButton) {
        preset.recallButton->setEnabled(preset.defined);
    }
}
