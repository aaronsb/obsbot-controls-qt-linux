#include "FilterPreviewWidget.h"

#include <QDebug>
#include <QOpenGLFunctions>
#include <QVector2D>
#include <QVideoFrame>
#include <QtMath>

namespace {

const char *kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;

uniform vec2 u_scale;

out vec2 v_texCoord;

void main()
{
    vec2 scaledPos = vec2(a_position.x / u_scale.x, a_position.y / u_scale.y);
    gl_Position = vec4(scaledPos, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";

const char *kFragmentShaderSource = R"(#version 330 core
uniform sampler2D u_texture;
uniform int u_filter;
uniform float u_strength;

in vec2 v_texCoord;
out vec4 fragColor;

vec3 toGrayscale(vec3 color)
{
    float gray = dot(color, vec3(0.299, 0.587, 0.114));
    return vec3(gray);
}

vec3 toSepia(vec3 color)
{
    float r = dot(color, vec3(0.393, 0.769, 0.189));
    float g = dot(color, vec3(0.349, 0.686, 0.168));
    float b = dot(color, vec3(0.272, 0.534, 0.131));
    return vec3(r, g, b);
}

vec3 toWarm(vec3 color)
{
    return color + vec3(0.05, 0.03, -0.02);
}

vec3 toCool(vec3 color)
{
    return color + vec3(-0.02, 0.03, 0.05);
}

void main()
{
    vec4 src = texture(u_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y));
    vec3 color = src.rgb;
    vec3 target = color;

    if (u_filter == 1) {
        target = toGrayscale(color);
    } else if (u_filter == 2) {
        target = toSepia(color);
    } else if (u_filter == 3) {
        target = vec3(1.0) - color;
    } else if (u_filter == 4) {
        target = toWarm(color);
    } else if (u_filter == 5) {
        target = toCool(color);
    }

    color = mix(color, clamp(target, 0.0, 1.0), clamp(u_strength, 0.0, 1.0));
    fragColor = vec4(color, src.a);
}
)";

} // namespace

FilterPreviewWidget::FilterPreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_textureDirty(false)
    , m_emitPending(false)
    , m_filterType(FilterType::None)
    , m_filterStrength(1.0f)
    , m_vertexBuffer(QOpenGLBuffer::VertexBuffer)
    , m_geometryInitialized(false)
{
    setMinimumSize(320, 240);
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
}

FilterPreviewWidget::~FilterPreviewWidget()
{
    if (isValid()) {
        makeCurrent();
        cleanupGLResources();
        doneCurrent();
    }
}

void FilterPreviewWidget::setFilter(FilterType type)
{
    if (m_filterType == type) {
        return;
    }
    m_filterType = type;
    update();
}

void FilterPreviewWidget::setFilterStrength(float strength)
{
    float clamped = qBound(0.0f, strength, 1.0f);
    if (qFuzzyCompare(clamped, m_filterStrength)) {
        return;
    }
    m_filterStrength = clamped;
    update();
}

void FilterPreviewWidget::updateVideoFrame(const QVideoFrame &frame)
{
    QVideoFrame copy(frame);
    if (!copy.isValid()) {
        return;
    }

    QImage image = copy.toImage();
    if (image.isNull()) {
        return;
    }

    if (image.format() != QImage::Format_RGBA8888) {
        image = image.convertToFormat(QImage::Format_RGBA8888);
    }

    m_currentImage = image;
    m_textureDirty = true;
    m_emitPending = true;
    update();
}

void FilterPreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    if (context()) {
        connect(context(), &QOpenGLContext::aboutToBeDestroyed,
                this, &FilterPreviewWidget::handleContextAboutToBeDestroyed,
                Qt::DirectConnection);
    }

    cleanupGLResources();

    ensureProgram();
    ensureGeometry();
}

void FilterPreviewWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void FilterPreviewWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_currentImage.isNull()) {
        return;
    }

    ensureProgram();
    ensureGeometry();
    uploadTextureIfNeeded();

    if (!m_program || !m_texture) {
        return;
    }

    const QSize frameSize = m_currentImage.size();
    ensureFramebuffer(frameSize);

    if (m_emitPending && m_framebuffer) {
        m_framebuffer->bind();
        renderToCurrentTarget(frameSize);

        QImage output(frameSize, QImage::Format_RGBA8888);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, frameSize.width(), frameSize.height(),
                     GL_RGBA, GL_UNSIGNED_BYTE, output.bits());
        m_framebuffer->release();

        emit processedFrameReady(output.flipped(Qt::Vertical));
        m_emitPending = false;
    }

    renderToCurrentTarget(size());
}

void FilterPreviewWidget::ensureProgram()
{
    if (m_program) {
        return;
    }

    m_program = std::make_unique<QOpenGLShaderProgram>();
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShaderSource)) {
        qWarning() << "Failed to compile vertex shader:" << m_program->log();
    }
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShaderSource)) {
        qWarning() << "Failed to compile fragment shader:" << m_program->log();
    }
    if (!m_program->link()) {
        qWarning() << "Failed to link shader program:" << m_program->log();
        m_program.reset();
        return;
    }
}

void FilterPreviewWidget::ensureGeometry()
{
    if (m_geometryInitialized) {
        return;
    }

    static const float vertexData[] = {
        // position   // texCoord
        -1.f, -1.f,   0.f, 1.f,
         1.f, -1.f,   1.f, 1.f,
        -1.f,  1.f,   0.f, 0.f,
         1.f,  1.f,   1.f, 0.f
    };

    m_vertexArray.create();
    QOpenGLVertexArrayObject::Binder binder(&m_vertexArray);

    m_vertexBuffer.create();
    m_vertexBuffer.bind();
    m_vertexBuffer.allocate(vertexData, sizeof(vertexData));

    if (m_program) {
        m_program->bind();
        m_program->enableAttributeArray(0);
        m_program->enableAttributeArray(1);
        m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));
        m_program->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));
        m_program->release();
    }

    m_vertexBuffer.release();
    m_geometryInitialized = true;
}

void FilterPreviewWidget::ensureFramebuffer(const QSize &size)
{
    if (size.isEmpty()) {
        m_framebuffer.reset();
        return;
    }

    if (m_framebuffer && m_framebuffer->size() == size) {
        return;
    }

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    format.setTextureTarget(GL_TEXTURE_2D);
    format.setInternalTextureFormat(GL_RGBA8);

    m_framebuffer = std::make_unique<QOpenGLFramebufferObject>(size, format);
    if (!m_framebuffer->isValid()) {
        qWarning() << "Failed to create framebuffer object for filter preview";
        m_framebuffer.reset();
    }
}

void FilterPreviewWidget::uploadTextureIfNeeded()
{
    if (!m_textureDirty || m_currentImage.isNull()) {
        return;
    }

    const QSize frameSize = m_currentImage.size();
    if (!m_texture) {
        m_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    }

    if (!m_texture->isCreated() ||
        m_texture->width() != frameSize.width() ||
        m_texture->height() != frameSize.height()) {
        m_texture->destroy();
        m_texture->create();
        m_texture->bind();
        m_texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        m_texture->setSize(frameSize.width(), frameSize.height());
        m_texture->setMipLevels(1);
        m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        m_texture->setMinificationFilter(QOpenGLTexture::Linear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    } else {
        m_texture->bind();
    }

    QImage glImage = m_currentImage.flipped(Qt::Vertical);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    frameSize.width(), frameSize.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, glImage.constBits());
    m_texture->release();

    m_textureDirty = false;
}

void FilterPreviewWidget::renderToCurrentTarget(const QSize &targetSize)
{
    if (!m_program || !m_texture) {
        return;
    }

    const qreal dpr = devicePixelRatioF();
    const QSize physicalSize = (QSizeF(targetSize) * dpr).toSize();

    glViewport(0, 0, physicalSize.width(), physicalSize.height());
    glClear(GL_COLOR_BUFFER_BIT);

    m_program->bind();
    m_program->setUniformValue("u_filter", static_cast<int>(m_filterType));
    m_program->setUniformValue("u_strength", m_filterStrength);
    m_program->setUniformValue("u_texture", 0);

    const QSizeF frameSize = frameAspectSize();
    const qreal frameAspect = frameSize.width() / frameSize.height();
    const qreal targetAspect = static_cast<qreal>(targetSize.width()) / targetSize.height();

    QVector2D scale(1.0f, 1.0f);
    if (frameAspect > targetAspect) {
        scale.setY(frameAspect / targetAspect);
    } else {
        scale.setX(targetAspect / frameAspect);
    }

    m_program->setUniformValue("u_scale", scale);

    glActiveTexture(GL_TEXTURE0);
    m_texture->bind();

    QOpenGLVertexArrayObject::Binder binder(&m_vertexArray);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_texture->release();
    m_program->release();
}

QSizeF FilterPreviewWidget::frameAspectSize() const
{
    if (m_currentImage.isNull()) {
        return QSizeF(16.0, 9.0);
    }
    return QSizeF(m_currentImage.size());
}

void FilterPreviewWidget::cleanupGLResources()
{
    if (m_texture) {
        m_texture->destroy();
        m_texture.reset();
    }
    if (m_framebuffer) {
        m_framebuffer.reset();
    }
    if (m_vertexBuffer.isCreated()) {
        m_vertexBuffer.destroy();
    }
    if (m_vertexArray.isCreated()) {
        m_vertexArray.destroy();
    }
    m_program.reset();
    m_geometryInitialized = false;
}

void FilterPreviewWidget::handleContextAboutToBeDestroyed()
{
    if (!isValid()) {
        return;
    }

    makeCurrent();
    cleanupGLResources();
    doneCurrent();
}
