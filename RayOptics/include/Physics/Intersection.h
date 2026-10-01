#pragma once
// ============================================================================
// RayOptics - Physics::Intersection (rayo-circunferencia y rayo-elipse)
// ----------------------------------------------------------------------------
// Calcula la intersección analítica entre un Ray (P(t) = P0 + t*d, con d
// unitario) y dos tipos de geometría óptica: Geometry::Circle (Fase 6) y
// Geometry::Ellipse (Fase 10, ver segunda mitad de este archivo).
//
// ----------------------------------------------------------------------------
// DERIVACIÓN MATEMÁTICA — CIRCUNFERENCIA (requisito: no es una
// aproximación, es exacta)
// ----------------------------------------------------------------------------
// Sea oc = P0 - C (vector del centro de la circunferencia al origen del
// rayo). La ecuación de la circunferencia es:
//
//     |P - C|^2 = R^2
//
// Sustituyendo P = P0 + t*d:
//
//     |oc + t*d|^2 = R^2
//     (oc + t*d)·(oc + t*d) = R^2
//     (d·d) t^2 + 2(oc·d) t + (oc·oc - R^2) = 0
//
// Esta es una ecuación cuadrática en t: a*t^2 + b*t + c = 0, con:
//
//     a = d·d
//     b = 2 (oc·d)
//     c = oc·oc - R^2
//
// IMPORTANTE: como Ray garantiza como invariante de clase que 'direction'
// es siempre unitario (ver Physics/Ray.h), se tiene d·d = 1 SIEMPRE, así
// que a = 1 exactamente. Esto no es una aproximación ni un caso especial:
// es una simplificación algebraica válida que además ahorra una división
// en la fórmula cuadrática (t = (-b ± sqrt(b^2-4ac)) / 2a se reduce a
// t = -oc·d ± sqrt((oc·d)^2 - c)).
//
// El discriminante de la cuadrática (con a=1) es:
//
//     disc = b^2 - 4c = 4[(oc·d)^2 - (oc·oc - R^2)]
//
// y las raíces:
//
//     t = -(oc·d) ± sqrt((oc·d)^2 - (oc·oc - R^2))
//
// Casos:
//   - disc < 0            -> el rayo no toca la circunferencia (recta secante
//                             que en realidad pasa fuera del círculo).
//   - disc ≈ 0 (tangente)  -> una única raíz real (con multiplicidad 2).
//   - disc > 0             -> dos raíces reales; el rayo, como semirrecta
//                             (t >= 0), puede intersectar en 0, 1 o 2 puntos
//                             de la circunferencia dependiendo de los signos
//                             de esas raíces.
//
// De las raíces válidas (t > epsilon, es decir, estrictamente por delante
// del origen del rayo, según pide el requisito), se elige la MENOR: es la
// intersección físicamente relevante, la primera superficie que el rayo
// encuentra en su trayectoria.
// ============================================================================

#include "Math/Vector2.h"
#include "Physics/Ray.h"
#include "Geometry/Circle.h"
#include "Geometry/Ellipse.h"

#include <cmath>
#include <optional>

namespace RayOptics::Physics
{

// ----------------------------------------------------------------------------
// Resultado de una intersección exitosa.
//
// Se agrupan en una sola estructura el parámetro t, el punto de impacto y
// la normal en ese punto, porque las tres cosas se calculan juntas y casi
// siempre se necesitan juntas (por ejemplo, la Fase 7 de reflexión
// necesita 'point' para el nuevo origen del rayo reflejado y 'normal' para
// aplicar r = d - 2(d.n)n).
// ----------------------------------------------------------------------------
struct RayCircleHit
{
    double t;                  // parametro a lo largo del rayo: P(t) = point
    Math::Vector2 point;       // punto de impacto en el mundo fisico
    Math::Vector2 normal;      // normal unitaria hacia afuera en 'point'
};

// ----------------------------------------------------------------------------
// intersectRayCircle
//
// Devuelve std::nullopt si el rayo no intersecta la circunferencia por
// delante de su origen (t > epsilon); en caso contrario, devuelve el
// RayCircleHit correspondiente a la intersección MÁS CERCANA al origen del
// rayo (el menor t válido).
//
// 'epsilon' es el umbral bajo el cual un t se considera "no por delante"
// del rayo (requisito: descartar t <= epsilon). Su valor por defecto es
// Math::EPSILON (1e-9), adecuado para la propia resolución de la
// cuadrática; la Fase 7 usará un epsilon de desplazamiento distinto (y
// mayor) al construir el rayo reflejado, para separar su origen de la
// superficie de la que rebota.
// ----------------------------------------------------------------------------
[[nodiscard]] inline std::optional<RayCircleHit> intersectRayCircle(
    const Ray& ray,
    const Geometry::Circle& circle,
    double epsilon = Math::EPSILON)
{
    const Math::Vector2 oc = ray.origin - circle.center;

    // a = ray.direction.dot(ray.direction) es exactamente 1.0 porque Ray
    // garantiza direccion unitaria (ver derivacion en el comentario de
    // cabecera). Se usa directamente esa simplificacion en vez de
    // recalcular 'a' y dividir por el, tanto por claridad matematica como
    // por evitar una division innecesaria.
    const double b_half = oc.dot(ray.direction);      // (oc . d), es decir, b/2
    const double c = oc.dot(oc) - circle.radius * circle.radius;

    // discriminante / 4 = (oc.d)^2 - c
    const double quarterDiscriminant = b_half * b_half - c;

    if (quarterDiscriminant < 0.0)
    {
        // Discriminante negativo: la recta que contiene al rayo pasa
        // completamente fuera de la circunferencia. No hay interseccion
        // real (ni siquiera detras del origen).
        return std::nullopt;
    }

    const double sqrtQuarterDisc = std::sqrt(quarterDiscriminant);

    // Las dos raices de la cuadratica (ya simplificadas con a=1):
    //   t = -b_half ± sqrtQuarterDisc
    const double tNear = -b_half - sqrtQuarterDisc;
    const double tFar = -b_half + sqrtQuarterDisc;

    // Se elige la menor raiz que este estrictamente por delante del rayo
    // (t > epsilon). tNear <= tFar siempre (sqrtQuarterDisc >= 0), asi que
    // se prueba primero tNear; si no es valida (por ejemplo, el origen
    // del rayo esta DENTRO de la circunferencia, y tNear es negativa),
    // se prueba tFar.
    double chosenT;
    if (tNear > epsilon)
    {
        chosenT = tNear;
    }
    else if (tFar > epsilon)
    {
        chosenT = tFar;
    }
    else
    {
        // Ambas raices estan detras del origen (o son practicamente cero,
        // es decir, el propio origen del rayo): no hay interseccion util
        // por delante de la trayectoria del rayo.
        return std::nullopt;
    }

    const Math::Vector2 hitPoint = ray.pointAt(chosenT);

    RayCircleHit result;
    result.t = chosenT;
    result.point = hitPoint;
    result.normal = circle.outwardNormalAt(hitPoint);
    return result;
}

// ============================================================================
// DERIVACIÓN MATEMÁTICA — ELIPSE (Fase 10)
// ----------------------------------------------------------------------------
// La ecuación de la elipse es:
//
//     (x-cx)^2/a^2 + (y-cy)^2/b^2 = 1
//
// Sea oc = P0 - center (igual que para la circunferencia). Sustituyendo
// P = P0 + t*d = (oc.x + t*d.x, oc.y + t*d.y) + center en la ecuación
// (el '+center' se cancela porque la ecuación ya está centrada en
// 'center'):
//
//     (oc.x + t*d.x)^2/a^2 + (oc.y + t*d.y)^2/b^2 = 1
//
// Expandiendo cada cuadrado y agrupando por potencias de t:
//
//     t^2 [d.x^2/a^2 + d.y^2/b^2]
//   + t   [2(oc.x*d.x)/a^2 + 2(oc.y*d.y)/b^2]
//   +     [oc.x^2/a^2 + oc.y^2/b^2 - 1]
//   = 0
//
// que es una cuadrática A t^2 + B t + C = 0 con:
//
//     A = d.x^2/a^2 + d.y^2/b^2
//     B = 2 (oc.x*d.x/a^2 + oc.y*d.y/b^2)
//     C = oc.x^2/a^2 + oc.y^2/b^2 - 1
//
// OBSERVACIÓN IMPORTANTE: a diferencia del caso de la circunferencia, A ya
// NO es necesariamente 1 (la dirección unitaria d, al escalarse de forma
// distinta en x e y por 1/a^2 y 1/b^2, pierde esa simplificación). A SÍ es
// siempre estrictamente positivo (suma de dos cuadrados no negativos,
// donde d no puede ser el vector nulo por invariante de Ray, y a,b > 0
// por invariante de Ellipse), así que dividir por A nunca es una división
// por cero.
//
// También se puede notar que C = Ellipse::implicitFunction(ray.origin):
// tiene sentido, ya que C es, por construcción, el valor de la ecuación
// de la elipse evaluada en t=0 (el propio origen del rayo).
//
// El resto del procedimiento (discriminante, elegir la menor raíz con
// t > epsilon) es idéntico en estructura al caso de la circunferencia.
// ============================================================================

struct RayEllipseHit
{
    double t;
    Math::Vector2 point;
    Math::Vector2 normal;
};

[[nodiscard]] inline std::optional<RayEllipseHit> intersectRayEllipse(
    const Ray& ray,
    const Geometry::Ellipse& ellipse,
    double epsilon = Math::EPSILON)
{
    const Math::Vector2 oc = ray.origin - ellipse.center;
    const double aSq = ellipse.a * ellipse.a;
    const double bSq = ellipse.b * ellipse.b;

    const double A = (ray.direction.x * ray.direction.x) / aSq
                    + (ray.direction.y * ray.direction.y) / bSq;
    const double B = 2.0 * (oc.x * ray.direction.x / aSq + oc.y * ray.direction.y / bSq);
    const double C = (oc.x * oc.x) / aSq + (oc.y * oc.y) / bSq - 1.0;

    const double discriminant = B * B - 4.0 * A * C;

    if (discriminant < 0.0)
    {
        // La recta que contiene al rayo no toca la elipse en ningun punto
        // real.
        return std::nullopt;
    }

    const double sqrtDiscriminant = std::sqrt(discriminant);

    // Formula cuadratica general (aqui SI hace falta dividir por 2A,
    // porque A no es 1 como en el caso de la circunferencia).
    const double tNear = (-B - sqrtDiscriminant) / (2.0 * A);
    const double tFar = (-B + sqrtDiscriminant) / (2.0 * A);

    double chosenT;
    if (tNear > epsilon)
    {
        chosenT = tNear;
    }
    else if (tFar > epsilon)
    {
        chosenT = tFar;
    }
    else
    {
        return std::nullopt;
    }

    const Math::Vector2 hitPoint = ray.pointAt(chosenT);

    RayEllipseHit result;
    result.t = chosenT;
    result.point = hitPoint;
    result.normal = ellipse.outwardNormalAt(hitPoint);
    return result;
}

} // namespace RayOptics::Physics
