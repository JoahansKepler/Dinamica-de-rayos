#pragma once
// ============================================================================
// RayOptics - Physics::computeCircleTrajectory / computeEllipseTrajectory
// ----------------------------------------------------------------------------
// Generaliza la Fase 7 (un único rebote) a una cadena de N reflexiones,
// produciendo exactamente la estructura de datos que pide la sección 9 de
// los requisitos:
//
//     std::vector<Vector2> trajectory;
//
// con trajectory[0] = origen del rayo inicial, y trajectory[i] (i >= 1) el
// i-ésimo punto de incidencia sobre la superficie.
//
// Decisión de diseño: esta función vive en Physics (no en main.cpp).
//   Aunque main.cpp podría en principio tener un bucle "for" que llame a
//   intersectRayCircle/intersectRayEllipse + reflectedRayFrom repetidamente,
//   encapsularlo aquí:
//     1. Hace que la operación "calcular la trayectoria completa de un
//        rayo en una cavidad" sea una unidad con su propio contrato
//        (parámetros, valor de retorno, casos límite) reutilizable desde
//        cualquier punto de la aplicación (por ejemplo, al recalcular la
//        trayectoria cada vez que el usuario mueve un control, o —como se
//        añade ahora— al alternar entre la escena del círculo y la de la
//        elipse con una tecla).
//     2. Es trivialmente testeable de forma aislada (ver el auto-test),
//        sin necesitar contexto de OpenGL ni de la aplicación.
//     3. Mantiene a main.cpp centrado en orquestar la aplicación (ventana,
//        entrada, dibujo), no en algoritmos de física.
//
// Decisión de diseño (añadida al incorporar la elipse): la función
// genérica computeTrajectory<Shape>() recibe la función de intersección
// como parámetro (intersectFn), en vez de duplicar el bucle una vez para
// Circle y otra para Ellipse. El bucle "intersectar, avanzar, reflejar,
// repetir" es idéntico para cualquier geometría: lo único que cambia es
// CÓMO se calcula la intersección con esa geometría concreta. Esto es
// inyección de dependencias vía plantilla: computeTrajectory no sabe (ni
// necesita saber) si 'shape' es un círculo o una elipse.
// computeCircleTrajectory() y computeEllipseTrajectory() son wrappers
// finos que fijan qué función de intersección usar, preservando la
// interfaz pública ya existente y probada desde la Fase 8.
// ============================================================================

#include "Math/Vector2.h"
#include "Physics/Ray.h"
#include "Physics/Intersection.h"
#include "Physics/Reflection.h"
#include "Geometry/Circle.h"
#include "Geometry/Ellipse.h"

#include <stdexcept>
#include <vector>

namespace RayOptics::Physics
{

// ----------------------------------------------------------------------------
// computeTrajectory (genérica)
//
// 'intersectFn' debe ser invocable como intersectFn(ray, shape) y devolver
// algo contextualmente booleano con miembros '.point' y '.normal' cuando
// hay impacto (es decir, un std::optional<RayCircleHit> o
// std::optional<RayEllipseHit>): exactamente lo que ya devuelven
// intersectRayCircle e intersectRayEllipse.
//
// Comportamiento (idéntico al de la Fase 8, ahora generalizado):
//   - trajectory[0] es siempre initialRay.origin.
//   - Si en algún punto 'intersectFn' no encuentra intersección, la
//     trayectoria se corta ahí (ver Fase 7: caso del "espejo convexo
//     visto desde afuera").
//   - Como máximo 'maxReflections' impactos, así que
//     trajectory.size() <= maxReflections + 1.
// ----------------------------------------------------------------------------
template <typename Shape, typename IntersectFn>
[[nodiscard]] inline std::vector<Math::Vector2> computeTrajectory(
    const Ray& initialRay,
    const Shape& shape,
    int maxReflections,
    IntersectFn&& intersectFn)
{
    if (maxReflections < 0)
    {
        throw std::invalid_argument(
            "computeTrajectory: maxReflections no puede ser negativo.");
    }

    std::vector<Math::Vector2> trajectory;
    trajectory.reserve(static_cast<size_t>(maxReflections) + 1);
    trajectory.push_back(initialRay.origin);

    Ray currentRay = initialRay;
    for (int bounce = 0; bounce < maxReflections; ++bounce)
    {
        const auto hit = intersectFn(currentRay, shape);
        if (!hit)
        {
            break;
        }

        trajectory.push_back(hit->point);
        currentRay = reflectedRayFrom(currentRay.direction, *hit);
    }

    return trajectory;
}

// ----------------------------------------------------------------------------
// computeCircleTrajectory (Fase 8) — wrapper sobre computeTrajectory
// ----------------------------------------------------------------------------
[[nodiscard]] inline std::vector<Math::Vector2> computeCircleTrajectory(
    const Ray& initialRay,
    const Geometry::Circle& circle,
    int maxReflections)
{
    return computeTrajectory(initialRay, circle, maxReflections,
        [](const Ray& r, const Geometry::Circle& c) { return intersectRayCircle(r, c); });
}

// ----------------------------------------------------------------------------
// computeEllipseTrajectory — wrapper sobre computeTrajectory
// ----------------------------------------------------------------------------
[[nodiscard]] inline std::vector<Math::Vector2> computeEllipseTrajectory(
    const Ray& initialRay,
    const Geometry::Ellipse& ellipse,
    int maxReflections)
{
    return computeTrajectory(initialRay, ellipse, maxReflections,
        [](const Ray& r, const Geometry::Ellipse& e) { return intersectRayEllipse(r, e); });
}

} // namespace RayOptics::Physics
