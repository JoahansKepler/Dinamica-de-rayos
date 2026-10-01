#pragma once
// ============================================================================
// RayOptics - Geometry::Circle
// ----------------------------------------------------------------------------
// Representa la circunferencia general (requisito, sección 3):
//
//     (x - h)^2 + (y - k)^2 = R^2
//
// En esta fase (5) SOLO se implementa la geometría en sí misma: centro,
// radio, y las consultas puramente geométricas que se derivan de la
// ecuación de la circunferencia (pertenencia de un punto, normal en un
// punto de la superficie). La intersección con un rayo (resolver la
// ecuación cuadrática) es responsabilidad de Physics/Intersection.h y se
// implementa en la Fase 6, para no mezclar "qué es una circunferencia"
// con "cómo interseca un rayo con una circunferencia" (de nuevo,
// responsabilidad única: Circle no debería saber qué es un Ray).
//
// Decisión de diseño: header-only.
//   Igual que Vector2, Circle es un tipo de valor pequeño (un Vector2 +
//   un double) con operaciones aritméticas simples. No hay E/S, no hay
//   estado de OpenGL, no hay nada que justifique separar .h de .cpp.
// ============================================================================

#include "Math/Vector2.h"

#include <stdexcept>

namespace RayOptics::Geometry
{

class Circle
{
public:
    Math::Vector2 center;
    double radius;

    // El radio debe ser estrictamente positivo: una circunferencia de
    // radio 0 (o negativo) no es una geometría válida para una cavidad
    // óptica. Se lanza en el constructor, no se "corrige en silencio"
    // (por ejemplo, tomando el valor absoluto), porque un radio <= 0
    // casi siempre indica un error de configuración en quien construye
    // el objeto (por ejemplo, restar mal dos coordenadas).
    Circle(const Math::Vector2& center_, double radius_)
        : center(center_)
        , radius(radius_)
    {
        if (radius <= Math::EPSILON)
        {
            throw std::domain_error(
                "Circle: el radio debe ser estrictamente positivo.");
        }
    }

    // ---- F(x,y) = (x-h)^2 + (y-k)^2 - R^2 -----------------------------------
    // Función implícita de la circunferencia. F(P) = 0 exactamente sobre
    // la circunferencia, F(P) > 0 fuera, F(P) < 0 dentro. Se expone
    // porque es la base tanto de isOnSurface() como, en la Fase 6, de la
    // deducción de la ecuación cuadrática de intersección.
    [[nodiscard]] double implicitFunction(const Math::Vector2& point) const noexcept
    {
        const Math::Vector2 delta = point - center;
        return delta.dot(delta) - radius * radius;
    }

    // ---- Pertenencia a la superficie (con tolerancia) -----------------------
    // Nunca se compara implicitFunction(point) == 0 directamente
    // (requisito 13): se usa una tolerancia acorde a la escala de R^2,
    // ya que el valor de F crece con el cuadrado del radio.
    [[nodiscard]] bool isOnSurface(const Math::Vector2& point, double epsilon = Math::EPSILON) const noexcept
    {
        // Tolerancia relativa a la magnitud de R^2 para que epsilon
        // tenga sentido tanto para circunferencias pequeñas (R=0.1) como
        // grandes (R=1000): un epsilon absoluto de 1e-9 sería
        // absurdamente estricto para R=1000 y F(P) calculado con la
        // precisión limitada de 'double'.
        const double scale = radius * radius;
        return std::fabs(implicitFunction(point)) <= epsilon * (1.0 + scale);
    }

    // ---- ¿El punto está estrictamente dentro de la cavidad? -----------------
    // Útil para la validación de la Fase 12 ("el rayo permanece dentro
    // de la cavidad"): un punto está dentro si su distancia al centro es
    // menor que el radio.
    [[nodiscard]] bool containsPoint(const Math::Vector2& point) const noexcept
    {
        return implicitFunction(point) < 0.0;
    }

    // ---- Normal unitaria hacia afuera en un punto de la superficie ----------
    // n = normalize(P - C), tal como especifica el requisito de la
    // sección 3. Se asume (precondición del método, no verificada aquí
    // por rendimiento) que 'point' está sobre la circunferencia; se
    // documenta como precondición porque calcular una "normal" en un
    // punto que no pertenece a la curva no tiene significado físico.
    //
    // NOTA IMPORTANTE sobre el signo de la normal: la ley de reflexión
    // r = d - 2(d.n)n es invariante ante n -> -n (se puede demostrar
    // algebraicamente: (d.(-n))·(-n) = (d.n)·n), así que no importa si
    // se usa la normal "hacia afuera" o "hacia adentro" para reflejar un
    // rayo. Aun así, se define consistentemente hacia afuera (alejándose
    // del centro) porque así lo pide el requisito y porque es más
    // intuitivo al depurar visualmente (Fase 13: mostrar normales).
    [[nodiscard]] Math::Vector2 outwardNormalAt(const Math::Vector2& point) const
    {
        // point - center puede ser (numéricamente) el vector nulo solo si
        // 'point' coincide con el centro, lo cual violaría la
        // precondición "point está sobre la circunferencia" (el centro
        // nunca pertenece a la circunferencia, salvo R=0, ya prohibido
        // en el constructor). Vector2::normalized() lanzará en ese caso,
        // lo cual es el comportamiento correcto: es un error de quien
        // llama, no un caso a tolerar.
        return (point - center).normalized();
    }
};

} // namespace RayOptics::Geometry
