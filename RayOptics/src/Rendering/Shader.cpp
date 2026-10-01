#include "Rendering/Shader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace RayOptics::Rendering
{

namespace
{
    // Lee un archivo de texto completo a un std::string. Se usa para cargar
    // el código fuente GLSL desde disco (shaders/line.vert, shaders/line.frag).
    std::string readTextFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error(
                "Shader: no se pudo abrir el archivo '" + path + "'. "
                "Verifica que la carpeta 'shaders/' este junto al ejecutable "
                "o en el directorio desde el que lo ejecutas.");
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}

GLuint Shader::compileShaderStage(const std::string& sourcePath, GLenum stageType)
{
    const std::string source = readTextFile(sourcePath);
    const char* sourceCStr = source.c_str();

    const GLuint stage = glCreateShader(stageType);
    glShaderSource(stage, 1, &sourceCStr, nullptr);
    glCompileShader(stage);

    GLint success = GL_FALSE;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        GLint logLength = 0;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(static_cast<size_t>(logLength > 0 ? logLength : 1));
        glGetShaderInfoLog(stage, logLength, nullptr, log.data());

        glDeleteShader(stage);

        throw std::runtime_error(
            "Shader: error de compilacion en '" + sourcePath + "':\n" + log.data());
    }

    return stage;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    const GLuint vertexStage = compileShaderStage(vertexPath, GL_VERTEX_SHADER);
    const GLuint fragmentStage = compileShaderStage(fragmentPath, GL_FRAGMENT_SHADER);

    m_id = glCreateProgram();
    glAttachShader(m_id, vertexStage);
    glAttachShader(m_id, fragmentStage);
    glLinkProgram(m_id);

    // Los shaders individuales ya no son necesarios una vez enlazados en
    // el programa: se marcan para borrado independientemente de si el
    // enlazado tuvo éxito o no.
    glDeleteShader(vertexStage);
    glDeleteShader(fragmentStage);

    GLint linkSuccess = GL_FALSE;
    glGetProgramiv(m_id, GL_LINK_STATUS, &linkSuccess);
    if (linkSuccess == GL_FALSE)
    {
        GLint logLength = 0;
        glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(static_cast<size_t>(logLength > 0 ? logLength : 1));
        glGetProgramInfoLog(m_id, logLength, nullptr, log.data());

        const GLuint failedId = m_id;
        m_id = 0;
        glDeleteProgram(failedId);

        throw std::runtime_error(
            std::string("Shader: error al enlazar el programa:\n") + log.data());
    }
}

Shader::~Shader()
{
    if (m_id != 0)
    {
        glDeleteProgram(m_id);
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_id(std::exchange(other.m_id, 0))
{
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        if (m_id != 0)
        {
            glDeleteProgram(m_id);
        }
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

void Shader::use() const noexcept
{
    glUseProgram(m_id);
}

GLint Shader::uniformLocation(const std::string& name) const
{
    // No se cachean las localizaciones de uniforms en un mapa: con el
    // número de uniforms que tiene este proyecto (un puñado), el costo de
    // glGetUniformLocation es insignificante frente a la claridad de no
    // introducir una caché que podría invalidarse si el programa se
    // recompila en caliente en el futuro (hot-reload de shaders).
    return glGetUniformLocation(m_id, name.c_str());
}

void Shader::setVec2(const std::string& name, float x, float y) const
{
    glUniform2f(uniformLocation(name), x, y);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
    glUniform3f(uniformLocation(name), x, y, z);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(uniformLocation(name), value);
}

} // namespace RayOptics::Rendering
