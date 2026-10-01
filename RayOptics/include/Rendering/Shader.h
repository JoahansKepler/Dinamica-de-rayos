#pragma once
// ============================================================================
// RayOptics - Rendering::Shader
// ----------------------------------------------------------------------------
// Encapsula la carga, compilación y enlazado de un programa de shaders
// (vertex + fragment) de OpenGL 3.3 Core. Es deliberadamente pequeña: no
// intenta ser un sistema de gestión de shaders genérico (sin soporte de
// geometry/compute shaders, sin "#include" dentro de GLSL, etc.) porque el
// proyecto, por ahora, solo necesita dibujar líneas con un color uniforme.
//
// Decisión de diseño: SÍ tiene .cpp (a diferencia de Vector2 y Ray).
//   A diferencia de los tipos matemáticos, compilar un shader implica
//   llamadas a la API de OpenGL, manejo de errores de compilación/enlazado
//   y lectura de archivos: es lógica con efectos secundarios reales, no
//   una operación aritmética pura. Separar declaración e implementación
//   aquí sí tiene sentido y evita que main.cpp (o cualquier otro archivo
//   que incluya este header) se vea forzado a incluir <fstream>, <sstream>,
//   etc. solo por usar la clase.
// ============================================================================

#include <glad/gl.h>

#include <string>

namespace RayOptics::Rendering
{

class Shader
{
public:
    // Compila y enlaza un programa de shaders a partir de dos archivos de
    // texto (código fuente GLSL). Lanza std::runtime_error con un mensaje
    // descriptivo si la compilación o el enlazado fallan: un shader que no
    // compila es un error irrecuperable para el programa (no hay un
    // "shader por defecto" razonable al que recurrir en un simulador
    // donde el renderer es toda la interfaz visual).
    Shader(const std::string& vertexPath, const std::string& fragmentPath);

    // Libera el programa de OpenGL asociado.
    ~Shader();

    // No tiene sentido copiar un Shader: dos copias compartirían el mismo
    // 'id' de OpenGL, y al destruirse una, la otra quedaría con un
    // identificador inválido (double free conceptual). Se prohíbe la copia
    // y se permite mover, transfiriendo la propiedad del recurso de OpenGL.
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Activa este programa de shaders (glUseProgram).
    void use() const noexcept;

    // ---- Uniforms ---------------------------------------------------------
    // Solo se exponen los tipos de uniform que el proyecto necesita hasta
    // ahora (un color RGB y un par de vectores 2D para la transformación
    // de cámara ortográfica). Se amplía según haga falta en fases
    // posteriores, en vez de anticipar una interfaz genérica tipo
    // "setUniform<T>" que hoy no aportaría nada y complicaría el código.
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setFloat(const std::string& name, float value) const;

    [[nodiscard]] GLuint id() const noexcept { return m_id; }

private:
    GLuint m_id{0};

    static GLuint compileShaderStage(const std::string& sourcePath, GLenum stageType);
    [[nodiscard]] GLint uniformLocation(const std::string& name) const;
};

} // namespace RayOptics::Rendering
