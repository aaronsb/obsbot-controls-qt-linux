#ifndef FILTERPREVIEWWIDGET_H
#define FILTERPREVIEWWIDGET_H

#include <QImage>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QVideoFrame>
#include <memory>

class FilterPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    enum class FilterType {
        None = 0,
        Grayscale = 1,
        Sepia = 2,
        Invert = 3,
        Warm = 4,
        Cool = 5
    };

    explicit FilterPreviewWidget(QWidget *parent = nullptr);
    ~FilterPreviewWidget() override;

    void setFilter(FilterType type);
    void setFilterStrength(float strength); // 0.0 - 1.0
    void updateVideoFrame(const QVideoFrame &frame);

signals:
    void processedFrameReady(const QImage &frame);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void ensureProgram();
    void ensureGeometry();
    void ensureFramebuffer(const QSize &size);
    void uploadTextureIfNeeded();
    void renderToCurrentTarget(const QSize &targetSize);
    QSizeF frameAspectSize() const;
    void cleanupGLResources();

    QImage m_currentImage;
    bool m_textureDirty;
    bool m_emitPending;
    FilterType m_filterType;
    float m_filterStrength;

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    std::unique_ptr<QOpenGLTexture> m_texture;
    std::unique_ptr<QOpenGLFramebufferObject> m_framebuffer;
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLVertexArrayObject m_vertexArray;
    bool m_geometryInitialized;

private slots:
    void handleContextAboutToBeDestroyed();
};

#endif // FILTERPREVIEWWIDGET_H
