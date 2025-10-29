#ifndef PREVIEWWINDOW_H
#define PREVIEWWINDOW_H

#include <QWidget>

class CameraPreviewWidget;
class QCloseEvent;
class QVBoxLayout;

class PreviewWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget *parent = nullptr);

    void setPreviewWidget(CameraPreviewWidget *widget);
    CameraPreviewWidget *takePreviewWidget();

signals:
    void previewClosed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QVBoxLayout *m_layout;
    CameraPreviewWidget *m_previewWidget;
};

#endif // PREVIEWWINDOW_H
