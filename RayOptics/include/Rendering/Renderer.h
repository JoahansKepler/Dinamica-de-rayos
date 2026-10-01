#pragma once
// ============================================================================
// RayOptics - Rendering::Renderer
// ----------------------------------------------------------------------------
// Dibuja segmentos de línea (polilíneas) en pantalla a partir de una lista
// de puntos en coordenadas del MUNDO FÍSICO (las mismas unidades que usa
// Physics::Ray y, en fases futuras, Geometry::Circle/Ellipse).
//
// Principio de diseño clave (requisito de la sección 9): "El renderer debe
// ser independiente del motor físico". Por eso Renderer:
//   - NO sabe qué es un rayo, una reflexión, un círculo o una elipse.
//   - Solo sabe dibujar: "aquí tienes una lista de puntos, dibuja los
//     segmentos P0->P1->P2->...->Pn con este color".
//
// La física (Fases 3, 6, 7...) producirá esa lista de puntos
// (std::vector<Math::Vector2>) sin saber nada de OpenGL, VAOs, shaders,
// etc. Esta separación es la que permitirá, más adelante, cambiar de
// motor gráfico sin tocar una sola línea de física.
// ============================================================================

#include "Math/Vector2.h"
#include "Rendering/Shader.h"

#include <glad/gl.h>

#include <string>
#include <vector>

namespace RayOptics::Rendering
{

class Renderer
{
public:
    // Compila el shader de líneas (line.vert + line.frag) y crea el
    // VAO/VBO que se reutilizará para cada llamada a drawPolyline().
    Renderer(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

    ~Renderer();

    // El Renderer posee recursos de OpenGL (VAO, VBO) identificados por
    // enteros (GLuint) que la GPU interpreta como handles. Copiarlos
    // produciría dos objetos C++ apuntando al mismo recurso de GPU, y al
    // destruirse uno, el otro quedaría con handles colgantes. Se prohíbe
    // copia y movimiento: en este proyecto solo existe una instancia de
    // Renderer, creada una vez en main(), así que no se necesita.
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // ---- Cámara ortográfica 2D ---------------------------------------------
    // Define qué región del mundo físico (en las mismas unidades que usan
    // Ray, Circle, Ellipse) es visible en la ventana, mapeándola al cubo
    // de coordenadas de dispositivo normalizado [-1,1]x[-1,1] de OpenGL.
    // 'halfWidth'/'halfHeight' son semi-extensiones en unidades del mundo;
    // ajustarlas junto con la relación de aspecto de la ventana evita que
    // círculos y elipses se vean deformados en pantalla.
    void setCamera(const Math::Vector2& center, double halfWidth, double halfHeight) noexcept;

    // ---- Dibujo -------------------------------------------------------------
    // Dibuja los segmentos P0->P1->P2->...->Pn definidos por 'points',
    // con color (r,g,b) en rango [0,1] y grosor 'lineWidth' en píxeles.
    // Si 'points' tiene menos de 2 elementos, no dibuja nada (no hay
    // ningún segmento que trazar).
    //
    // NOTA SOBRE 'lineWidth': OpenGL 3.3 Core solo GARANTIZA soporte para
    // líneas de 1 píxel. Muchos drivers (especialmente en Windows/Linux
    // con Mesa/NVIDIA/AMD) sí respetan valores mayores vía glLineWidth(),
    // pero otros (notablemente macOS con su backend Metal/ANGLE) los
    // ignoran silenciosamente y siempre dibujan a 1px. Si necesitas
    // líneas gruesas garantizadas en todas las plataformas, la técnica
    // robusta es dibujar cada segmento como un rectángulo (dos
    // triángulos) en vez de una primitiva GL_LINE; no se implementa aquí
    // porque añade complejidad no pedida, pero queda documentado por si
    // hace falta en el futuro.
    //
    // NOTA DE PRECISION: la física interna usa 'double' (requisito de la
    // sección 13), pero los VBO de OpenGL 3.3 Core con
    // glVertexAttribPointer trabajan de forma nativa y eficiente con
    // 'float' (GL_FLOAT). La conversion double->float ocurre SOLO aqui,
    // en la frontera entre fisica y render, justo antes de subir los
    // datos a la GPU: la perdida de precision (de ~15-17 a ~7 cifras
    // significativas) es completamente irrelevante para dibujar pixeles
    // en una pantalla, e importante NO usarla en ningun calculo fisico.
    void drawPolyline(const std::vector<Math::Vector2>& points, float r, float g, float b,
                       float lineWidth = 1.0f);

    // ---- Punto animado ------------------------------------------------------
    // Dibuja un único punto (GL_POINTS) en 'point', con color (r,g,b) y
    // tamaño en píxeles 'pixelSize'. Pensado para un marcador brillante
    // que recorre una trayectoria ya calculada (ver main.cpp: el punto
    // avanza con el tiempo, en bucle, a lo largo de una polilínea de
    // trayectoria ya existente — la física no cambia, solo se anima QUÉ
    // parte de ella se resalta en cada frame).
    void drawPoint(const Math::Vector2& point, float r, float g, float b, float pixelSize = 8.0f);

private:
    Shader m_shader;
    GLuint m_vao{0};
    GLuint m_vbo{0};

    Math::Vector2 m_cameraCenter{0.0, 0.0};
    double m_cameraHalfWidth{1.0};
    double m_cameraHalfHeight{1.0};

    // Buffer reutilizado entre llamadas a drawPolyline para evitar
    // reasignar memoria en cada frame (relevante quando la Fase 11 en
    // adelante dibuje trayectorias con animacion frame a frame).
    std::vector<float> m_vertexScratchBuffer;
};

} // namespace RayOptics::Rendering
