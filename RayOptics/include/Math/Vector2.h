#pragma once
// ============================================================================
// RayOptics - Math::Vector2
// ----------------------------------------------------------------------------
// Vector 2D de doble precisión, implementado a mano (sin GLM ni ninguna otra
// dependencia externa) tal como exige el punto 8 de los requisitos: la física
// debe ser transparente y verificable, no una caja negra dentro de una
// librería de terceros.
//
// GLM podrá usarse más adelante para transformaciones puramente gráficas
// (por ejemplo, matrices de proyección/vista en el renderer), pero TODA la
// física (intersecciones, normales, reflexión, refracción) se apoya
// exclusivamente en esta clase.
//
// Decisión de diseño: clase "header-only".
//   Vector2 es un tipo de valor muy pequeño (dos doubles) que se va a usar
//   en el interior de bucles de cálculo (intersecciones, reflexiones,
//   trayectorias con potencialmente miles de puntos). Declarar sus métodos
//   como funciones cortas dentro del propio header permite al compilador
//   inlinearlas de forma agresiva, evitando el coste de una llamada a
//   función por cada suma o producto punto. No hay lógica compleja que
//   justifique separar declaración (.h) de implementación (.cpp) todavía;
//   si en el futuro Vector2 creciera en complejidad, se dividiría.
// ============================================================================

#include <cmath>       // std::sqrt, std::fabs
#include <ostream>     // operator<< para depuración
#include <stdexcept>   // std::domain_error (normalizar un vector nulo)

namespace RayOptics::Math
{

// ----------------------------------------------------------------------------
// Tolerancia numérica por defecto.
//
// Requisito 13 (precisión numérica): nunca comparar doubles con `== 0`.
// Esta constante se usa, entre otros sitios, para detectar vectores
// "prácticamente nulos" antes de normalizar (evitar división por cero).
// ----------------------------------------------------------------------------
inline constexpr double EPSILON = 1e-9;

// ----------------------------------------------------------------------------
// isNearlyZero
//
// Comparación segura de un double contra cero, con tolerancia. Se usa en
// vez de `value == 0.0` en todo el proyecto (física, geometría, tests).
// ----------------------------------------------------------------------------
[[nodiscard]] inline bool isNearlyZero(double value, double epsilon = EPSILON) noexcept
{
    return std::fabs(value) < epsilon;
}

// ============================================================================
// class Vector2
// ============================================================================
class Vector2
{
public:
    double x{0.0};
    double y{0.0};

    // ---- Construcción -----------------------------------------------------
    constexpr Vector2() noexcept = default;
    constexpr Vector2(double x_, double y_) noexcept : x(x_), y(y_) {}

    // ---- Suma ---------------------------------------------------------------
    [[nodiscard]] constexpr Vector2 operator+(const Vector2& rhs) const noexcept
    {
        return Vector2(x + rhs.x, y + rhs.y);
    }

    constexpr Vector2& operator+=(const Vector2& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    // ---- Resta ---------------------------------------------------------------
    [[nodiscard]] constexpr Vector2 operator-(const Vector2& rhs) const noexcept
    {
        return Vector2(x - rhs.x, y - rhs.y);
    }

    constexpr Vector2& operator-=(const Vector2& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    // Vector opuesto (unario). Útil, por ejemplo, para invertir la dirección
    // de un rayo al comprobar de qué lado de una superficie llega.
    [[nodiscard]] constexpr Vector2 operator-() const noexcept
    {
        return Vector2(-x, -y);
    }

    // ---- Multiplicación por escalar ------------------------------------------
    [[nodiscard]] constexpr Vector2 operator*(double scalar) const noexcept
    {
        return Vector2(x * scalar, y * scalar);
    }

    constexpr Vector2& operator*=(double scalar) noexcept
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    // Permite escribir tanto `v * 2.0` como `2.0 * v`.
    [[nodiscard]] friend constexpr Vector2 operator*(double scalar, const Vector2& v) noexcept
    {
        return v * scalar;
    }

    // ---- División por escalar -------------------------------------------------
    // NO es constexpr ni noexcept: puede lanzar si el divisor es
    // (numéricamente) cero, en vez de producir silenciosamente inf/NaN,
    // que después se propagaría de forma invisible por todos los cálculos
    // de intersección y reflexión.
    [[nodiscard]] Vector2 operator/(double scalar) const
    {
        if (isNearlyZero(scalar))
        {
            throw std::domain_error(
                "Vector2::operator/: division por un escalar practicamente cero.");
        }
        return Vector2(x / scalar, y / scalar);
    }

    Vector2& operator/=(double scalar)
    {
        if (isNearlyZero(scalar))
        {
            throw std::domain_error(
                "Vector2::operator/=: division por un escalar practicamente cero.");
        }
        x /= scalar;
        y /= scalar;
        return *this;
    }

    // ---- Igualdad aproximada ---------------------------------------------------
    // Comparación con tolerancia; jamás se debe usar `==` directo entre
    // Vector2 que provienen de cálculos en coma flotante (requisito 13).
    [[nodiscard]] bool isApprox(const Vector2& rhs, double epsilon = EPSILON) const noexcept
    {
        return isNearlyZero(x - rhs.x, epsilon) && isNearlyZero(y - rhs.y, epsilon);
    }

    // ---- Producto punto -------------------------------------------------------
    // Pieza central de la ley de reflexión: r = d - 2(d.n)n
    [[nodiscard]] constexpr double dot(const Vector2& rhs) const noexcept
    {
        return x * rhs.x + y * rhs.y;
    }

    // ---- Producto cruzado 2D (escalar) -----------------------------------------
    // En 2D el "producto cruzado" de dos vectores no da un vector sino un
    // escalar (la componente z de lo que sería el producto cruzado en 3D
    // con z=0 para ambos). Es útil para determinar orientación (sentido
    // horario/antihorario), lo cual se necesitará en Geometry para, por
    // ejemplo, orientar correctamente las normales.
    [[nodiscard]] constexpr double cross(const Vector2& rhs) const noexcept
    {
        return x * rhs.y - y * rhs.x;
    }

    // ---- Norma (magnitud) -------------------------------------------------------
    // No puede ser `constexpr` en C++20 porque std::sqrt no es constexpr
    // hasta C++26. Se prioriza corrección/portabilidad sobre "constexpr a
    // toda costa".
    [[nodiscard]] double length() const noexcept
    {
        return std::sqrt(lengthSquared());
    }

    // Norma al cuadrado. Se expone por separado porque en muchas
    // comparaciones (por ejemplo, "¿qué intersección está más cerca?")
    // basta con comparar longitudSquared y así se evita una raíz cuadrada
    // innecesaria (relevante para rendimiento, prioridad 6 del proyecto,
    // aunque siempre por debajo de la corrección física).
    [[nodiscard]] constexpr double lengthSquared() const noexcept
    {
        return x * x + y * y;
    }

    // ---- Normalización -------------------------------------------------------
    // Devuelve un nuevo vector unitario en la misma dirección. Lanza si el
    // vector es (numéricamente) el vector nulo, ya que un "vector unitario
    // de dirección nula" no tiene sentido físico ni matemático: normalizar
    // ese caso en silencio (devolviendo, por ejemplo, (0,0)) ocultaría un
    // error de más arriba en el programa (un rayo con dirección nula, una
    // normal mal calculada, etc.).
    [[nodiscard]] Vector2 normalized() const
    {
        const double len = length();
        if (isNearlyZero(len))
        {
            throw std::domain_error(
                "Vector2::normalized: no se puede normalizar un vector de longitud casi nula.");
        }
        return Vector2(x / len, y / len);
    }

    // Versión que modifica el propio objeto en lugar de devolver uno nuevo.
    void normalize()
    {
        *this = normalized();
    }

    // ---- Vector perpendicular ---------------------------------------------------
    // Rota el vector 90 grados en sentido antihorario: (x, y) -> (-y, x).
    // Se usará en Geometry para construir tangentes a partir de normales
    // (la tangente es perpendicular a la normal), necesario en la Fase 12
    // para verificar "la normal es perpendicular a la tangente".
    [[nodiscard]] constexpr Vector2 perpendicular() const noexcept
    {
        return Vector2(-y, x);
    }

    // ---- Reflexión especular ----------------------------------------------------
    // Implementa directamente r = d - 2(d.n)n, donde `this` es la
    // dirección incidente `d` y `normal` debe ser unitaria.
    //
    // Se incluye aquí, como método de Vector2, además de en
    // Physics/Reflection.h (Fase 7), porque conceptualmente es una
    // operación puramente vectorial que no depende de ninguna geometría
    // concreta (círculo, elipse...). Physics/Reflection.h la usará junto
    // con metadatos físicos adicionales (ángulos, puntos de incidencia,
    // etc.). No se implementa todavía en esta fase: se deja documentado
    // aquí para que quede claro por qué Vector2 ya tiene las operaciones
    // (dot, resta, multiplicación por escalar) que la hacen posible.
    //
    // (Implementación real: Fase 7 - Physics/Reflection.h)

    // ---- Salida por flujo (depuración) --------------------------------------------
    friend std::ostream& operator<<(std::ostream& os, const Vector2& v)
    {
        os << "(" << v.x << ", " << v.y << ")";
        return os;
    }
};

} // namespace RayOptics::Math
