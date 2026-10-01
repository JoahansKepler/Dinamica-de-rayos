#pragma once
// ============================================================================
// RayOptics - Physics::Reflection
// ----------------------------------------------------------------------------
// Implementa la ley de reflexión especular en su forma vectorial rigurosa
// (requisito explícito: NO una aproximación basada en ángulos):
//
//     r = d - 2(d·n)n
//
// donde d es la dirección UNITARIA incidente, n es la normal UNITARIA de
// la superficie en el punto de incidencia, y r es la dirección UNITARIA
// reflejada.
//
// ----------------------------------------------------------------------------
// POR QUÉ ESTA ECUACIÓN REPRODUCE ángulo_incidencia = ángulo_reflexión
// ----------------------------------------------------------------------------
// Se descompone el vector incidente d en dos componentes ortogonales
// respecto a la normal n:
//
//     d = d_n + d_t
//
// donde:
//     d_n = (d·n) n            <- componente de d a lo largo de la normal
//     d_t = d - d_n            <- componente de d tangente a la superficie
//                                  (perpendicular a n, por construcción)
//
// La reflexión especular ideal se define físicamente como: "la componente
// tangencial del rayo se conserva, la componente normal se invierte".
// Esto es exactamente lo que dice la ley de reflexión de la óptica
// geométrica (el rayo se refleja como si rebotara contra un plano rígido
// perpendicular a n). Aplicando esa regla:
//
//     r = d_t - d_n = (d - d_n) - d_n = d - 2 d_n = d - 2(d·n)n
//
// que es exactamente la fórmula pedida. Es decir, la fórmula NO es una
// receta arbitraria: es la consecuencia algebraica directa de "conservar
// la componente tangencial, invertir la componente normal".
//
// De aquí se sigue directamente que el ángulo de incidencia (medido desde
// la normal) es igual al ángulo de reflexión (medido desde la normal):
// ambos ángulos son el ángulo entre ±d_n y d_t en el paralelogramo
// formado por d_n y d_t, y ese ángulo no cambia al invertir el signo de
// d_n (invertir un componente de un triángulo rectángulo no cambia el
// ángulo entre la hipotenusa y el otro cateto... más precisamente: el
// ángulo de d respecto a n es arctan(|d_t|/|d_n|) en valor absoluto, y el
// de r respecto a n es arctan(|d_t|/|-d_n|) = arctan(|d_t|/|d_n|): el
// mismo ángulo).
//
// Dos propiedades adicionales, verificadas en el auto-test de esta fase:
//
//   1. CONSERVACIÓN DE LA MAGNITUD: |r| = |d| = 1. Demostración:
//        |r|^2 = r·r = (d - 2(d·n)n)·(d - 2(d·n)n)
//              = d·d - 4(d·n)(n·d) + 4(d·n)^2 (n·n)
//              = |d|^2 - 4(d·n)^2 + 4(d·n)^2 · 1      [porque n·n = 1, n unitaria]
//              = |d|^2
//      Es decir, reflejar no cambia la "velocidad" del rayo, coherente
//      con un espejo ideal que no absorbe energía.
//
//   2. INVARIANCIA ANTE EL SIGNO DE n: sustituyendo n por -n en la
//      fórmula, (d·(-n))(-n) = (d·n)(n), así que r no cambia. Por eso no
//      importa si Geometry::Circle/Ellipse definen su normal "hacia
//      afuera" o "hacia adentro": el resultado de la reflexión es
//      idéntico.
// ============================================================================

#include "Math/Vector2.h"
#include "Physics/Ray.h"
#include "Physics/Intersection.h"

#include <stdexcept>

namespace RayOptics::Physics
{

// ----------------------------------------------------------------------------
// Desplazamiento epsilon aplicado al ORIGEN del rayo reflejado, a lo largo
// de su nueva dirección, antes de que se le vuelva a probar contra
// cualquier superficie.
//
// Es un valor DISTINTO de Math::EPSILON (1e-9): aquel se usa para decidir
// si una raíz t de la cuadrática de intersección está "por delante" del
// origen del rayo (una comparación puramente algebraica). Este otro,
// REFLECTION_OFFSET, es una distancia geométrica real en el mundo físico:
// debe ser lo bastante grande para que, tras la acumulación de error de
// redondeo en punto (·), resta y normalización, el nuevo origen quede
// inequívocamente en el exterior de la superficie de la que rebotó (y no
// vuelva a auto-intersectarla por culpa de ese error), pero lo bastante
// pequeño para no desviar visiblemente la trayectoria real. 1e-6 es un
// valor típico para geometrías con radios/semiejes del orden de 1-100
// unidades; si en el futuro se manejan escalas muy distintas, convendría
// escalar este valor con el tamaño de la geometría.
inline constexpr double REFLECTION_OFFSET = 1e-6;

// ----------------------------------------------------------------------------
// reflect
//
// Aplica r = d - 2(d·n)n. Ambos vectores DEBEN ser unitarios (precondición
// física: la fórmula solo reproduce ángulo_incidencia = ángulo_reflexión
// si n es unitaria; ver derivación de arriba). En builds de depuración se
// verifica esa precondición explícitamente y se lanza si se viola, en vez
// de producir silenciosamente una reflexión con magnitud incorrecta.
// ----------------------------------------------------------------------------
[[nodiscard]] inline Math::Vector2 reflect(const Math::Vector2& incidentDirection,
                                            const Math::Vector2& surfaceNormal)
{
#ifndef NDEBUG
    if (!Math::isNearlyZero(incidentDirection.length() - 1.0, 1e-6))
    {
        throw std::domain_error(
            "Physics::reflect: la direccion incidente debe ser unitaria.");
    }
    if (!Math::isNearlyZero(surfaceNormal.length() - 1.0, 1e-6))
    {
        throw std::domain_error(
            "Physics::reflect: la normal de superficie debe ser unitaria.");
    }
#endif

    const double dDotN = incidentDirection.dot(surfaceNormal);
    return incidentDirection - (2.0 * dDotN) * surfaceNormal;
}

// ----------------------------------------------------------------------------
// reflectedRayFrom
//
// Construye el Ray reflejado a partir de un impacto rayo-superficie
// (cualquier tipo que exponga '.point' y '.normal': RayCircleHit de la
// Fase 6, o RayEllipseHit de la Fase 10) y la dirección incidente que
// produjo ese impacto.
//
// Se implementa como PLANTILLA (template) en vez de duplicar una función
// idéntica para RayCircleHit y otra para RayEllipseHit: ambos tipos de
// impacto tienen exactamente la misma forma relevante aquí (un punto y
// una normal), y la lógica de reflejar + desplazar por epsilon no depende
// en absoluto de qué geometría produjo ese impacto. Esto es "duck typing"
// estático de C++: HitType puede ser cualquier tipo con miembros .point y
// .normal, sin necesidad de una jerarquía de clases ni de herencia
// virtual (que aquí sería una complejidad innecesaria para dos structs de
// datos simples).
//
// El nuevo origen no es 'hit.point' directamente: se usa
// Ray::advanced(REFLECTION_OFFSET) (ver Physics/Ray.h, Fase 3) para
// desplazarlo una distancia mínima a lo largo de la dirección reflejada,
// exactamente la salvaguarda numérica pedida en el requisito de la
// sección 3 ("evitar que, después de reflejarse, el nuevo rayo vuelva a
// intersectar inmediatamente el mismo punto").
// ----------------------------------------------------------------------------
template <typename HitType>
[[nodiscard]] inline Ray reflectedRayFrom(const Math::Vector2& incidentDirection,
                                           const HitType& hit,
                                           double offset = REFLECTION_OFFSET)
{
    const Math::Vector2 reflectedDirection = reflect(incidentDirection, hit.normal);
    const Ray rayAtSurface(hit.point, reflectedDirection);
    return rayAtSurface.advanced(offset);
}

} // namespace RayOptics::Physics
