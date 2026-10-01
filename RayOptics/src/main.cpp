// ============================================================================
// RayOptics - Fase 1
// ----------------------------------------------------------------------------
// Objetivo de esta fase (SOLO esto, nada de física ni geometría todavía):
//
//   1. Abrir una ventana con GLFW.
//   2. Crear un contexto OpenGL 3.3 Core Profile.
//   3. Cargar los punteros a funciones de OpenGL con GLAD.
//   4. Ejecutar un bucle de render que simplemente limpia la pantalla con
//      un color de fondo oscuro (ver sección 14 de los requisitos:
//      "fondo oscuro").
//   5. Cerrar la ventana correctamente al pulsar ESC o al hacer clic en la X.
//
// En fases posteriores este archivo se irá vaciando de responsabilidades:
// la física irá a Physics/, la geometría a Geometry/ y el render a
// Rendering/. Por ahora main.cpp concentra todo porque aún no hay nada que
// separar: separar prematuramente código que no existe todavía sería
// sobre-ingeniería.
// ============================================================================

// IMPORTANTE: glad.h SIEMPRE debe incluirse antes que cualquier cabecera que
// a su vez incluya <GL/gl.h> (como GLFW). Si el orden se invierte, el
// compilador se queja de "gl.h included before glad.h" o de redefiniciones
// de tipos de OpenGL. Este es el error número 1 de todo proyecto GLFW+GLAD.
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "Math/Vector2.h"
#include "Physics/Ray.h"
#include "Physics/Intersection.h"
#include "Physics/Reflection.h"
#include "Physics/Refraction.h"
#include "Physics/Trajectory.h"
#include "Geometry/Circle.h"
#include "Geometry/Ellipse.h"
#include "Rendering/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

// ----------------------------------------------------------------------------
// Configuración de la ventana (constantes con nombre, no "números mágicos"
// repartidos por el código).
// ----------------------------------------------------------------------------
namespace AppConfig
{
    constexpr int WINDOW_WIDTH = 1000;
    constexpr int WINDOW_HEIGHT = 800;
    constexpr const char* WINDOW_TITLE = "RayOptics - Simulador de reflexion 2D (Fase 1)";

    // Color de fondo oscuro pedido en los requisitos de visualización.
    // Se usa un gris muy oscuro azulado en vez de negro puro para que,
    // más adelante, los rayos y la geometría óptica (que se dibujarán en
    // colores claros) tengan buen contraste sin resultar duro a la vista.
    constexpr float BACKGROUND_R = 0.07f;
    constexpr float BACKGROUND_G = 0.08f;
    constexpr float BACKGROUND_B = 0.10f;
    constexpr float BACKGROUND_A = 1.00f;

    // Cámara ortográfica 2D (Fase 4): semi-altura visible del mundo físico,
    // en las mismas unidades que usarán Ray, Circle y Ellipse. La
    // semi-anchura se deriva de esta multiplicándola por la relación de
    // aspecto de la ventana, para que un círculo se vea como un círculo
    // y no como una elipse por deformación de la proyección.
    constexpr double CAMERA_HALF_HEIGHT = 5.0;
}

// ----------------------------------------------------------------------------
// Callback de error de GLFW.
//
// Decisión de C++: en vez de dejar que GLFW falle en silencio, registramos
// explícitamente un callback que imprime cualquier error interno de GLFW
// (por ejemplo, si el driver no soporta el perfil de OpenGL solicitado).
// Esto es fundamental para depurar problemas de creación de contexto, que
// son la fuente más común de fallos en la Fase 1.
// ----------------------------------------------------------------------------
void glfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "[GLFW Error %d]: %s\n", error, description);
}

// ----------------------------------------------------------------------------
// Callback de redimensionado de framebuffer.
//
// Se registra para que, si el usuario cambia el tamaño de la ventana, el
// viewport de OpenGL se actualice acorde. Sin esto, al redimensionar la
// ventana la imagen se vería recortada o deformada.
// ----------------------------------------------------------------------------
void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

// ----------------------------------------------------------------------------
// Escena activa de la demo: qué cavidad óptica se muestra y anima.
//
// Se alterna con la tecla TAB (ver keyCallback). Vive en un struct
// AppState en vez de una variable global suelta: GLFW no tiene forma
// nativa de pasar contexto de la aplicación a sus callbacks salvo a
// través de glfwSetWindowUserPointer()/glfwGetWindowUserPointer(), así
// que este struct es exactamente ese "contexto" que se cuelga de la
// ventana.
// ----------------------------------------------------------------------------
enum class Scene
{
    Circle,
    Ellipse
};

struct AppState
{
    Scene currentScene = Scene::Circle;
};

// ----------------------------------------------------------------------------
// Callback de teclado (sustituye a la antigua processInput() basada en
// sondeo por frame): ESC cierra la ventana, TAB alterna entre la escena
// del círculo y la de la elipse. Se usa un callback de eventos (en vez de
// sondear glfwGetKey cada frame) porque TAB es una pulsación puntual que
// debe disparar la acción UNA vez por pulsación, no repetidamente
// mientras la tecla esté apretada; GLFW ya distingue GLFW_PRESS de
// GLFW_REPEAT, así que basta con reaccionar solo a GLFW_PRESS.
// ----------------------------------------------------------------------------
void keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action != GLFW_PRESS)
    {
        return;
    }

    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (key == GLFW_KEY_TAB)
    {
        auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
        if (state != nullptr)
        {
            state->currentScene =
                (state->currentScene == Scene::Circle) ? Scene::Ellipse : Scene::Circle;
            std::printf("Escena activa: %s\n",
                        state->currentScene == Scene::Circle ? "Circulo" : "Elipse");
        }
    }
}

// ----------------------------------------------------------------------------
// Auto-test de Math::Vector2 (Fase 2).
//
// Esto NO es un framework de pruebas formal (eso, si se decide adoptar
// uno como Catch2/GoogleTest, se introduciría en una fase posterior sin
// romper esta arquitectura). Es, deliberadamente, un chequeo mínimo y
// legible en tiempo de ejecución que:
//
//   1. No requiere ningún contexto OpenGL ni ventana: se ejecuta ANTES de
//      glfwInit(), así que sirve también como prueba de compilación/lógica
//      pura en entornos sin servidor gráfico (por ejemplo, un contenedor
//      de integración continua).
//   2. Verifica cada operación pedida en el punto 8 de los requisitos:
//      suma, resta, multiplicación/división por escalar, producto punto,
//      norma y normalización.
//
// Si alguna comprobación falla, se informa por stderr pero NO se aborta
// el programa: en esta fase el objetivo es "ver" el resultado, no todavía
// integrar un sistema de tests que detenga la build (eso llega con la
// validación física formal de la Fase 12).
// ----------------------------------------------------------------------------
namespace
{
    void runVector2SelfTest()
    {
        using RayOptics::Math::Vector2;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        std::printf("Ejecutando auto-test de Math::Vector2...\n");

        const Vector2 a(3.0, 4.0);
        const Vector2 b(1.0, 2.0);

        // Suma
        const Vector2 sum = a + b;
        check(sum.isApprox(Vector2(4.0, 6.0)), "suma: (3,4) + (1,2) debe ser (4,6)");

        // Resta
        const Vector2 diff = a - b;
        check(diff.isApprox(Vector2(2.0, 2.0)), "resta: (3,4) - (1,2) debe ser (2,2)");

        // Multiplicacion por escalar (en ambos ordenes)
        const Vector2 scaledRight = a * 2.0;
        const Vector2 scaledLeft = 2.0 * a;
        check(scaledRight.isApprox(Vector2(6.0, 8.0)), "a * 2 debe ser (6,8)");
        check(scaledLeft.isApprox(scaledRight), "2 * a debe ser igual a a * 2 (conmutatividad)");

        // Division por escalar
        const Vector2 divided = a / 2.0;
        check(divided.isApprox(Vector2(1.5, 2.0)), "(3,4) / 2 debe ser (1.5, 2.0)");

        // Division por (casi) cero debe lanzar, no producir inf/NaN silenciosos
        bool threwOnDivideByZero = false;
        try
        {
            [[maybe_unused]] const Vector2 invalid = a / 0.0;
        }
        catch (const std::domain_error&)
        {
            threwOnDivideByZero = true;
        }
        check(threwOnDivideByZero, "dividir por 0 debe lanzar std::domain_error, no dar inf/NaN");

        // Producto punto: (3,4) . (1,2) = 3*1 + 4*2 = 11
        check(RayOptics::Math::isNearlyZero(a.dot(b) - 11.0),
              "producto punto de (3,4).(1,2) debe ser 11");

        // Producto punto de un vector unitario consigo mismo debe ser 1
        const Vector2 unitX(1.0, 0.0);
        check(RayOptics::Math::isNearlyZero(unitX.dot(unitX) - 1.0),
              "vector unitario . si mismo debe ser 1");

        // Norma: (3,4) tiene longitud 5 (triangulo 3-4-5)
        check(RayOptics::Math::isNearlyZero(a.length() - 5.0),
              "la norma de (3,4) debe ser 5 (triangulo 3-4-5)");

        // Normalizacion: el resultado debe tener longitud 1
        const Vector2 normalizedA = a.normalized();
        check(RayOptics::Math::isNearlyZero(normalizedA.length() - 1.0),
              "un vector normalizado debe tener longitud 1");
        check(normalizedA.isApprox(Vector2(0.6, 0.8)),
              "(3,4) normalizado debe ser (0.6, 0.8)");

        // Normalizar el vector nulo debe lanzar (no hay direccion definida)
        bool threwOnNormalizeZero = false;
        try
        {
            [[maybe_unused]] const Vector2 invalid = Vector2(0.0, 0.0).normalized();
        }
        catch (const std::domain_error&)
        {
            threwOnNormalizeZero = true;
        }
        check(threwOnNormalizeZero,
              "normalizar el vector (0,0) debe lanzar std::domain_error");

        // Perpendicular: rotar (1,0) 90 grados antihorario debe dar (0,1)
        check(unitX.perpendicular().isApprox(Vector2(0.0, 1.0)),
              "perpendicular de (1,0) debe ser (0,1)");

        // La perpendicular de un vector debe ser ortogonal a el (producto punto = 0)
        check(RayOptics::Math::isNearlyZero(a.dot(a.perpendicular())),
              "un vector y su perpendicular deben tener producto punto 0");

        if (allPassed)
        {
            std::printf("Auto-test de Vector2: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de Vector2: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }

        std::cout << "Ejemplo de uso: a = " << a << '\n';
    }

    // ------------------------------------------------------------------
    // Auto-test de Physics::Ray (Fase 3)
    // ------------------------------------------------------------------
    void runRaySelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Physics::Ray;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        std::printf("Ejecutando auto-test de Physics::Ray...\n");

        // Un rayo que parte de (0,0) hacia (1,0): direccion ya unitaria.
        const Ray horizontal(Vector2(0.0, 0.0), Vector2(1.0, 0.0));
        check(horizontal.direction.isApprox(Vector2(1.0, 0.0)),
              "direccion ya unitaria no debe alterarse");
        check(horizontal.pointAt(0.0).isApprox(Vector2(0.0, 0.0)),
              "P(0) debe ser igual al origen");
        check(horizontal.pointAt(5.0).isApprox(Vector2(5.0, 0.0)),
              "P(5) sobre un rayo horizontal unitario debe ser (5,0)");

        // Invariante de clase: la direccion SIEMPRE se normaliza, aunque
        // se pase un vector no unitario (por ejemplo, (3,4), de longitud 5).
        const Ray fromNonUnit(Vector2(1.0, 1.0), Vector2(3.0, 4.0));
        check(fromNonUnit.direction.isApprox(Vector2(0.6, 0.8)),
              "la direccion debe normalizarse automaticamente en el constructor");
        check(RayOptics::Math::isNearlyZero(fromNonUnit.direction.length() - 1.0),
              "direction.length() debe ser 1 sin importar el vector de entrada");

        // pointAt debe seguir la formula P(t) = P0 + t*d incluso con
        // origen distinto de (0,0).
        const Vector2 expected = fromNonUnit.origin + fromNonUnit.direction * 2.0;
        check(fromNonUnit.pointAt(2.0).isApprox(expected),
              "P(t) debe coincidir con origin + t*direction");

        // Un rayo con direccion (numericamente) nula debe lanzar excepcion,
        // igual que Vector2::normalized().
        bool threwOnZeroDirection = false;
        try
        {
            [[maybe_unused]] const Ray invalid(Vector2(0.0, 0.0), Vector2(0.0, 0.0));
        }
        catch (const std::domain_error&)
        {
            threwOnZeroDirection = true;
        }
        check(threwOnZeroDirection,
              "construir un Ray con direccion nula debe lanzar std::domain_error");

        // advanced(): debe mover el origen a lo largo de la direccion sin
        // tocar la direccion.
        const Ray advancedRay = horizontal.advanced(3.0);
        check(advancedRay.origin.isApprox(Vector2(3.0, 0.0)),
              "advanced(3.0) sobre rayo horizontal debe mover el origen a (3,0)");
        check(advancedRay.direction.isApprox(horizontal.direction),
              "advanced() no debe modificar la direccion del rayo");

        if (allPassed)
        {
            std::printf("Auto-test de Ray: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de Ray: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Geometry::Circle (Fase 5)
    // ------------------------------------------------------------------
    void runCircleSelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Geometry::Circle;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        std::printf("Ejecutando auto-test de Geometry::Circle...\n");

        // Circunferencia unidad centrada en el origen, para verificar los
        // casos mas simples de forma exacta.
        const Circle unitCircle(Vector2(0.0, 0.0), 1.0);

        check(unitCircle.isOnSurface(Vector2(1.0, 0.0)),
              "(1,0) debe estar sobre la circunferencia unidad");
        check(unitCircle.isOnSurface(Vector2(0.0, 1.0)),
              "(0,1) debe estar sobre la circunferencia unidad");
        check(!unitCircle.isOnSurface(Vector2(0.0, 0.0)),
              "el centro (0,0) NO debe estar sobre la circunferencia");
        check(!unitCircle.isOnSurface(Vector2(2.0, 0.0)),
              "(2,0) esta fuera, no debe considerarse 'sobre la superficie'");

        check(unitCircle.containsPoint(Vector2(0.0, 0.0)),
              "el centro debe estar DENTRO de la circunferencia");
        check(!unitCircle.containsPoint(Vector2(2.0, 0.0)),
              "(2,0) NO debe estar dentro (esta fuera del radio 1)");

        // Normal en (1,0) debe apuntar en +x; en (0,1) debe apuntar en +y.
        check(unitCircle.outwardNormalAt(Vector2(1.0, 0.0)).isApprox(Vector2(1.0, 0.0)),
              "normal en (1,0) debe ser (1,0)");
        check(unitCircle.outwardNormalAt(Vector2(0.0, 1.0)).isApprox(Vector2(0.0, 1.0)),
              "normal en (0,1) debe ser (0,1)");

        // Circunferencia general: centro desplazado y radio != 1, para
        // asegurarse de que la formula no esta "harcodeada" al caso trivial.
        const Circle generalCircle(Vector2(3.0, -2.0), 5.0);
        const Vector2 pointOnGeneral = generalCircle.center + Vector2(5.0, 0.0); // (8, -2)

        check(generalCircle.isOnSurface(pointOnGeneral),
              "un punto a distancia R del centro debe estar sobre la superficie");

        const Vector2 normalOnGeneral = generalCircle.outwardNormalAt(pointOnGeneral);
        check(RayOptics::Math::isNearlyZero(normalOnGeneral.length() - 1.0),
              "la normal debe ser un vector unitario, sin importar el radio");
        check(normalOnGeneral.isApprox(Vector2(1.0, 0.0)),
              "normal en el punto (centro + (R,0)) debe apuntar en +x");

        // La normal debe ser perpendicular a la tangente en ese punto
        // (validacion pedida explicitamente en la Fase 12, comprobada ya
        // aqui porque la propiedad es una consecuencia directa de la
        // geometria de Circle, no de ningun calculo de interseccion).
        const Vector2 tangentOnGeneral = normalOnGeneral.perpendicular();
        check(RayOptics::Math::isNearlyZero(normalOnGeneral.dot(tangentOnGeneral)),
              "la normal debe ser perpendicular a la tangente (producto punto = 0)");

        // Un radio invalido (<= 0) debe lanzar excepcion.
        bool threwOnInvalidRadius = false;
        try
        {
            [[maybe_unused]] const Circle invalid(Vector2(0.0, 0.0), 0.0);
        }
        catch (const std::domain_error&)
        {
            threwOnInvalidRadius = true;
        }
        check(threwOnInvalidRadius,
              "construir un Circle con radio 0 debe lanzar std::domain_error");

        bool threwOnNegativeRadius = false;
        try
        {
            [[maybe_unused]] const Circle invalid(Vector2(0.0, 0.0), -3.0);
        }
        catch (const std::domain_error&)
        {
            threwOnNegativeRadius = true;
        }
        check(threwOnNegativeRadius,
              "construir un Circle con radio negativo debe lanzar std::domain_error");

        if (allPassed)
        {
            std::printf("Auto-test de Circle: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de Circle: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Physics::intersectRayCircle (Fase 6)
    // ------------------------------------------------------------------
    void runIntersectionSelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Physics::Ray;
        using RayOptics::Geometry::Circle;
        using RayOptics::Physics::intersectRayCircle;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        std::printf("Ejecutando auto-test de Physics::intersectRayCircle...\n");

        const Circle circle(Vector2(0.0, 0.0), 4.0);

        // --- Caso 1: rayo desde fuera, apuntando directo al centro -----------
        // Origen (-10,0), direccion (+1,0): debe golpear en (-4,0), t=6.
        {
            const Ray ray(Vector2(-10.0, 0.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayCircle(ray, circle);

            check(hit.has_value(), "rayo apuntando al centro desde fuera debe intersectar");
            if (hit)
            {
                check(RayOptics::Math::isNearlyZero(hit->t - 6.0),
                      "t debe ser 6 (distancia de -10 a -4)");
                check(hit->point.isApprox(Vector2(-4.0, 0.0)),
                      "punto de impacto debe ser (-4,0)");
                check(hit->normal.isApprox(Vector2(-1.0, 0.0)),
                      "normal en (-4,0) debe apuntar en -x (hacia afuera)");
                check(circle.isOnSurface(hit->point),
                      "el punto de impacto debe pertenecer a la circunferencia");
            }
        }

        // --- Caso 2: rayo desde DENTRO de la cavidad --------------------------
        // Origen (0,0) (centro), direccion (+1,0): debe golpear en (4,0), t=4.
        // Este caso tiene tNear < 0 (detras del origen) y tFar > 0: se debe
        // elegir tFar, no descartar la interseccion.
        {
            const Ray ray(Vector2(0.0, 0.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayCircle(ray, circle);

            check(hit.has_value(), "rayo desde el centro debe intersectar la pared");
            if (hit)
            {
                check(RayOptics::Math::isNearlyZero(hit->t - 4.0),
                      "t debe ser 4 (radio) al disparar desde el centro");
                check(hit->point.isApprox(Vector2(4.0, 0.0)),
                      "punto de impacto debe ser (4,0)");
            }
        }

        // --- Caso 3: rayo que no toca la circunferencia (pasa por fuera) -----
        // La recta y=10 nunca esta a distancia <= 4 del origen.
        {
            const Ray ray(Vector2(-10.0, 10.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayCircle(ray, circle);
            check(!hit.has_value(), "un rayo que pasa lejos del circulo no debe intersectar");
        }

        // --- Caso 4: rayo tangente ---------------------------------------------
        // La recta y=4 es tangente a la circunferencia de radio 4 en (0,4).
        {
            const Ray ray(Vector2(-10.0, 4.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayCircle(ray, circle);

            check(hit.has_value(), "un rayo tangente SI debe reportar una interseccion (discriminante ~ 0)");
            if (hit)
            {
                check(hit->point.isApprox(Vector2(0.0, 4.0), 1e-6),
                      "el punto de tangencia debe ser (0,4)");
            }
        }

        // --- Caso 5: rayo que empieza EXACTAMENTE en la superficie, alejandose ---
        // Origen (4,0), direccion (+1,0) (alejandose del circulo hacia +x):
        // ambas raices de la cuadratica quedan en t <= epsilon (una es 0,
        // el propio origen; la otra es negativa). No debe haber interseccion
        // "hacia adelante": esto es exactamente la salvaguarda numerica que
        // evita que un rayo reflejado vuelva a chocar consigo mismo.
        {
            const Ray ray(Vector2(4.0, 0.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayCircle(ray, circle);
            check(!hit.has_value(),
                  "un rayo que parte de la superficie alejandose no debe reintersectar consigo mismo");
        }

        // --- Caso 6: rayo diagonal, para verificar que no esta 'harcodeado' a ejes ---
        // Origen (-10,-10), direccion hacia el origen (normalizada).
        // Debe golpear la circunferencia en algun punto sobre ella, y ese
        // punto debe estar en la mitad del circulo mas cercana al origen
        // del rayo (componentes x,y ambas negativas).
        {
            const Vector2 origin(-10.0, -10.0);
            const Vector2 direction = (Vector2(0.0, 0.0) - origin).normalized();
            const Ray ray(origin, direction);
            const auto hit = intersectRayCircle(ray, circle);

            check(hit.has_value(), "rayo diagonal hacia el centro debe intersectar");
            if (hit)
            {
                check(circle.isOnSurface(hit->point),
                      "punto de impacto diagonal debe pertenecer a la circunferencia");
                check(hit->point.x < 0.0 && hit->point.y < 0.0,
                      "el impacto diagonal debe estar en el cuadrante mas cercano al origen del rayo");
                check(RayOptics::Math::isNearlyZero(hit->normal.length() - 1.0),
                      "la normal en el impacto diagonal debe ser unitaria");
            }
        }

        if (allPassed)
        {
            std::printf("Auto-test de intersectRayCircle: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de intersectRayCircle: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Physics::reflect / reflectedRayFrom (Fase 7)
    // ------------------------------------------------------------------
    void runReflectionSelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Physics::Ray;
        using RayOptics::Geometry::Circle;
        using RayOptics::Physics::reflect;
        using RayOptics::Physics::reflectedRayFrom;
        using RayOptics::Physics::intersectRayCircle;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        // Angulo (en radianes) entre dos vectores unitarios, via acos del
        // producto punto. Se usa solo en este auto-test, para verificar
        // explicitamente que angulo_incidencia == angulo_reflexion; no
        // forma parte de la interfaz publica de Physics (no hace falta
        // en ningun otro sitio del proyecto todavia).
        const auto angleBetween = [](const Vector2& a, const Vector2& b) -> double
        {
            const double cosTheta = std::clamp(a.dot(b), -1.0, 1.0);
            return std::acos(cosTheta);
        };

        std::printf("Ejecutando auto-test de Physics::reflect...\n");

        // --- Caso 1: incidencia normal (de frente), 0 grados -------------------
        // d y n exactamente opuestos: el rayo debe rebotar exactamente
        // hacia atras.
        {
            const Vector2 d(1.0, 0.0);
            const Vector2 n(-1.0, 0.0);
            const Vector2 r = reflect(d, n);
            check(r.isApprox(Vector2(-1.0, 0.0)),
                  "incidencia normal (0 grados): el rayo debe rebotar exactamente hacia atras");
        }

        // --- Caso 2: espejo horizontal clasico (como una pelota rebotando) -----
        // d apunta en diagonal hacia abajo-derecha, n es "hacia arriba":
        // el resultado clasico es que se invierte solo la componente y.
        {
            const Vector2 d = Vector2(1.0, -1.0).normalized();
            const Vector2 n(0.0, 1.0);
            const Vector2 r = reflect(d, n);
            check(r.isApprox(Vector2(1.0, 1.0).normalized()),
                  "espejo horizontal: la componente x se conserva, la y se invierte");
        }

        // --- Caso 3: conservacion de magnitud ------------------------------------
        {
            const Vector2 d = Vector2(3.0, 1.0).normalized();
            const Vector2 n = Vector2(-0.4, 0.9).normalized();
            const Vector2 r = reflect(d, n);
            check(RayOptics::Math::isNearlyZero(r.length() - 1.0),
                  "reflejar un vector unitario debe dar otro vector unitario (|r|=|d|)");
        }

        // --- Caso 4: angulo de incidencia == angulo de reflexion (caso general) ---
        // Se mide el angulo entre -d (rayo incidente, invertido para que
        // "apunte hacia la normal" como se mide convencionalmente el
        // angulo de incidencia) y n, y se compara con el angulo entre r y n.
        {
            const Vector2 d = Vector2(2.0, -3.0).normalized();
            const Vector2 n = Vector2(0.3, 1.0).normalized();
            const Vector2 r = reflect(d, n);

            const double incidenceAngle = angleBetween(-d, n);
            const double reflectionAngle = angleBetween(r, n);

            check(RayOptics::Math::isNearlyZero(incidenceAngle - reflectionAngle, 1e-9),
                  "angulo de incidencia debe ser igual al angulo de reflexion");
        }

        // --- Caso 5: invariancia ante el signo de la normal ----------------------
        {
            const Vector2 d = Vector2(1.0, -0.5).normalized();
            const Vector2 n = Vector2(-0.2, 0.7).normalized();
            const Vector2 rWithN = reflect(d, n);
            const Vector2 rWithMinusN = reflect(d, -n);
            check(rWithN.isApprox(rWithMinusN),
                  "reflect(d,n) debe ser identico a reflect(d,-n) (invariancia de signo)");
        }

        // --- Caso 6: reflectedRayFrom sobre un impacto real de circulo ----------
        // IMPORTANTE: el rayo de prueba se origina DENTRO de la cavidad
        // (distancia al centro < R), no fuera de ella. Esto es la
        // configuracion fisicamente correcta para "una circunferencia
        // altamente reflectante por su interior" (requisito, seccion 1):
        // el rayo vive dentro de la cavidad y rebota contra la pared
        // desde el lado interior. Si el rayo se originara FUERA del
        // circulo, el primer contacto se comportaria como un espejo
        // convexo visto desde afuera (formula igualmente correcta, pero
        // el rayo reflejado se alejaria para siempre, sin volver a
        // intersectar la cavidad: no es el caso de uso de este
        // simulador). Se detecto este matiz precisamente al escribir
        // este auto-test con un origen exterior y observar que el
        // segundo impacto fallaba: el fallo era la premisa del test, no
        // el codigo de reflexion.
        //
        // Verifica: (a) el rayo reflejado tiene direccion unitaria;
        // (b) su origen esta desplazado del punto de impacto por
        // aproximadamente REFLECTION_OFFSET; (c) el desplazamiento es
        // suficiente para que una nueva interseccion contra el MISMO
        // circulo SI encuentre la pared opuesta (rebote interno), sin
        // reintersectar el mismo punto por error numerico.
        {
            const Circle circle(Vector2(0.0, 0.0), 4.0);
            const Ray incidentRay(Vector2(-1.5, -1.0), Vector2(1.0, 0.5).normalized());
            const auto hit = intersectRayCircle(incidentRay, circle);

            check(hit.has_value(), "el rayo de prueba para reflectedRayFrom debe golpear el circulo");
            if (hit)
            {
                const Ray reflectedRay = reflectedRayFrom(incidentRay.direction, *hit);

                check(RayOptics::Math::isNearlyZero(reflectedRay.direction.length() - 1.0),
                      "el rayo reflejado debe tener direccion unitaria");

                const double originOffsetDistance = (reflectedRay.origin - hit->point).length();
                check(originOffsetDistance > 0.0 &&
                      originOffsetDistance < RayOptics::Physics::REFLECTION_OFFSET * 10.0,
                      "el origen del rayo reflejado debe estar desplazado una distancia pequeña del punto de impacto");

                // Segunda interseccion: el rayo reflejado, al viajar
                // dentro de la cavidad, debe golpear la pared en un
                // punto DISTINTO del primero (no debe re-detectar
                // inmediatamente el mismo punto por error numerico).
                const auto secondHit = intersectRayCircle(reflectedRay, circle);
                check(secondHit.has_value(),
                      "el rayo reflejado (originado dentro de la cavidad) debe volver a golpear la pared opuesta");
                if (secondHit)
                {
                    const double distanceBetweenHits = (secondHit->point - hit->point).length();
                    check(distanceBetweenHits > 0.01,
                          "el segundo punto de impacto debe estar claramente separado del primero");
                    check(circle.isOnSurface(secondHit->point),
                          "el segundo punto de impacto debe pertenecer a la circunferencia");

                    // Verificacion fisica cruzada: el angulo de
                    // incidencia en el primer impacto debe coincidir con
                    // el angulo de reflexion medido con la normal de ESE
                    // primer impacto (ya probado en el Caso 4 de forma
                    // sintetica; aqui se repite con datos reales de la
                    // escena para mayor confianza).
                    const double incidenceAngle = angleBetween(-incidentRay.direction, hit->normal);
                    const double reflectionAngle = angleBetween(reflectedRay.direction, hit->normal);
                    check(RayOptics::Math::isNearlyZero(incidenceAngle - reflectionAngle, 1e-6),
                          "en la escena real, angulo de incidencia == angulo de reflexion");
                }
            }
        }

        // --- Caso 7: precondicion de vectores unitarios (solo verificable en Debug) ---
#ifndef NDEBUG
        {
            bool threwOnNonUnitDirection = false;
            try
            {
                [[maybe_unused]] const Vector2 invalid = reflect(Vector2(2.0, 0.0), Vector2(1.0, 0.0));
            }
            catch (const std::domain_error&)
            {
                threwOnNonUnitDirection = true;
            }
            check(threwOnNonUnitDirection,
                  "reflect() con direccion no unitaria debe lanzar en builds Debug");
        }
#endif

        if (allPassed)
        {
            std::printf("Auto-test de reflect/reflectedRayFrom: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de reflect/reflectedRayFrom: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Physics::refract / criticalAngle (Fase 11)
    // ------------------------------------------------------------------
    void runRefractionSelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Physics::refract;
        using RayOptics::Physics::criticalAngle;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        // Angulo (en radianes) entre dos vectores unitarios, via acos.
        // Se reutiliza el mismo patron que en el auto-test de Reflection.
        const auto angleBetween = [](const Vector2& a, const Vector2& b) -> double
        {
            const double cosTheta = std::clamp(a.dot(b), -1.0, 1.0);
            return std::acos(cosTheta);
        };

        std::printf("Ejecutando auto-test de Physics::refract...\n");

        // --- Caso 1: incidencia normal, cualquier n1/n2 -> sigue recto -----------
        // A incidencia normal (theta1=0), sin(theta1)=0, asi que
        // sin(theta2)=0 tambien sin importar n1/n2: el rayo no se dobla.
        {
            const Vector2 d(1.0, 0.0);
            const Vector2 n(-1.0, 0.0);
            const auto refracted = refract(d, n, 1.0, 1.5);
            check(refracted.has_value(), "incidencia normal nunca produce TIR");
            if (refracted)
            {
                check(refracted->isApprox(d),
                      "a incidencia normal, el rayo refractado no debe desviarse");
            }
        }

        // --- Caso 2: n1 == n2 (mismo medio) -> tampoco se dobla, para cualquier angulo ---
        // Si ambos medios tienen el mismo indice, eta=1 y la formula se
        // reduce algebraicamente a t=d exactamente (verificado en la
        // derivacion del header).
        {
            const Vector2 d = Vector2(0.6, -0.8);
            const Vector2 n(0.0, 1.0);
            const auto refracted = refract(d, n, 1.33, 1.33);
            check(refracted.has_value(), "n1==n2 nunca produce TIR");
            if (refracted)
            {
                check(refracted->isApprox(d),
                      "con n1==n2, el rayo refractado debe ser identico al incidente");
            }
        }

        // --- Caso 3: 30 grados, aire -> vidrio (n1=1.0, n2=1.5) ------------------
        // Valores verificados independientemente (calculo numerico de
        // referencia): con theta1=30deg, theta2 esperado = asin(sin(30)/1.5)
        // ~= 19.47 grados.
        {
            const double theta1 = 30.0 * M_PI / 180.0;
            const Vector2 d(std::sin(theta1), -std::cos(theta1)); // viaja hacia abajo-derecha
            const Vector2 n(0.0, 1.0); // superficie horizontal, normal hacia arriba
            const auto refracted = refract(d, n, 1.0, 1.5);

            check(refracted.has_value(), "30 grados aire->vidrio no debe dar TIR (va a medio mas denso)");
            if (refracted)
            {
                check(RayOptics::Math::isNearlyZero(refracted->length() - 1.0, 1e-9),
                      "el rayo refractado debe ser unitario");

                const double theta2 = angleBetween(*refracted, -n);
                const double expectedTheta2 = std::asin(std::sin(theta1) / 1.5);
                check(RayOptics::Math::isNearlyZero(theta2 - expectedTheta2, 1e-9),
                      "theta2 debe coincidir con asin(sin(theta1)*n1/n2) = ~19.47 grados");

                // Verificacion cruzada de la propia ley de Snell con los
                // vectores resultantes, no solo con la formula escalar.
                check(RayOptics::Math::isNearlyZero(1.0 * std::sin(theta1) - 1.5 * std::sin(theta2), 1e-9),
                      "n1*sin(theta1) debe igualar n2*sin(theta2)");
            }
        }

        // --- Caso 4: justo por debajo del angulo critico, vidrio -> aire ---------
        // theta_c = asin(n2/n1) = asin(1/1.5) ~= 41.81 grados.
        {
            const auto thetaC = criticalAngle(1.5, 1.0);
            check(thetaC.has_value(), "debe existir angulo critico para n1=1.5 > n2=1.0");
            if (thetaC)
            {
                check(RayOptics::Math::isNearlyZero(*thetaC - std::asin(1.0 / 1.5), 1e-9),
                      "criticalAngle debe ser asin(n2/n1)");

                const double theta1 = *thetaC - (5.0 * M_PI / 180.0); // 5 grados por debajo
                const Vector2 d(std::sin(theta1), -std::cos(theta1));
                const Vector2 n(0.0, 1.0);
                const auto refracted = refract(d, n, 1.5, 1.0);

                check(refracted.has_value(),
                      "justo por debajo del angulo critico, SI debe haber refraccion (no TIR)");
                if (refracted)
                {
                    check(RayOptics::Math::isNearlyZero(refracted->length() - 1.0, 1e-9),
                          "el rayo refractado debe ser unitario");
                    const double theta2 = angleBetween(*refracted, -n);
                    check(RayOptics::Math::isNearlyZero(1.5 * std::sin(theta1) - 1.0 * std::sin(theta2), 1e-9),
                          "n1*sin(theta1) debe igualar n2*sin(theta2), incluso cerca del angulo critico");
                }
            }
        }

        // --- Caso 5: justo por encima del angulo critico -> TIR ------------------
        {
            const auto thetaC = criticalAngle(1.5, 1.0);
            if (thetaC)
            {
                const double theta1 = *thetaC + (5.0 * M_PI / 180.0); // 5 grados por encima
                const Vector2 d(std::sin(theta1), -std::cos(theta1));
                const Vector2 n(0.0, 1.0);
                const auto refracted = refract(d, n, 1.5, 1.0);

                check(!refracted.has_value(),
                      "justo por encima del angulo critico, DEBE producirse reflexion interna total");
            }
        }

        // --- Caso 6: invariancia ante el signo de la normal recibida -------------
        // refract() debe dar el mismo resultado sin importar si se le pasa
        // n o -n (la funcion corrige el signo internamente).
        {
            const Vector2 d = Vector2(0.4, -0.9).normalized();
            const Vector2 n = Vector2(0.1, 1.0).normalized();
            const auto refractedWithN = refract(d, n, 1.0, 1.5);
            const auto refractedWithMinusN = refract(d, -n, 1.0, 1.5);

            check(refractedWithN.has_value() && refractedWithMinusN.has_value(),
                  "ambas orientaciones de la normal deben producir refraccion valida en este caso");
            if (refractedWithN && refractedWithMinusN)
            {
                check(refractedWithN->isApprox(*refractedWithMinusN),
                      "refract(d,n) debe ser identico a refract(d,-n)");
            }
        }

        // --- Caso 7: criticalAngle no definido cuando n2 >= n1 -------------------
        {
            const auto thetaC = criticalAngle(1.0, 1.5);
            check(!thetaC.has_value(),
                  "no debe existir angulo critico cuando el segundo medio es mas denso (n2 > n1)");

            const auto thetaCEqual = criticalAngle(1.4, 1.4);
            check(!thetaCEqual.has_value(),
                  "no debe existir angulo critico cuando n1 == n2");
        }

        // --- Caso 8: indices de refraccion invalidos deben lanzar ----------------
        {
            bool threwOnZeroN1 = false;
            try
            {
                [[maybe_unused]] const auto r = refract(Vector2(1.0, 0.0), Vector2(-1.0, 0.0), 0.0, 1.5);
            }
            catch (const std::domain_error&)
            {
                threwOnZeroN1 = true;
            }
            check(threwOnZeroN1, "refract() con n1<=0 debe lanzar std::domain_error");

            bool threwOnNegativeN2 = false;
            try
            {
                [[maybe_unused]] const auto r = refract(Vector2(1.0, 0.0), Vector2(-1.0, 0.0), 1.0, -2.0);
            }
            catch (const std::domain_error&)
            {
                threwOnNegativeN2 = true;
            }
            check(threwOnNegativeN2, "refract() con n2<=0 debe lanzar std::domain_error");
        }

        if (allPassed)
        {
            std::printf("Auto-test de refract/criticalAngle: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de refract/criticalAngle: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Physics::computeCircleTrajectory (Fase 8)
    // ------------------------------------------------------------------
    void runTrajectorySelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Physics::Ray;
        using RayOptics::Geometry::Circle;
        using RayOptics::Physics::computeCircleTrajectory;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        const auto angleBetween = [](const Vector2& a, const Vector2& b) -> double
        {
            const double cosTheta = std::clamp(a.dot(b), -1.0, 1.0);
            return std::acos(cosTheta);
        };

        std::printf("Ejecutando auto-test de Physics::computeCircleTrajectory...\n");

        const Circle circle(Vector2(0.0, 0.0), 4.0);

        // --- Caso 1: maxReflections = 0 -> solo el origen -----------------------
        {
            const Ray ray(Vector2(-1.5, -1.0), Vector2(1.0, 0.5));
            const auto trajectory = computeCircleTrajectory(ray, circle, 0);
            check(trajectory.size() == 1, "maxReflections=0 debe devolver solo el origen");
            check(trajectory[0].isApprox(ray.origin), "el unico punto debe ser el origen del rayo");
        }

        // --- Caso 2: maxReflections = 1 debe coincidir con la Fase 6 -----------
        // (un solo punto de impacto, igual que intersectRayCircle directamente)
        {
            const Ray ray(Vector2(0.0, 0.0), Vector2(1.0, 0.0));
            const auto trajectory = computeCircleTrajectory(ray, circle, 1);
            check(trajectory.size() == 2, "maxReflections=1 debe devolver origen + 1 impacto");
            check(trajectory[1].isApprox(Vector2(4.0, 0.0)),
                  "el unico impacto debe coincidir con el resultado directo de la Fase 6");
        }

        // --- Caso 3: validacion fisica de una cadena larga de rebotes -----------
        // Se verifica, para CADA punto intermedio de la trayectoria, que:
        //   (a) el punto pertenece a la circunferencia;
        //   (b) el angulo de incidencia (segmento entrante) es igual al
        //       angulo de reflexion (segmento saliente), medidos ambos
        //       respecto a la normal en ese punto.
        // Esta es exactamente la validacion que pide la seccion 12 de los
        // requisitos ("el angulo incidente coincide con el reflejado"),
        // aplicada de forma generica a una cadena de N rebotes, no solo
        // al primero.
        {
            constexpr int MAX_REFLECTIONS = 8;
            const Ray ray(Vector2(-1.5, -1.0), Vector2(1.0, 0.5));
            const auto trajectory = computeCircleTrajectory(ray, circle, MAX_REFLECTIONS);

            // Con un origen estrictamente dentro de una cavidad cerrada y
            // convexa, TODO rayo en cualquier direccion siempre vuelve a
            // tocar la pared: no deberia "escaparse" nunca, asi que se
            // espera la trayectoria completa (origen + MAX_REFLECTIONS
            // impactos).
            check(trajectory.size() == static_cast<size_t>(MAX_REFLECTIONS) + 1,
                  "un origen interior a una cavidad cerrada nunca deberia hacer que la trayectoria se corte antes de tiempo");

            check(trajectory[0].isApprox(ray.origin), "trajectory[0] debe ser el origen");

            for (size_t i = 1; i < trajectory.size(); ++i)
            {
                check(circle.isOnSurface(trajectory[i]),
                      "cada punto de impacto de la cadena debe pertenecer a la circunferencia");
            }

            // Para cada punto de impacto intermedio (no el primero ni el
            // ultimo, que no tienen segmento entrante o saliente
            // respectivamente dentro de la trayectoria calculada),
            // reconstruir las direcciones entrante/saliente a partir de
            // los propios puntos y comparar angulos contra la normal.
            for (size_t i = 1; i + 1 < trajectory.size(); ++i)
            {
                const Vector2 incoming = (trajectory[i] - trajectory[i - 1]).normalized();
                const Vector2 outgoing = (trajectory[i + 1] - trajectory[i]).normalized();
                const Vector2 normal = circle.outwardNormalAt(trajectory[i]);

                const double incidenceAngle = angleBetween(-incoming, normal);
                const double reflectionAngle = angleBetween(outgoing, normal);

                check(RayOptics::Math::isNearlyZero(incidenceAngle - reflectionAngle, 1e-6),
                      "en cada rebote de la cadena, angulo de incidencia debe igualar angulo de reflexion");
            }
        }

        // --- Caso 4: maxReflections negativo debe lanzar ------------------------
        {
            bool threwOnNegative = false;
            try
            {
                const Ray ray(Vector2(0.0, 0.0), Vector2(1.0, 0.0));
                [[maybe_unused]] const auto trajectory = computeCircleTrajectory(ray, circle, -1);
            }
            catch (const std::invalid_argument&)
            {
                threwOnNegative = true;
            }
            check(threwOnNegative, "maxReflections negativo debe lanzar std::invalid_argument");
        }

        // --- Caso 5: el rayo permanece dentro de la cavidad ---------------------
        // Validacion pedida en la seccion 12 ("el rayo permanece dentro de
        // la cavidad"): cada punto de la trayectoria (incluido el origen)
        // debe estar a distancia <= R del centro (dentro o justo sobre la
        // pared, nunca fuera).
        {
            const Ray ray(Vector2(1.0, -2.0), Vector2(-0.3, 1.0));
            const auto trajectory = computeCircleTrajectory(ray, circle, 10);
            for (const Vector2& point : trajectory)
            {
                const double distanceFromCenter = (point - circle.center).length();
                check(distanceFromCenter <= circle.radius + 1e-6,
                      "todo punto de la trayectoria debe permanecer dentro (o sobre) la cavidad");
            }
        }

        if (allPassed)
        {
            std::printf("Auto-test de computeCircleTrajectory: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de computeCircleTrajectory: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Geometry::Ellipse (Fase 9)
    // ------------------------------------------------------------------
    void runEllipseSelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Geometry::Ellipse;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        std::printf("Ejecutando auto-test de Geometry::Ellipse...\n");

        // Elipse centrada en el origen, semieje mayor a=5 en x, b=3 en y.
        const Ellipse ellipse(Vector2(0.0, 0.0), 5.0, 3.0);

        check(ellipse.isOnSurface(Vector2(5.0, 0.0)),
              "(5,0) (extremo del eje mayor) debe estar sobre la elipse");
        check(ellipse.isOnSurface(Vector2(0.0, 3.0)),
              "(0,3) (extremo del eje menor) debe estar sobre la elipse");
        check(!ellipse.isOnSurface(Vector2(0.0, 0.0)),
              "el centro NO debe estar sobre la elipse");
        check(!ellipse.isOnSurface(Vector2(10.0, 0.0)),
              "(10,0), fuera de la elipse, no debe considerarse 'sobre la superficie'");

        check(ellipse.containsPoint(Vector2(0.0, 0.0)),
              "el centro debe estar DENTRO de la elipse");
        check(!ellipse.containsPoint(Vector2(10.0, 0.0)),
              "(10,0) no debe estar dentro");

        // Normal via gradiente: en los extremos de los ejes, la normal
        // debe alinearse exactamente con ese eje.
        check(ellipse.outwardNormalAt(Vector2(5.0, 0.0)).isApprox(Vector2(1.0, 0.0)),
              "normal en (5,0) (extremo del eje mayor) debe ser (1,0)");
        check(ellipse.outwardNormalAt(Vector2(-5.0, 0.0)).isApprox(Vector2(-1.0, 0.0)),
              "normal en (-5,0) debe ser (-1,0)");
        check(ellipse.outwardNormalAt(Vector2(0.0, 3.0)).isApprox(Vector2(0.0, 1.0)),
              "normal en (0,3) (extremo del eje menor) debe ser (0,1)");
        check(ellipse.outwardNormalAt(Vector2(0.0, -3.0)).isApprox(Vector2(0.0, -1.0)),
              "normal en (0,-3) debe ser (0,-1)");

        // La normal debe ser siempre unitaria, tambien en un punto
        // "generico" de la elipse (no en un extremo de eje), donde el
        // gradiente NO es trivialmente axial.
        {
            // Punto generico sobre la elipse: se parte de un angulo
            // parametrico y se ajusta a la ecuacion de la elipse
            // (x=a*cos(t), y=b*sin(t) SI satisface la ecuacion implicita
            // por construccion, para cualquier t).
            const double t = 0.7; // radianes, arbitrario
            const Vector2 genericPoint(5.0 * std::cos(t), 3.0 * std::sin(t));
            check(ellipse.isOnSurface(genericPoint),
                  "el punto parametrico (a*cos t, b*sin t) debe estar sobre la elipse");
            const Vector2 normal = ellipse.outwardNormalAt(genericPoint);
            check(RayOptics::Math::isNearlyZero(normal.length() - 1.0),
                  "la normal en un punto generico debe ser unitaria");
        }

        // Focos: c = sqrt(a^2-b^2) = sqrt(25-9) = 4, sobre el eje mayor (x).
        {
            const auto [f1, f2] = ellipse.foci();
            check(f1.isApprox(Vector2(-4.0, 0.0)) || f1.isApprox(Vector2(4.0, 0.0)),
                  "un foco debe estar en (+-4, 0)");
            check(f2.isApprox(Vector2(-4.0, 0.0)) || f2.isApprox(Vector2(4.0, 0.0)),
                  "el otro foco debe estar en (+-4, 0)");
            check(!f1.isApprox(f2), "los dos focos deben ser puntos distintos");
        }

        // Elipse con centro desplazado y eje mayor vertical (b > a), para
        // verificar que la formula no esta "harcodeada" al caso trivial
        // ni a que el eje mayor sea siempre x.
        {
            const Ellipse verticalEllipse(Vector2(2.0, -1.0), 3.0, 5.0);

            const Vector2 topPoint = verticalEllipse.center + Vector2(0.0, 5.0);
            check(verticalEllipse.isOnSurface(topPoint),
                  "extremo superior de una elipse con eje mayor vertical debe estar sobre la curva");
            check(verticalEllipse.outwardNormalAt(topPoint).isApprox(Vector2(0.0, 1.0)),
                  "normal en el extremo superior (eje mayor vertical) debe ser (0,1)");

            const auto [f1, f2] = verticalEllipse.foci();
            const double c = std::sqrt(5.0 * 5.0 - 3.0 * 3.0); // = 4
            const Vector2 expectedF1 = verticalEllipse.center - Vector2(0.0, c);
            const Vector2 expectedF2 = verticalEllipse.center + Vector2(0.0, c);
            check((f1.isApprox(expectedF1) && f2.isApprox(expectedF2)) ||
                  (f1.isApprox(expectedF2) && f2.isApprox(expectedF1)),
                  "los focos de una elipse con eje mayor vertical deben estar sobre el eje y, desplazados por el centro");
        }

        // Semiejes invalidos deben lanzar.
        bool threwOnZeroA = false;
        try
        {
            [[maybe_unused]] const Ellipse invalid(Vector2(0.0, 0.0), 0.0, 3.0);
        }
        catch (const std::domain_error&)
        {
            threwOnZeroA = true;
        }
        check(threwOnZeroA, "construir una Ellipse con a=0 debe lanzar std::domain_error");

        bool threwOnNegativeB = false;
        try
        {
            [[maybe_unused]] const Ellipse invalid(Vector2(0.0, 0.0), 5.0, -2.0);
        }
        catch (const std::domain_error&)
        {
            threwOnNegativeB = true;
        }
        check(threwOnNegativeB, "construir una Ellipse con b negativo debe lanzar std::domain_error");

        // Una elipse degenerada (a == b, es decir, un circulo) no tiene
        // focos distintos definidos: foci() debe lanzar.
        bool threwOnDegenerateFoci = false;
        try
        {
            const Ellipse circularEllipse(Vector2(0.0, 0.0), 4.0, 4.0);
            [[maybe_unused]] const auto degenerateFoci = circularEllipse.foci();
        }
        catch (const std::domain_error&)
        {
            threwOnDegenerateFoci = true;
        }
        check(threwOnDegenerateFoci,
              "foci() sobre una elipse con a==b (un circulo) debe lanzar std::domain_error");

        if (allPassed)
        {
            std::printf("Auto-test de Ellipse: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de Ellipse: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Physics::intersectRayEllipse (Fase 10)
    // ------------------------------------------------------------------
    void runEllipseIntersectionSelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Physics::Ray;
        using RayOptics::Geometry::Ellipse;
        using RayOptics::Physics::intersectRayEllipse;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        std::printf("Ejecutando auto-test de Physics::intersectRayEllipse...\n");

        const Ellipse ellipse(Vector2(0.0, 0.0), 6.0, 3.0);

        // --- Caso 1: rayo desde fuera, a lo largo del eje mayor -----------------
        {
            const Ray ray(Vector2(-20.0, 0.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayEllipse(ray, ellipse);
            check(hit.has_value(), "rayo a lo largo del eje mayor debe intersectar la elipse");
            if (hit)
            {
                check(RayOptics::Math::isNearlyZero(hit->t - 14.0),
                      "t debe ser 14 (distancia de -20 a -6, el extremo del eje mayor)");
                check(hit->point.isApprox(Vector2(-6.0, 0.0)),
                      "punto de impacto debe ser (-6,0)");
                check(hit->normal.isApprox(Vector2(-1.0, 0.0)),
                      "normal en (-6,0) debe apuntar en -x");
            }
        }

        // --- Caso 2: rayo desde el centro, a lo largo del eje menor -------------
        {
            const Ray ray(Vector2(0.0, 0.0), Vector2(0.0, 1.0));
            const auto hit = intersectRayEllipse(ray, ellipse);
            check(hit.has_value(), "rayo desde el centro a lo largo del eje menor debe intersectar");
            if (hit)
            {
                check(hit->point.isApprox(Vector2(0.0, 3.0)),
                      "punto de impacto debe ser (0,3), el extremo del eje menor");
            }
        }

        // --- Caso 3: rayo que pasa por fuera de la elipse (no la toca) ----------
        // y=10 nunca esta dentro del rango [-3,3] del eje menor.
        {
            const Ray ray(Vector2(-20.0, 10.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayEllipse(ray, ellipse);
            check(!hit.has_value(), "un rayo que pasa por encima de la elipse no debe intersectar");
        }

        // --- Caso 4: rayo tangente en el extremo del eje menor -------------------
        {
            const Ray ray(Vector2(-20.0, 3.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayEllipse(ray, ellipse);
            check(hit.has_value(), "un rayo tangente SI debe reportar interseccion (discriminante ~ 0)");
            if (hit)
            {
                check(hit->point.isApprox(Vector2(0.0, 3.0), 1e-5),
                      "el punto de tangencia debe ser (0,3)");
            }
        }

        // --- Caso 5: rayo que parte de la superficie alejandose -----------------
        // Origen (6,0) (extremo del eje mayor), direccion (+1,0) alejandose:
        // igual que en el caso analogo de Circle (Fase 6), no debe haber
        // auto-interseccion.
        {
            const Ray ray(Vector2(6.0, 0.0), Vector2(1.0, 0.0));
            const auto hit = intersectRayEllipse(ray, ellipse);
            check(!hit.has_value(),
                  "un rayo que parte de la superficie alejandose no debe reintersectar consigo mismo");
        }

        // --- Caso 6: rayo diagonal, para confirmar que no esta 'harcodeado' a ejes ---
        {
            const Vector2 origin(-10.0, -8.0);
            const Vector2 direction = Vector2(1.0, 0.6).normalized();
            const Ray ray(origin, direction);
            const auto hit = intersectRayEllipse(ray, ellipse);

            check(hit.has_value(), "rayo diagonal debe intersectar la elipse");
            if (hit)
            {
                check(ellipse.isOnSurface(hit->point),
                      "punto de impacto diagonal debe pertenecer a la elipse");
                check(RayOptics::Math::isNearlyZero(hit->normal.length() - 1.0),
                      "la normal en el impacto diagonal debe ser unitaria");
            }
        }

        // --- Caso 7: coherencia C == implicitFunction(origen) --------------------
        // Verificacion cruzada de la derivacion: el termino independiente
        // C de la cuadratica debe coincidir exactamente con evaluar la
        // ecuacion implicita de la elipse en el origen del rayo (t=0).
        {
            const Vector2 origin(-2.0, 1.0);
            const double implicitAtOrigin = ellipse.implicitFunction(origin);
            const Ray ray(origin, Vector2(1.0, 0.3));

            // Se reconstruye C manualmente con la misma formula que usa
            // intersectRayEllipse, para verificar la equivalencia.
            const Vector2 oc = origin - ellipse.center;
            const double C = (oc.x * oc.x) / (ellipse.a * ellipse.a)
                            + (oc.y * oc.y) / (ellipse.b * ellipse.b) - 1.0;

            check(RayOptics::Math::isNearlyZero(C - implicitAtOrigin),
                  "el termino C de la cuadratica debe coincidir con implicitFunction(origen)");
        }

        if (allPassed)
        {
            std::printf("Auto-test de intersectRayEllipse: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de intersectRayEllipse: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }

    // ------------------------------------------------------------------
    // Auto-test de Physics::computeEllipseTrajectory
    // ------------------------------------------------------------------
    // Análogo directo del auto-test de computeCircleTrajectory (Fase 8),
    // usando la misma validación física genérica (ángulo de incidencia ==
    // ángulo de reflexión en cada rebote), ahora sobre una elipse.
    void runEllipseTrajectorySelfTest()
    {
        using RayOptics::Math::Vector2;
        using RayOptics::Physics::Ray;
        using RayOptics::Geometry::Ellipse;
        using RayOptics::Physics::computeEllipseTrajectory;

        bool allPassed = true;
        const auto check = [&allPassed](bool condition, const char* description)
        {
            if (!condition)
            {
                allPassed = false;
                std::fprintf(stderr, "  [FALLO] %s\n", description);
            }
        };

        const auto angleBetween = [](const Vector2& a, const Vector2& b) -> double
        {
            const double cosTheta = std::clamp(a.dot(b), -1.0, 1.0);
            return std::acos(cosTheta);
        };

        std::printf("Ejecutando auto-test de Physics::computeEllipseTrajectory...\n");

        const Ellipse ellipse(Vector2(0.0, 0.0), 6.0, 3.0);

        // --- maxReflections = 0 -> solo el origen -------------------------------
        {
            const Ray ray(Vector2(0.0, 0.0), Vector2(1.0, 0.3));
            const auto trajectory = computeEllipseTrajectory(ray, ellipse, 0);
            check(trajectory.size() == 1, "maxReflections=0 debe devolver solo el origen");
        }

        // --- cadena larga: validacion fisica generica en cada rebote ------------
        {
            constexpr int MAX_REFLECTIONS = 10;
            const Ray ray(Vector2(1.0, -0.5), Vector2(1.0, 0.3));
            const auto trajectory = computeEllipseTrajectory(ray, ellipse, MAX_REFLECTIONS);

            check(trajectory.size() == static_cast<size_t>(MAX_REFLECTIONS) + 1,
                  "un origen interior a la elipse nunca deberia hacer que la trayectoria se corte antes de tiempo");

            for (size_t i = 1; i < trajectory.size(); ++i)
            {
                check(ellipse.isOnSurface(trajectory[i]),
                      "cada punto de impacto debe pertenecer a la elipse");
            }

            for (size_t i = 1; i + 1 < trajectory.size(); ++i)
            {
                const Vector2 incoming = (trajectory[i] - trajectory[i - 1]).normalized();
                const Vector2 outgoing = (trajectory[i + 1] - trajectory[i]).normalized();
                const Vector2 normal = ellipse.outwardNormalAt(trajectory[i]);

                const double incidenceAngle = angleBetween(-incoming, normal);
                const double reflectionAngle = angleBetween(outgoing, normal);

                check(RayOptics::Math::isNearlyZero(incidenceAngle - reflectionAngle, 1e-6),
                      "en cada rebote dentro de la elipse, angulo de incidencia debe igualar angulo de reflexion");
            }
        }

        if (allPassed)
        {
            std::printf("Auto-test de computeEllipseTrajectory: TODAS las pruebas pasaron.\n");
        }
        else
        {
            std::printf("Auto-test de computeEllipseTrajectory: HAY PRUEBAS FALLIDAS (ver arriba).\n");
        }
    }
} // namespace

int main()
{
    // ------------------------------------------------------------------
    // 0) Auto-test de Vector2 (Fase 2)
    // ------------------------------------------------------------------
    runVector2SelfTest();
    runRaySelfTest();
    runCircleSelfTest();
    runIntersectionSelfTest();
    runReflectionSelfTest();
    runRefractionSelfTest();
    runTrajectorySelfTest();
    runEllipseSelfTest();
    runEllipseIntersectionSelfTest();
    runEllipseTrajectorySelfTest();

    // ------------------------------------------------------------------
    // 1) Inicializar GLFW
    // ------------------------------------------------------------------
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit())
    {
        std::fprintf(stderr, "Error fatal: no se pudo inicializar GLFW.\n");
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------------
    // 2) Configurar el contexto OpenGL 3.3 Core Profile
    // ------------------------------------------------------------------
    // Se pide explícitamente la versión 3.3 y el perfil "Core" (sin
    // funciones fijas como glBegin/glEnd/glVertex, tal como exigen los
    // requisitos del proyecto). Esto obliga a usar VAO/VBO/shaders desde
    // el principio, lo cual se implementará en la Fase 4.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    // macOS requiere este hint adicional para poder crear contextos Core
    // Profile modernos. No afecta a Linux/Windows, pero se deja preparado
    // por si el proyecto se compila en Mac en el futuro.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    // ------------------------------------------------------------------
    // 3) Crear la ventana
    // ------------------------------------------------------------------
    GLFWwindow* window = glfwCreateWindow(
        AppConfig::WINDOW_WIDTH,
        AppConfig::WINDOW_HEIGHT,
        AppConfig::WINDOW_TITLE,
        nullptr,  // monitor: nullptr = modo ventana, no pantalla completa
        nullptr   // share: nullptr = no compartir contexto con otra ventana
    );

    if (window == nullptr)
    {
        std::fprintf(stderr,
            "Error fatal: no se pudo crear la ventana GLFW.\n"
            "Posibles causas:\n"
            "  - El driver grafico no soporta OpenGL 3.3 Core Profile.\n"
            "  - Se esta ejecutando en un entorno sin servidor grafico "
            "(por ejemplo, una terminal remota sin X11/Wayland).\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);

    // Activar V-Sync (sincronismo vertical). Esto limita la tasa de
    // fotogramas a la de refresco del monitor y evita "tearing" visual,
    // además de evitar que el bucle de render consuma 100% de CPU/GPU
    // sin necesidad, algo relevante porque en fases futuras aquí vivirá
    // la animación del rayo (Fase 13 / sección 11).
    glfwSwapInterval(1);

    // ------------------------------------------------------------------
    // 4) Cargar OpenGL con GLAD
    // ------------------------------------------------------------------
    // GLAD necesita la dirección de las funciones de OpenGL, que se
    // obtiene a través de GLFW (glfwGetProcAddress). Esto debe hacerse
    // DESPUÉS de crear el contexto (glfwMakeContextCurrent) y ANTES de
    // llamar a cualquier función gl*().
    int gladVersion = gladLoadGL(glfwGetProcAddress);
    if (gladVersion == 0)
    {
        std::fprintf(stderr, "Error fatal: GLAD no pudo cargar OpenGL.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::printf("OpenGL cargado correctamente: version %d.%d\n",
                GLAD_VERSION_MAJOR(gladVersion),
                GLAD_VERSION_MINOR(gladVersion));
    std::printf("Vendor  : %s\n", glGetString(GL_VENDOR));
    std::printf("Renderer: %s\n", glGetString(GL_RENDERER));
    std::printf("GL Ver. : %s\n", glGetString(GL_VERSION));

    // ------------------------------------------------------------------
    // 5) Configurar el viewport inicial y el callback de redimensionado
    // ------------------------------------------------------------------
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // AppState vive en el stack de main() y sobrevive tanto como la
    // propia ventana, así que es seguro colgarle un puntero a GLFW: el
    // callback nunca se invocará después de que 'appState' se destruya
    // (la ventana se destruye antes de que main() retorne).
    AppState appState;
    glfwSetWindowUserPointer(window, &appState);
    glfwSetKeyCallback(window, keyCallback);
    std::printf("Controles: TAB para alternar Circulo/Elipse, FLECHAS IZQ/DER para girar el rayo, ESC para salir.\n");

    // ------------------------------------------------------------------
    // 5.5) Crear el Renderer (Fase 4)
    // ------------------------------------------------------------------
    // El Shader::Shader() puede lanzar std::runtime_error si el GLSL no
    // compila o no enlaza. Se captura aquí, en vez de dejar que una
    // excepción no capturada termine el programa de forma abrupta (y con
    // un mensaje de terminate() poco útil para el usuario), para poder
    // liberar la ventana de GLFW correctamente e informar con claridad
    // qué archivo de shader falló y por qué.
    //
    // Las rutas son relativas ("shaders/line.vert"): CMake copia la
    // carpeta shaders/ junto al ejecutable en cada build (ver
    // CMakeLists.txt, POST_BUILD), y también existe en la raíz del
    // proyecto. Por eso funciona tanto si ejecutas desde la raíz del
    // proyecto (./build/RayOptics) como desde dentro de build/
    // (./RayOptics).
    std::unique_ptr<RayOptics::Rendering::Renderer> renderer;
    try
    {
        renderer = std::make_unique<RayOptics::Rendering::Renderer>(
            "shaders/line.vert", "shaders/line.frag");
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Error fatal al crear el Renderer: %s\n", ex.what());
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const double aspectRatio =
        static_cast<double>(framebufferWidth) / static_cast<double>(framebufferHeight);
    renderer->setCamera(
        RayOptics::Math::Vector2(0.0, 0.0),
        AppConfig::CAMERA_HALF_HEIGHT * aspectRatio,
        AppConfig::CAMERA_HALF_HEIGHT
    );

    // ------------------------------------------------------------------
    // Geometría de prueba para esta fase (Fase 5): la cavidad circular
    // reflectante. Los parámetros son, por ahora, fijos (se harán
    // configurables interactivamente en la Fase 13). Un radio de 4, con
    // la cámara actual (semi-altura 5), deja margen visible alrededor.
    // ------------------------------------------------------------------
    const RayOptics::Geometry::Circle testCircle(
        RayOptics::Math::Vector2(0.0, 0.0), 4.0);

    // ------------------------------------------------------------------
    // Rayo de prueba para esta fase: se origina DENTRO de la cavidad
    // (distancia al centro < R), que es la configuración físicamente
    // correcta para "una circunferencia altamente reflectante por su
    // interior" (requisito, sección 1): el rayo vive dentro de la cavidad
    // y golpea la pared desde el lado interior. Si se originara fuera del
    // círculo, la reflexión seguiría siendo matemáticamente correcta,
    // pero se comportaría como un espejo convexo visto desde afuera (el
    // rayo reflejado se alejaría para siempre, sin volver a intersectar
    // la cavidad) — no es el caso de uso de este simulador.
    // ------------------------------------------------------------------
    // ------------------------------------------------------------------
    // Interactividad: el ORIGEN del rayo queda fijo, pero su ÁNGULO de
    // incidencia (dirección) ahora es una variable que el usuario puede
    // modificar en tiempo real con las flechas IZQUIERDA/DERECHA (ver el
    // bucle de render más abajo). Recalcular la trayectoria completa cada
    // vez que cambia el ángulo es computacionalmente trivial: son, como
    // mucho, DEMO_MAX_REFLECTIONS intersecciones analíticas (fórmulas
    // cerradas, sin iteración), del orden de microsegundos incluso en
    // hardware modesto — se puede recalcular en cada frame sin ningún
    // problema de rendimiento.
    // ------------------------------------------------------------------
    const RayOptics::Math::Vector2 circleRayOrigin(-1.5, -1.0);
    double circleRayAngle = std::atan2(0.5, 1.0); // ángulo inicial (mismo que antes)

    constexpr int DEMO_MAX_REFLECTIONS = 12;

    // Estos ya NO son 'const': se recalculan cada vez que el ángulo
    // cambia (ver bucle de render).
    std::vector<RayOptics::Math::Vector2> trajectoryPoints =
        RayOptics::Physics::computeCircleTrajectory(
            RayOptics::Physics::Ray(circleRayOrigin,
                RayOptics::Math::Vector2(std::cos(circleRayAngle), std::sin(circleRayAngle))),
            testCircle, DEMO_MAX_REFLECTIONS);

    // Verificación en consola con los parámetros iniciales de esta demo
    // (complementa el auto-test genérico de arriba, que usa casos
    // sintéticos independientes de la escena): confirma cuántos puntos de
    // impacto se calcularon y que todos pertenecen a la circunferencia.
    std::printf("Trayectoria de demo: %zu puntos (origen + hasta %d reflexiones)\n",
                trajectoryPoints.size(), DEMO_MAX_REFLECTIONS);
    for (size_t i = 1; i < trajectoryPoints.size(); ++i)
    {
        const bool onSurface = testCircle.isOnSurface(trajectoryPoints[i]);
        std::printf("  Rebote %zu: punto=(%.4f, %.4f) %s\n",
                    i, trajectoryPoints[i].x, trajectoryPoints[i].y,
                    onSurface ? "[sobre la circunferencia, OK]" : "[FUERA DE LA CIRCUNFERENCIA - ERROR]");
    }

    // La circunferencia no tiene una representación nativa como polilínea
    // (es una curva continua, no una lista de segmentos): para dibujarla
    // con el mismo Renderer genérico de líneas se la muestrea en N puntos
    // igualmente espaciados en ángulo y se cierra el contorno repitiendo
    // el primer punto al final. Esta función es puramente de
    // VISUALIZACIÓN (vive en main.cpp, no en Geometry::Circle, que no
    // debe saber nada de "cuántos segmentos usar para dibujarse": esa es
    // una decisión del renderer/aplicación, no una propiedad geométrica
    // de la circunferencia).
    const auto sampleCircleOutline =
        [](const RayOptics::Geometry::Circle& circle, int segments)
        -> std::vector<RayOptics::Math::Vector2>
    {
        std::vector<RayOptics::Math::Vector2> outline;
        outline.reserve(static_cast<size_t>(segments) + 1);
        for (int i = 0; i <= segments; ++i)
        {
            const double angle = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(segments);
            outline.emplace_back(
                circle.center.x + circle.radius * std::cos(angle),
                circle.center.y + circle.radius * std::sin(angle)
            );
        }
        return outline;
    };

    constexpr int CIRCLE_OUTLINE_SEGMENTS = 128;
    const std::vector<RayOptics::Math::Vector2> circleOutline =
        sampleCircleOutline(testCircle, CIRCLE_OUTLINE_SEGMENTS);

    // ------------------------------------------------------------------
    // Fase 9: geometría de la elipse, SOLO para confirmación visual de la
    // forma en sí misma (todavía no hay intersección rayo-elipse: eso es
    // la Fase 10). Se posiciona con semiejes distintos entre sí y
    // distintos del radio del círculo, precisamente para comprobar
    // visualmente que la curva NO es un círculo disfrazado.
    // ------------------------------------------------------------------
    const RayOptics::Geometry::Ellipse testEllipse(
        RayOptics::Math::Vector2(0.0, 0.0), 6.0, 2.5);

    // Muestreo análogo al de sampleCircleOutline, pero parametrizado con
    // x = cx + a*cos(t), y = cy + b*sin(t): la parametrización estándar
    // de una elipse, que por construcción satisface siempre la ecuación
    // implícita (se puede verificar sustituyendo: cos^2+sin^2=1).
    const auto sampleEllipseOutline =
        [](const RayOptics::Geometry::Ellipse& ellipse, int segments)
        -> std::vector<RayOptics::Math::Vector2>
    {
        std::vector<RayOptics::Math::Vector2> outline;
        outline.reserve(static_cast<size_t>(segments) + 1);
        for (int i = 0; i <= segments; ++i)
        {
            const double t = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(segments);
            outline.emplace_back(
                ellipse.center.x + ellipse.a * std::cos(t),
                ellipse.center.y + ellipse.b * std::sin(t)
            );
        }
        return outline;
    };

    constexpr int ELLIPSE_OUTLINE_SEGMENTS = 128;
    const std::vector<RayOptics::Math::Vector2> ellipseOutline =
        sampleEllipseOutline(testEllipse, ELLIPSE_OUTLINE_SEGMENTS);

    // Los dos focos de la elipse, dibujados como cruces pequeñas (cada
    // brazo de la cruz es, en sí mismo, una polilínea de 2 puntos,
    // aprovechando de nuevo el mismo Renderer genérico de líneas).
    const auto [ellipseFocus1, ellipseFocus2] = testEllipse.foci();
    constexpr double FOCUS_MARKER_HALF_SIZE = 0.15;
    const auto focusMarkerHorizontal =
        [](const RayOptics::Math::Vector2& p) -> std::vector<RayOptics::Math::Vector2>
    {
        return {
            RayOptics::Math::Vector2(p.x - FOCUS_MARKER_HALF_SIZE, p.y),
            RayOptics::Math::Vector2(p.x + FOCUS_MARKER_HALF_SIZE, p.y)
        };
    };
    const auto focusMarkerVertical =
        [](const RayOptics::Math::Vector2& p) -> std::vector<RayOptics::Math::Vector2>
    {
        return {
            RayOptics::Math::Vector2(p.x, p.y - FOCUS_MARKER_HALF_SIZE),
            RayOptics::Math::Vector2(p.x, p.y + FOCUS_MARKER_HALF_SIZE)
        };
    };
    const std::vector<RayOptics::Math::Vector2> focus1MarkerH = focusMarkerHorizontal(ellipseFocus1);
    const std::vector<RayOptics::Math::Vector2> focus1MarkerV = focusMarkerVertical(ellipseFocus1);
    const std::vector<RayOptics::Math::Vector2> focus2MarkerH = focusMarkerHorizontal(ellipseFocus2);
    const std::vector<RayOptics::Math::Vector2> focus2MarkerV = focusMarkerVertical(ellipseFocus2);

    // ------------------------------------------------------------------
    // Rayo de prueba DENTRO de la elipse: se calcula su trayectoria
    // completa con computeEllipseTrajectory, exactamente igual que para
    // el círculo (Fase 8). Esto sustituye la demo de "un solo rebote" que
    // se usó al introducir intersectRayEllipse: ahora que existe
    // computeEllipseTrajectory, generalizarla a N rebotes es automático
    // gracias a que reflectedRayFrom() es una plantilla que funciona
    // igual de bien con RayEllipseHit que con RayCircleHit. La propiedad
    // focal (un rayo desde un foco se refleja hacia el otro) se deja como
    // validación formal para la Fase 12.
    // ------------------------------------------------------------------
    const RayOptics::Math::Vector2 ellipseRayOrigin(0.0, 0.0);
    double ellipseRayAngle = std::atan2(0.3, 1.0); // ángulo inicial (mismo que antes)

    std::vector<RayOptics::Math::Vector2> ellipseTrajectoryPoints =
        RayOptics::Physics::computeEllipseTrajectory(
            RayOptics::Physics::Ray(ellipseRayOrigin,
                RayOptics::Math::Vector2(std::cos(ellipseRayAngle), std::sin(ellipseRayAngle))),
            testEllipse, DEMO_MAX_REFLECTIONS);

    std::printf("Trayectoria de la elipse: %zu puntos (origen + hasta %d reflexiones)\n",
                ellipseTrajectoryPoints.size(), DEMO_MAX_REFLECTIONS);

    // ------------------------------------------------------------------
    // Longitud total de cada trayectoria (suma de las longitudes de sus
    // segmentos), necesaria para animar el punto brillante que la
    // recorre en bucle: se avanza una distancia en unidades de mundo por
    // segundo y se envuelve (módulo) por esta longitud total, así el
    // recorrido vuelve suavemente al principio en vez de "saltar".
    // ------------------------------------------------------------------
    const auto totalPathLength =
        [](const std::vector<RayOptics::Math::Vector2>& path) -> double
    {
        double total = 0.0;
        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
            total += (path[i + 1] - path[i]).length();
        }
        return total;
    };

    // ------------------------------------------------------------------
    // tracedPrefixOfPath: dado un camino (polilínea) y una distancia
    // recorrida a lo largo de él (ya envuelta al rango [0, longitud
    // total)), devuelve el PREFIJO de esa polilínea hasta ese punto —
    // es decir, "cuánto del rayo ya se ha trazado". Esto sustituye al
    // marcador puntual (que se veía como un cuadrado poco elegante,
    // artefacto de cómo OpenGL rasteriza GL_POINTS): en vez de un punto
    // suelto, se dibuja el rayo real avanzando y reflejándose, como si la
    // luz recorriera la cavidad en vivo. Es interpolación lineal simple
    // segmento a segmento: NO es física nueva, por eso vive en main.cpp
    // como utilidad de animación y no en Physics.
    // ------------------------------------------------------------------
    const auto tracedPrefixOfPath =
        [](const std::vector<RayOptics::Math::Vector2>& path, double distance)
        -> std::vector<RayOptics::Math::Vector2>
    {
        std::vector<RayOptics::Math::Vector2> prefix;
        if (path.empty())
        {
            return prefix;
        }

        prefix.push_back(path[0]);
        double remaining = distance;
        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
            const double segmentLength = (path[i + 1] - path[i]).length();
            if (remaining < segmentLength)
            {
                const double t = segmentLength > RayOptics::Math::EPSILON
                    ? std::clamp(remaining / segmentLength, 0.0, 1.0)
                    : 0.0;
                prefix.push_back(path[i] + (path[i + 1] - path[i]) * t);
                return prefix;
            }
            prefix.push_back(path[i + 1]);
            remaining -= segmentLength;
        }
        // 'distance' llegó (o superó ligeramente, por redondeo) la
        // longitud total: se devuelve el camino completo.
        return prefix;
    };

    double circleTrajectoryLength = totalPathLength(trajectoryPoints);
    double ellipseTrajectoryLength = totalPathLength(ellipseTrajectoryPoints);

    // Velocidad a la que se traza el rayo, en unidades de mundo por
    // segundo. Es un parámetro puramente visual (no físico).
    constexpr double TRACE_SPEED = 6.0;

    // Velocidad de rotación del ángulo de incidencia al mantener
    // presionadas las flechas IZQUIERDA/DERECHA, en radianes por segundo.
    constexpr double ROTATION_SPEED = 1.2;

    // ------------------------------------------------------------------
    // Ayuda para pruebas automatizadas / CI (no forma parte de la
    // experiencia de usuario final): si la variable de entorno
    // RAYOPTICS_AUTOCLOSE_FRAMES está definida con un entero positivo,
    // la ventana se cierra sola tras dibujar ese número de frames. Esto
    // permite verificar en un pipeline sin intervención humana (por
    // ejemplo, bajo Xvfb) que la ventana se crea, el contexto OpenGL se
    // inicializa y el render no falla, sin necesitar cerrar la ventana
    // manualmente.
    int autoCloseFrames = -1;
    if (const char* envValue = std::getenv("RAYOPTICS_AUTOCLOSE_FRAMES"))
    {
        autoCloseFrames = std::atoi(envValue);
    }
    int frameCounter = 0;

    // deltaTime entre frames, necesario para que la velocidad de rotación
    // (ROTATION_SPEED, en radianes/segundo) sea independiente de la tasa
    // de fotogramas: sin esto, la rotación sería más rápida en una
    // máquina con más FPS, lo cual sería incorrecto.
    double lastFrameTime = glfwGetTime();

    // ------------------------------------------------------------------
    // 6) Bucle principal de render
    // ------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        const double currentFrameTime = glfwGetTime();
        const double deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // --- Entrada ---
        // ESC y TAB se manejan en keyCallback() (eventos puntuales). Las
        // flechas IZQUIERDA/DERECHA, en cambio, se SONDEAN aquí cada
        // frame con glfwGetKey(): a diferencia de TAB, mientras el
        // usuario mantenga la flecha presionada se espera que el ángulo
        // siga girando de forma continua y suave, no que "salte" una vez
        // por pulsación. Sondear es el patrón correcto de GLFW para
        // "mientras se mantenga presionada esta tecla, sigue haciendo X".
        const bool showingCircle = (appState.currentScene == Scene::Circle);
        double& activeAngle = showingCircle ? circleRayAngle : ellipseRayAngle;

        bool angleChanged = false;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            activeAngle -= ROTATION_SPEED * deltaTime;
            angleChanged = true;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            activeAngle += ROTATION_SPEED * deltaTime;
            angleChanged = true;
        }

        if (angleChanged)
        {
            // Recalcular la trayectoria completa de la escena activa con
            // el nuevo ángulo. El origen NO cambia, solo la dirección
            // (esto es exactamente "modificar el ángulo de incidencia"
            // manteniendo fija la posición inicial del rayo).
            const RayOptics::Math::Vector2 newDirection(
                std::cos(activeAngle), std::sin(activeAngle));

            if (showingCircle)
            {
                trajectoryPoints = RayOptics::Physics::computeCircleTrajectory(
                    RayOptics::Physics::Ray(circleRayOrigin, newDirection),
                    testCircle, DEMO_MAX_REFLECTIONS);
                circleTrajectoryLength = totalPathLength(trajectoryPoints);
            }
            else
            {
                ellipseTrajectoryPoints = RayOptics::Physics::computeEllipseTrajectory(
                    RayOptics::Physics::Ray(ellipseRayOrigin, newDirection),
                    testEllipse, DEMO_MAX_REFLECTIONS);
                ellipseTrajectoryLength = totalPathLength(ellipseTrajectoryPoints);
            }
        }

        // --- Render ---
        glClearColor(
            AppConfig::BACKGROUND_R,
            AppConfig::BACKGROUND_G,
            AppConfig::BACKGROUND_B,
            AppConfig::BACKGROUND_A
        );
        glClear(GL_COLOR_BUFFER_BIT);

        // ---- Selección de escena dinámica (TAB) --------------------------------
        // Solo se dibuja la geometría y trayectoria de la escena activa
        // en 'appState.currentScene'.
        const std::vector<RayOptics::Math::Vector2>& activeTrajectory =
            showingCircle ? trajectoryPoints : ellipseTrajectoryPoints;
        const double activeTrajectoryLength =
            showingCircle ? circleTrajectoryLength : ellipseTrajectoryLength;

        if (showingCircle)
        {
            // Cavidad circular reflectante, en un celeste tenue (color
            // "superficie óptica" pedido en la sección 14 de requisitos
            // de visualización).
            renderer->drawPolyline(circleOutline, 0.30f, 0.70f, 0.95f);
        }
        else
        {
            // Elipse, en magenta tenue, con sus dos focos marcados.
            renderer->drawPolyline(ellipseOutline, 0.85f, 0.40f, 0.75f);
            renderer->drawPolyline(focus1MarkerH, 0.85f, 0.40f, 0.75f);
            renderer->drawPolyline(focus1MarkerV, 0.85f, 0.40f, 0.75f);
            renderer->drawPolyline(focus2MarkerH, 0.85f, 0.40f, 0.75f);
            renderer->drawPolyline(focus2MarkerV, 0.85f, 0.40f, 0.75f);
        }

        // Trayectoria completa (guía tenue, gris azulado): muestra de un
        // vistazo el patrón completo de reflexiones, sin importar cuánto
        // se haya "trazado" ya el rayo animado sobre ella.
        renderer->drawPolyline(activeTrajectory, 0.42f, 0.42f, 0.48f);

        // ---- Trazado progresivo del rayo (sustituye al punto animado) ---------
        // En vez de un marcador puntual suelto (que se veía como un
        // cuadrado, un artefacto de cómo OpenGL rasteriza GL_POINTS sin
        // un shader de puntos dedicado), se dibuja el PREFIJO ya recorrido
        // de la trayectoria en un color brillante, avanzando con el
        // tiempo: da la sensación de que el rayo de luz viaja y se
        // refleja en vivo, en vez de solo mostrar "dónde está" un punto.
        // Al llegar al final, vuelve a empezar (fmod), en bucle.
        if (activeTrajectoryLength > RayOptics::Math::EPSILON)
        {
            const double distance = std::fmod(currentFrameTime * TRACE_SPEED, activeTrajectoryLength);
            const std::vector<RayOptics::Math::Vector2> tracedPrefix =
                tracedPrefixOfPath(activeTrajectory, distance);
            renderer->drawPolyline(tracedPrefix, 1.0f, 0.95f, 0.55f);
        }

        // --- Intercambio de buffers y eventos ---
        glfwSwapBuffers(window);
        glfwPollEvents();

        ++frameCounter;
        if (autoCloseFrames >= 0 && frameCounter >= autoCloseFrames)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    // ------------------------------------------------------------------
    // 7) Limpieza de recursos
    // ------------------------------------------------------------------
    // ORDEN CRÍTICO: 'renderer' posee un VAO y un VBO (recursos de la GPU
    // asociados al contexto OpenGL actual). Debe destruirse ANTES de
    // glfwDestroyWindow()/glfwTerminate(), porque estas últimas destruyen
    // el contexto OpenGL en el que esos recursos viven. Si se invierte el
    // orden, el destructor de Renderer llamaría a glDeleteBuffers /
    // glDeleteVertexArrays sobre un contexto ya inválido, lo cual es
    // comportamiento indefinido (en la práctica: un segmentation fault,
    // como se detectó al probar esta fase bajo Xvfb + llvmpipe).
    //
    // 'renderer' es un std::unique_ptr, así que basta con resetearlo
    // explícitamente aquí para forzar su destrucción en el momento
    // correcto, en vez de esperar a que salga de ámbito al final de
    // main() (que sería DESPUÉS de glfwTerminate()).
    renderer.reset();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
