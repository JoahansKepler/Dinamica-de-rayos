#pragma once
// ============================================================================
// RayOptics - Physics::refract (Ley de Snell)
// ----------------------------------------------------------------------------
// Implementa la ley de Snell en su forma vectorial:
//
//     n1 sin(theta1) = n2 sin(theta2)
//
// donde theta1 es el ángulo de incidencia (medido desde la normal) en el
// medio de índice n1, y theta2 es el ángulo de refracción (también desde
// la normal) en el medio de índice n2.
//
// ----------------------------------------------------------------------------
// DERIVACIÓN VECTORIAL COMPLETA
// ----------------------------------------------------------------------------
// Sea d la dirección UNITARIA incidente (dirección de propagación de la
// luz) y n la normal unitaria de la superficie, orientada de forma que
// apunte EN CONTRA del rayo incidente (es decir, d·n < 0; si la normal
// que se recibe no cumple esto, la función la invierte automáticamente,
// ver más abajo). Se define:
//
//     cos(theta1) = -d·n      (>= 0, por construcción de n)
//
// Igual que en la derivación de la reflexión (Physics/Reflection.h), se
// descompone d en su componente normal y tangencial:
//
//     d = d_n + d_t,   d_n = (d·n) n = -cos(theta1) n,   d_t = d - d_n
//
// Por Pitágoras (|d|=1, |d_n|=cos(theta1)): |d_t| = sin(theta1).
//
// La refracción, a diferencia de la reflexión, SÍ cambia el ángulo con la
// normal (theta1 -> theta2), pero mantiene la dirección tangencial: el
// rayo refractado no "gira" fuera del plano de incidencia. Su componente
// tangencial debe tener magnitud sin(theta2) en la misma dirección que
// d_t/sin(theta1) (el vector tangencial unitario):
//
//     t_t = sin(theta2) * (d_t / sin(theta1))
//
// Aquí aparece la simplificación clave: por la propia ley de Snell,
// sin(theta2)/sin(theta1) = n1/n2. Así que, SIN necesitar calcular ningún
// ángulo explícitamente:
//
//     t_t = (n1/n2) * d_t = (n1/n2) * (d + cos(theta1) n)
//
// La componente normal del rayo refractado tiene magnitud cos(theta2) y
// apunta hacia ADELANTE (continuando la propagación hacia el medio 2, es
// decir, en la dirección -n, ya que n apunta hacia atrás por
// construcción):
//
//     t_n = -cos(theta2) n
//
// Sumando:
//
//     t = t_t + t_n = (n1/n2) d + [(n1/n2) cos(theta1) - cos(theta2)] n
//
// que es la fórmula vectorial estándar de la refracción. Falta cos(theta2),
// que se obtiene de la propia ley de Snell:
//
//     sin(theta2) = (n1/n2) sin(theta1)
//     sin^2(theta2) = (n1/n2)^2 (1 - cos^2(theta1))
//     cos(theta2) = sqrt(1 - sin^2(theta2))          [si sin^2(theta2) <= 1]
//
// ----------------------------------------------------------------------------
// REFLEXIÓN INTERNA TOTAL: NO es una condición arbitraria
// ----------------------------------------------------------------------------
// La expresión sin^2(theta2) = (n1/n2)^2 (1 - cos^2(theta1)) es, para
// n1 > n2 y theta1 suficientemente grande, MAYOR QUE 1. Pero sin(theta2)
// es, por definición, el seno de un ángulo real: NUNCA puede exceder 1.
// Cuando la ecuación de Snell exige sin(theta2) > 1, la conclusión física
// es que NO EXISTE ningún ángulo real theta2 que satisfaga la ley de
// Snell: no hay rayo transmitido posible. Toda la energía se refleja
// (reflexión interna total). Esto no se impone "a mano": es una
// consecuencia directa de que sqrt(1 - sin^2(theta2)) no tiene solución
// real cuando sin^2(theta2) > 1, exactamente como pide el requisito
// ("debe derivarse de la ley de Snell").
//
// El ángulo crítico theta_c (a partir del cual ocurre TIR) se obtiene
// poniendo sin(theta2) = 1 en la ley de Snell:
//
//     n1 sin(theta_c) = n2 sin(90°) = n2   =>   theta_c = arcsin(n2/n1)
//
// (solo tiene solución real si n2/n1 <= 1, es decir, si n1 >= n2: la TIR
// solo puede ocurrir al pasar de un medio MÁS denso a uno MENOS denso,
// nunca al revés — otra consecuencia que emerge de la propia fórmula, no
// una regla añadida aparte).
// ============================================================================

#include "Math/Vector2.h"

#include <cmath>
#include <optional>
#include <stdexcept>

namespace RayOptics::Physics
{

// ----------------------------------------------------------------------------
// refract
//
// Calcula la dirección unitaria refractada al pasar de un medio de índice
// 'n1' (donde viaja el rayo incidente) a uno de índice 'n2' (donde
// viajaría el rayo transmitido), a través de una superficie con normal
// 'surfaceNormal' (unitaria, en cualquiera de las dos orientaciones: la
// función se encarga de orientarla correctamente).
//
// Devuelve std::nullopt si y solo si se produce reflexión interna total
// (sin^2(theta2) > 1): en ese caso NO existe físicamente un rayo
// refractado, y quien llama a esta función debe usar Physics::reflect()
// en su lugar. Esta fase (11) se limita a implementar correctamente la
// ley de Snell, incluyendo la detección de este caso; la INTEGRACIÓN de
// "si hay TIR, usar el espejo ideal en su lugar" en el flujo de
// trazado de rayos es tarea de la Fase 12.
//
// Precondiciones: incidentDirection y surfaceNormal deben ser unitarios
// (verificado en builds de depuración, igual que en Physics::reflect);
// n1 y n2 deben ser estrictamente positivos (un índice de refracción no
// positivo no tiene significado físico).
// ----------------------------------------------------------------------------
[[nodiscard]] inline std::optional<Math::Vector2> refract(
    const Math::Vector2& incidentDirection,
    const Math::Vector2& surfaceNormal,
    double n1,
    double n2)
{
#ifndef NDEBUG
    if (!Math::isNearlyZero(incidentDirection.length() - 1.0, 1e-6))
    {
        throw std::domain_error("Physics::refract: la direccion incidente debe ser unitaria.");
    }
    if (!Math::isNearlyZero(surfaceNormal.length() - 1.0, 1e-6))
    {
        throw std::domain_error("Physics::refract: la normal de superficie debe ser unitaria.");
    }
#endif

    if (n1 <= 0.0 || n2 <= 0.0)
    {
        throw std::domain_error("Physics::refract: los indices de refraccion deben ser positivos.");
    }

    // Orientar la normal para que apunte EN CONTRA del rayo incidente
    // (d·n < 0), tal como exige la derivación de arriba. Si la normal
    // recibida (por ejemplo, la normal "hacia afuera" que devuelven
    // Circle/Ellipse) apunta en la misma dirección general que el rayo
    // -es decir, el rayo la está atravesando de dentro hacia afuera-, se
    // invierte para obtener la convención correcta.
    Math::Vector2 n = surfaceNormal;
    double cosTheta1 = -incidentDirection.dot(n);
    if (cosTheta1 < 0.0)
    {
        n = -n;
        cosTheta1 = -cosTheta1;
    }

    const double eta = n1 / n2;
    const double sin2Theta2 = eta * eta * (1.0 - cosTheta1 * cosTheta1);

    if (sin2Theta2 > 1.0)
    {
        // Reflexion interna total: ver derivacion de arriba.
        return std::nullopt;
    }

    const double cosTheta2 = std::sqrt(1.0 - sin2Theta2);

    return eta * incidentDirection + (eta * cosTheta1 - cosTheta2) * n;
}

// ----------------------------------------------------------------------------
// criticalAngle
//
// theta_c = arcsin(n2/n1), el ángulo de incidencia a partir del cual
// ocurre reflexión interna total al pasar de un medio n1 a uno n2 MENOS
// denso (n2 < n1). Se expone como función independiente porque es útil
// para la interfaz interactiva (Fase 13: mostrar el ángulo crítico al
// usuario) sin tener que llamar a refract() con un ángulo de prueba.
//
// Devuelve std::nullopt si n2 >= n1: en ese caso no existe ángulo crítico
// (la TIR es geométricamente imposible al pasar a un medio MÁS denso o
// igualmente denso), consistente con la nota al final de la derivación.
// ----------------------------------------------------------------------------
[[nodiscard]] inline std::optional<double> criticalAngle(double n1, double n2)
{
    if (n1 <= 0.0 || n2 <= 0.0)
    {
        throw std::domain_error("Physics::criticalAngle: los indices de refraccion deben ser positivos.");
    }

    if (n2 >= n1)
    {
        return std::nullopt;
    }

    return std::asin(n2 / n1);
}

} // namespace RayOptics::Physics
