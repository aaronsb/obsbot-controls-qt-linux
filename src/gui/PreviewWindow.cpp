#include "PreviewWindow.h"

#include "CameraPreviewWidget.h"

#include <QVBoxLayout>
#include <QCloseEvent>

namespace {
constexpr int kDefaultPreviewWidth = 720;
constexpr int kDefaultPreviewHeight = 405;
}

PreviewWindow::PreviewWindow(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_previewWidget(nullptr)
{
    setWindowTitle("Camera Preview");
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowFlag(Qt::Window, true);

    m_layout->setContentsMargins(12, 12, 12, 12);
    m_layout->setSpacing(0);

    resize(kDefaultPreviewWidth, kDefaultPreviewHeight);
}

void PreviewWindow::setPreviewWidget(CameraPreviewWidget *widget)
{
    if (m_previewWidget == widget) {
        return;
    }

    if (m_previewWidget) {
        m_layout->removeWidget(m_previewWidget);
        m_previewWidget->setParent(nullptr);
    }

    m_previewWidget = widget;
    if (m_previewWidget) {
        m_previewWidget->setParent(this);
        m_previewWidget->setControlsVisible(false);
        m_layout->addWidget(m_previewWidget);
        m_previewWidget->show();
    }
}

CameraPreviewWidget *PreviewWindow::takePreviewWidget()
{
    CameraPreviewWidget *widget = m_previewWidget;
    if (m_previewWidget) {
        m_layout->removeWidget(m_previewWidget);
        m_previewWidget->setParent(nullptr);
        m_previewWidget = nullptr;
    }
    if (widget) {
        widget->show();
    }
    return widget;
}

void PreviewWindow::closeEvent(QCloseEvent *event)
{
    emit previewClosed();
    QWidget::closeEvent(event);
}
