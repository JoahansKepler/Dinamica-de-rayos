#include "Rendering/Renderer.h"

namespace RayOptics::Rendering
{

Renderer::Renderer(const std::string& vertexShaderPath, const std::string& fragmentShaderPath)
    : m_shader(vertexShaderPath, fragmentShaderPath)
{
    // ------------------------------------------------------------------
    // Creación de VAO + VBO (requisito explícito: nada de glBegin/glEnd).
    // ------------------------------------------------------------------
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Se reserva el buffer sin datos todavía (nullptr): el contenido real
    // se sube en cada llamada a drawPolyline() mediante glBufferData,
    // porque la trayectoria a dibujar cambia (numero de puntos y valores)
    // en cada fase del proyecto y, mas adelante, en cada frame de
    // animacion. GL_DYNAMIC_DRAW le indica al driver que espere
    // actualizaciones frecuentes de este buffer.
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Layout location 0 en el vertex shader: vec2 (x, y) por vertice,
    // sin padding entre vertices (stride = 2 floats).
    glVertexAttribPointer(
        0,                              // location
        2,                              // componentes por vertice (x, y)
        GL_FLOAT,                       // tipo
        GL_FALSE,                       // normalizar (no aplica a floats)
        2 * sizeof(float),              // stride
        nullptr                         // offset
    );
    glEnableVertexAttribArray(0);

    // Se desvincula el VAO (buena practica: evita modificaciones
    // accidentales de su estado desde otro punto del codigo hasta la
    // proxima vez que se vuelva a vincular explicitamente).
    glBindVertexArray(0);
}

Renderer::~Renderer()
{
    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void Renderer::setCamera(const Math::Vector2& center, double halfWidth, double halfHeight) noexcept
{
    m_cameraCenter = center;
    m_cameraHalfWidth = halfWidth;
    m_cameraHalfHeight = halfHeight;
}

void Renderer::drawPolyline(const std::vector<Math::Vector2>& points, float r, float g, float b,
                             float lineWidth)
{
    // Un segmento requiere al menos dos puntos. Menos que eso no es un
    // error (por ejemplo, una trayectoria que todavia no ha calculado
    // ninguna reflexion tiene un solo punto: el origen), simplemente no
    // hay nada que dibujar todavia.
    if (points.size() < 2)
    {
        return;
    }

    // ------------------------------------------------------------------
    // Conversion double (fisica) -> float (GPU). Ver nota de precision
    // en el header.
    // ------------------------------------------------------------------
    m_vertexScratchBuffer.clear();
    m_vertexScratchBuffer.reserve(points.size() * 2);
    for (const Math::Vector2& p : points)
    {
        m_vertexScratchBuffer.push_back(static_cast<float>(p.x));
        m_vertexScratchBuffer.push_back(static_cast<float>(p.y));
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Se resube el buffer completo con glBufferData (en vez de
    // glBufferSubData) porque el numero de puntos cambia de una llamada a
    // otra (una trayectoria con mas reflexiones tiene mas puntos que
    // otra). GL_DYNAMIC_DRAW ya declarado en el constructor sigue siendo
    // la politica de uso correcta para este patron.
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertexScratchBuffer.size() * sizeof(float)),
        m_vertexScratchBuffer.data(),
        GL_DYNAMIC_DRAW
    );

    m_shader.use();
    m_shader.setVec2("uCameraCenter", static_cast<float>(m_cameraCenter.x),
                                       static_cast<float>(m_cameraCenter.y));
    m_shader.setVec2("uCameraHalfExtents", static_cast<float>(m_cameraHalfWidth),
                                            static_cast<float>(m_cameraHalfHeight));
    m_shader.setVec3("uColor", r, g, b);

    // Ver nota en el header sobre soporte de plataforma para grosores
    // mayores a 1px.
    glLineWidth(lineWidth);

    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(points.size()));

    glBindVertexArray(0);
}

void Renderer::drawPoint(const Math::Vector2& point, float r, float g, float b, float pixelSize)
{
    // Reutiliza el MISMO VAO/VBO/shader que drawPolyline: un único punto
    // es, para la GPU, un "buffer de vértices" de tamaño 1, dibujado con
    // GL_POINTS en vez de GL_LINE_STRIP. No hace falta un shader ni un
    // VAO separados solo para esto.
    m_vertexScratchBuffer.clear();
    m_vertexScratchBuffer.push_back(static_cast<float>(point.x));
    m_vertexScratchBuffer.push_back(static_cast<float>(point.y));

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertexScratchBuffer.size() * sizeof(float)),
        m_vertexScratchBuffer.data(),
        GL_DYNAMIC_DRAW
    );

    m_shader.use();
    m_shader.setVec2("uCameraCenter", static_cast<float>(m_cameraCenter.x),
                                       static_cast<float>(m_cameraCenter.y));
    m_shader.setVec2("uCameraHalfExtents", static_cast<float>(m_cameraHalfWidth),
                                            static_cast<float>(m_cameraHalfHeight));
    m_shader.setVec3("uColor", r, g, b);

    // glPointSize() es una función estándar de OpenGL 3.3 Core (no forma
    // parte de la API fija "glBegin/glEnd" prohibida por los requisitos;
    // es simplemente el parámetro de estado que controla el tamaño en
    // píxeles de la primitiva GL_POINTS). Sin llamar a esto, el tamaño
    // por defecto es de 1 píxel, prácticamente invisible.
    glPointSize(pixelSize);
    glDrawArrays(GL_POINTS, 0, 1);

    glBindVertexArray(0);
}

} // namespace RayOptics::Rendering
