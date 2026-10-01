cmake_minimum_required(VERSION 3.16)
project(RayOptics LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Estándar de C++
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tipo de build por defecto (si el usuario no especifica uno)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencias externas
# ---------------------------------------------------------------------------
# GLFW: se busca como paquete del sistema (instalado vía apt: libglfw3-dev,
# o vía vcpkg/pacman en otras plataformas). Esto evita tener que vendorizar
# todo su código fuente dentro del repositorio.
find_package(glfw3 3.3 REQUIRED)

# OpenGL (necesario para enlazar contra la librería del sistema; GLAD se
# encarga de cargar las funciones en tiempo de ejecución, pero igualmente
# se debe enlazar contra libGL/opengl32 según la plataforma).
find_package(OpenGL REQUIRED)

# GLAD: SÍ se vendoriza (external/glad), porque GLAD no es una librería
# genérica: es código generado específicamente para la versión y el perfil
# de OpenGL que se quiere usar (aquí: OpenGL 3.3 Core Profile). Se compila
# como una pequeña librería estática propia.
add_library(glad STATIC
    external/glad/src/gl.c
)
target_include_directories(glad PUBLIC
    external/glad/include
)

# ---------------------------------------------------------------------------
# Ejecutable principal
# ---------------------------------------------------------------------------
add_executable(RayOptics
    src/main.cpp
    src/Rendering/Shader.cpp
    src/Rendering/Renderer.cpp
)

target_include_directories(RayOptics PRIVATE
    include
)

target_link_libraries(RayOptics PRIVATE
    glad
    glfw
    OpenGL::GL
)

# ---------------------------------------------------------------------------
# Copiar la carpeta de shaders al directorio de salida del ejecutable
# ---------------------------------------------------------------------------
add_custom_command(TARGET RayOptics POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:RayOptics>/shaders
)

# ---------------------------------------------------------------------------
# Warnings estrictos (ayudan a detectar errores de física/matemáticas pronto)
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(RayOptics PRIVATE -Wall -Wextra -Wpedantic)
endif()
