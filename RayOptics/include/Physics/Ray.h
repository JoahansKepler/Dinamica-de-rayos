#pragma once
// ============================================================================
// RayOptics - Physics::Ray
// ----------------------------------------------------------------------------
// Representa un rayo de luz en su forma paramétrica:
//
//     P(t) = P0 + t * d,   t >= 0
//
// donde P0 es el origen y d es la dirección UNITARIA del rayo. Esta es
// exactamente la formulación que se usará en la Fase 6 para calcular
// intersecciones rayo-circunferencia y, más adelante, rayo-elipse.
//
// Decisión de diseño: header-only, igual que Vector2.
//   Ray es, deliberadamente, un tipo muy simple: un par (origen, dirección)
//   más una función pointAt(t). No contiene ningún algoritmo de
//   intersección ni de reflexión — esos vivirán en Physics/Intersection.h,
//   Physics/Reflection.h y Physics/Refraction.h (fases 6, 7 y 11), que sí
//   merecerán su .cpp porque tendrán lógica no trivial. Meter esa lógica
//   dentro de la propia clase Ray acoplaría "qué es un rayo" con "qué le
//   pasa a un rayo al chocar con una superficie", que son responsabilidades
//   distintas (violación del principio de responsabilidad única).
// ============================================================================

#include "Math/Vector2.h"

namespace RayOptics::Physics
{

// ============================================================================
// class Ray
// ============================================================================
class Ray
{
public:
    Math::Vector2 origin;

    // INVARIANTE DE CLASE: 'direction' siempre es un vector unitario.
    // Se garantiza en el constructor (ver más abajo) normalizando
    // explícitamente el vector recibido, en vez de confiar en que quien
    // llame a Ray ya le pase un vector normalizado. Esto es importante
    // porque toda la física posterior (cálculo de t en la intersección,
    // ángulos de incidencia, ley de reflexión) asume d unitario; si no lo
    // fuera, esas fórmulas darían resultados incorrectos de forma sutil
    // (no un error visible, sino una simulación que "casi" funciona).
    Math::Vector2 direction;

    // ---- Construcción -----------------------------------------------------
    // Nota: 'direction_' se recibe por valor y se normaliza al construir.
    // Si 'direction_' es (numéricamente) el vector nulo,
    // Vector2::normalized() lanzará std::domain_error: un rayo sin
    // dirección definida es un error de programación en quien lo crea
    // (por ejemplo, origin y un segundo punto coincidentes al construir
    // "dirección = destino - origen"), no un caso a tolerar en silencio.
    Ray(const Math::Vector2& origin_, const Math::Vector2& direction_)
        : origin(origin_)
        , direction(direction_.normalized())
    {
    }

    // ---- Evaluación paramétrica: P(t) = P0 + t*d ---------------------------
    // Se le puede pasar cualquier t; corresponde a quien la use (por
    // ejemplo, el módulo de intersección en la Fase 6) decidir qué valores
    // de t son físicamente válidos (t > epsilon, es decir, "por delante"
    // del origen del rayo).
    [[nodiscard]] Math::Vector2 pointAt(double t) const noexcept
    {
        return origin + direction * t;
    }

    // ---- Construye un nuevo rayo desplazado a lo largo de su dirección -----
    // Utilidad pequeña que se necesitará en la Fase 7 para "despegar" el
    // punto de origen de un rayo reflejado de la superficie de la que
    // rebotó, evitando que el siguiente cálculo de intersección vuelva a
    // encontrar el mismo punto por errores de redondeo. Se incluye ya
    // aquí, junto a pointAt(), porque es una operación puramente
    // geométrica sobre un Ray, no algo específico de "qué es reflexión".
    [[nodiscard]] Ray advanced(double distance) const noexcept
    {
        return Ray(origin + direction * distance, direction, SkipNormalizationTag{});
    }

private:
    // Tag interno para permitir un segundo constructor privado que NO
    // vuelve a normalizar la dirección (porque ya es unitaria por
    // invariante de clase). Evita normalizar dos veces sin exponer un
    // constructor "inseguro" en la interfaz pública.
    struct SkipNormalizationTag {};

    Ray(const Math::Vector2& origin_, const Math::Vector2& direction_, SkipNormalizationTag)
        : origin(origin_)
        , direction(direction_)
    {
    }
};

} // namespace RayOptics::Physics
