import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from ipywidgets import interact, FloatSlider, IntSlider
import warnings
warnings.filterwarnings('ignore')

# ==========================================
# 1. FUNCIONES MATEMÁTICAS BÁSICAS
# ==========================================

def reflexion_vectorial(d, n):
    """Calcula el vector reflejado dado un vector director d y una normal n."""
    d = d / np.linalg.norm(d)
    n = n / np.linalg.norm(n)
    return d - 2 * np.dot(d, n) * n

def interseccion_circulo(p, d, R):
    """Encuentra el siguiente punto de intersección en un círculo de radio R."""
    d = d / np.linalg.norm(d)
    # Ecuación cuadrática: |p + t*d|^2 = R^2 => t^2 + 2(p.d)t + (|p|^2 - R^2) = 0
    # Como p está dentro, |p|^2 - R^2 < 0, hay dos soluciones reales.
    b = 2 * np.dot(p, d)
    c = np.dot(p, p) - R**2
    discriminante = b**2 - 4 * c
    t1 = (-b + np.sqrt(discriminante)) / 2
    t2 = (-b - np.sqrt(discriminante)) / 2
    t = max(t1, t2) # Tomamos la distancia positiva (hacia donde apunta el rayo)
    return p + t * d

def interseccion_elipse(p, d, a, b):
    """Encuentra el siguiente punto de intersección en una elipse x^2/a^2 + y^2/b^2 = 1."""
    d = d / np.linalg.norm(d)
    # Ecuación: (px + t*dx)^2/a^2 + (py + t*dy)^2/b^2 = 1
    A = (d[0]**2 / a**2) + (d[1]**2 / b**2)
    B = 2 * ((p[0] * d[0] / a**2) + (p[1] * d[1] / b**2))
    C = (p[0]**2 / a**2) + (p[1]**2 / b**2) - 1
    discriminante = B**2 - 4 * A * C
    if discriminante < 0:
        return None # No hay intersección
    t1 = (-B + np.sqrt(discriminante)) / (2 * A)
    t2 = (-B - np.sqrt(discriminante)) / (2 * A)
    t = max(t1, t2)
    return p + t * d

def normal_elipse(p, a, b):
    """Calcula la normal exterior en un punto de la elipse."""
    n = np.array([p[0] / a**2, p[1] / b**2])
    return n / np.linalg.norm(n)

# ==========================================
# 2. SIMULACIÓN Y GRÁFICAS ESTÁTICAS
# ==========================================

def simular_trayectoria(tipo, params, n_rebotes=50):
    """Genera las coordenadas x, y de una trayectoria con múltiples rebotes."""
    trayectoria = []
    if tipo == 'circulo':
        R = params['R']
        p = np.array([0.0, 0.0]) # Inicia en el centro
        # Ángulo de incidencia respecto a la horizontal
        theta = params['theta']
        d = np.array([np.cos(theta), np.sin(theta)])
        trayectoria.append(p)

        for _ in range(n_rebotes):
            p_int = interseccion_circulo(p, d, R)
            trayectoria.append(p_int)
            # Normal en el círculo es el vector radial
            n = p_int / np.linalg.norm(p_int)
            d = reflexion_vectorial(d, n)
            p = p_int

    elif tipo == 'elipse':
        a, b = params['a'], params['b']
        # Inicia en el foco F1 (c, 0)
        c = np.sqrt(a**2 - b**2)
        p = np.array([c, 0.0])
        theta = params['theta'] # Ángulo de disparo
        d = np.array([np.cos(theta), np.sin(theta)])
        trayectoria.append(p)

        for _ in range(n_rebotes):
            p_int = interseccion_elipse(p, d, a, b)
            if p_int is None: break
            trayectoria.append(p_int)
            n = normal_elipse(p_int, a, b)
            d = reflexion_vectorial(d, n)
            p = p_int

    return np.array(trayectoria)

# --- Gráficas estáticas comparativas (Cíclico vs No Cíclico) ---
fig, axes = plt.subplots(2, 2, figsize=(12, 12))

# Círculo - Periódico (Ángulo racional)
ax = axes[0, 0]
tray = simular_trayectoria('circulo', {'R': 1, 'theta': np.deg2rad(144)}, 30)
ax.plot(tray[:, 0], tray[:, 1], 'r.-', lw=1, ms=3)
ax.set_title('Círculo: Periódico (144°)')
ax.set_aspect('equal')

# Círculo - Cuasiperiódico (Ángulo irracional)
ax = axes[0, 1]
tray = simular_trayectoria('circulo', {'R': 1, 'theta': np.deg2rad(67.5)}, 100) # Irracional o no divisor
ax.plot(tray[:, 0], tray[:, 1], 'b.-', lw=1, ms=3)
ax.set_title('Círculo: Cuasiperiódico (67.5°)')
ax.set_aspect('equal')

# Elipse - Desde el foco (Pasa por el otro foco)
ax = axes[1, 0]
tray = simular_trayectoria('elipse', {'a': 2, 'b': 1, 'theta': np.deg2rad(30)}, 20)
ax.plot(tray[:, 0], tray[:, 1], 'g.-', lw=1, ms=3)
# Dibujar la elipse
t = np.linspace(0, 2*np.pi, 100)
ax.plot(2*np.cos(t), 1*np.sin(t), 'k--', alpha=0.5)
ax.plot([np.sqrt(3), -np.sqrt(3)], [0, 0], 'ko', label='Focos')
ax.set_title('Elipse: Disparo desde Foco F1')
ax.legend()
ax.set_aspect('equal')

# Elipse - Trayectoria general (Caústica)
ax = axes[1, 1]
# Iniciar en un punto arbitrario del borde
p_start = np.array([1.5, 0.6614])
theta = np.deg2rad(80)
tray = []
p = p_start
d = np.array([np.cos(theta), np.sin(theta)])
tray.append(p)
for _ in range(30):
    p_int = interseccion_elipse(p, d, 2, 1)
    tray.append(p_int)
    n = normal_elipse(p_int, 2, 1)
    d = reflexion_vectorial(d, n)
    p = p_int
tray = np.array(tray)
ax.plot(tray[:, 0], tray[:, 1], 'm.-', lw=1, ms=3)
t = np.linspace(0, 2*np.pi, 100)
ax.plot(2*np.cos(t), 1*np.sin(t), 'k--', alpha=0.5)
ax.set_title('Elipse: Trayectoria General (Caústica)')
ax.set_aspect('equal')

plt.tight_layout()
plt.show()