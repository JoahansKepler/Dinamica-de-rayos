#pragma once
// ============================================================================
// RayOptics - Geometry::Ellipse
// ----------------------------------------------------------------------------
// Representa la elipse (requisito, sección 4):
//
//     (x - cx)^2/a^2 + (y - cy)^2/b^2 = 1
//
// Igual que en la Fase 5 con Circle, esta fase SOLO implementa la
// geometría en sí misma (centro, semiejes, pertenencia de un punto, normal
// vía gradiente, y los focos como propiedad geométrica intrínseca de la
// elipse). La intersección rayo-elipse es responsabilidad de la Fase 10
// (Physics/Intersection.h se ampliará con una sobrecarga para Ellipse).
//
// Decisión de diseño: header-only, mismos motivos que Circle (Fase 5):
// tipo de valor pequeño, sin E/S, sin estado de OpenGL.
// ============================================================================

#include "Math/Vector2.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace RayOptics::Geometry
{

class Ellipse
{
public:
    Math::Vector2 center;
    double a; // semieje en la dirección x (antes de considerar cuál es el mayor)
    double b; // semieje en la dirección y

    // Ambos semiejes deben ser estrictamente positivos, por la misma
    // razón que el radio de Circle: un semieje <= 0 no describe una
    // elipse válida y casi siempre delata un error de configuración.
    Ellipse(const Math::Vector2& center_, double a_, double b_)
        : center(center_)
        , a(a_)
        , b(b_)
    {
        if (a <= Math::EPSILON || b <= Math::EPSILON)
        {
            throw std::domain_error(
                "Ellipse: los semiejes 'a' y 'b' deben ser estrictamente positivos.");
        }
    }

    // ---- F(x,y) = (x-cx)^2/a^2 + (y-cy)^2/b^2 - 1 ---------------------------
    // Función implícita de la elipse. F(P) = 0 sobre la curva, F(P) > 0
    // fuera, F(P) < 0 dentro. Es la base de isOnSurface() y, en la Fase
    // 10, de la deducción de la ecuación cuadrática rayo-elipse (de forma
    // análoga a como implicitFunction() de Circle dio pie a la cuadrática
    // de la Fase 6).
    [[nodiscard]] double implicitFunction(const Math::Vector2& point) const noexcept
    {
        const double dx = (point.x - center.x) / a;
        const double dy = (point.y - center.y) / b;
        return dx * dx + dy * dy - 1.0;
    }

    // ---- Pertenencia a la superficie (con tolerancia) -----------------------
    // Al igual que en Circle, se evita comparar implicitFunction(point)
    // con 0 de forma exacta (requisito 13). Como F ya está normalizada
    // por a^2 y b^2 (sus términos son adimensionales, de orden 1 cerca de
    // la curva), un epsilon absoluto es razonable aquí, a diferencia de
    // Circle::isOnSurface, que necesitaba escalar por R^2.
    [[nodiscard]] bool isOnSurface(const Math::Vector2& point, double epsilon = 1e-6) const noexcept
    {
        return std::fabs(implicitFunction(point)) <= epsilon;
    }

    // ---- ¿El punto está estrictamente dentro de la cavidad elíptica? --------
    [[nodiscard]] bool containsPoint(const Math::Vector2& point) const noexcept
    {
        return implicitFunction(point) < 0.0;
    }

    // ---- Normal unitaria hacia afuera, vía el GRADIENTE de F -----------------
    // ∇F = (2(x-cx)/a^2, 2(y-cy)/b^2), tal como pide el requisito. El
    // gradiente de la función implícita de cualquier curva de nivel
    // (aquí, F=0) es SIEMPRE perpendicular a la curva en ese punto y
    // apunta en la dirección de crecimiento de F, que es hacia afuera de
    // la elipse (F crece al alejarse del centro). Por eso normalize(∇F)
    // es exactamente la normal unitaria hacia afuera pedida.
    //
    // Nota: el factor 2 común a ambas componentes de ∇F desaparece al
    // normalizar, así que se podría omitir por eficiencia; se conserva
    // en el código para que la fórmula sea visualmente idéntica a la del
    // requisito y a la derivación matemática, priorizando claridad sobre
    // una micro-optimización irrelevante.
    //
    // Precondición (no verificada aquí, por rendimiento): 'point' debe
    // estar sobre la elipse. Igual que en Circle::outwardNormalAt, pedir
    // la normal en un punto fuera de la curva no tiene significado físico.
    [[nodiscard]] Math::Vector2 outwardNormalAt(const Math::Vector2& point) const
    {
        const Math::Vector2 gradient(
            2.0 * (point.x - center.x) / (a * a),
            2.0 * (point.y - center.y) / (b * b)
        );
        return gradient.normalized();
    }

    // ---- Focos de la elipse ---------------------------------------------------
    // Propiedad geométrica intrínseca (no depende de ningún rayo): los
    // dos puntos focales, situados sobre el eje MAYOR, a distancia
    // c = sqrt(max(a,b)^2 - min(a,b)^2) del centro. Se incluyen ya en
    // esta fase (y no se posponen a la Fase 12, donde se usarán para la
    // validación de la propiedad focal) porque, igual que la normal, son
    // una propiedad de la forma en sí misma, no del trazado de rayos.
    //
    // Si a == b (numéricamente), la "elipse" es en realidad un círculo:
    // sus dos focos coinciden en el centro y la noción de "eje mayor" no
    // está definida. Se lanza en ese caso en vez de devolver dos puntos
    // iguales en silencio, porque casi siempre indica que se quería usar
    // Circle en primer lugar.
    [[nodiscard]] std::pair<Math::Vector2, Math::Vector2> foci() const
    {
        if (Math::isNearlyZero(a - b, 1e-9))
        {
            throw std::domain_error(
                "Ellipse::foci: a y b son (numericamente) iguales; los focos "
                "coinciden en el centro y el eje mayor no esta definido. "
                "Si se buscaba una circunferencia, usar Geometry::Circle.");
        }

        if (a > b)
        {
            // Eje mayor a lo largo de x.
            const double c = std::sqrt(a * a - b * b);
            return {
                Math::Vector2(center.x - c, center.y),
                Math::Vector2(center.x + c, center.y)
            };
        }
        else
        {
            // Eje mayor a lo largo de y.
            const double c = std::sqrt(b * b - a * a);
            return {
                Math::Vector2(center.x, center.y - c),
                Math::Vector2(center.x, center.y + c)
            };
        }
    }
};

} // namespace RayOptics::Geometry
