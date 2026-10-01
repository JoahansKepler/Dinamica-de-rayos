import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from ipywidgets import interact, FloatSlider, IntSlider
from IPython.display import HTML
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
    b = 2 * np.dot(p, d)
    c = np.dot(p, p) - R**2
    discriminante = b**2 - 4 * c
    t1 = (-b + np.sqrt(discriminante)) / 2
    t2 = (-b - np.sqrt(discriminante)) / 2
    t = max(t1, t2)
    return p + t * d

def interseccion_elipse(p, d, a, b):
    """Encuentra el siguiente punto de intersección en una elipse x^2/a^2 + y^2/b^2 = 1."""
    d = d / np.linalg.norm(d)
    A = (d[0]**2 / a**2) + (d[1]**2 / b**2)
    B = 2 * ((p[0] * d[0] / a**2) + (p[1] * d[1] / b**2))
    C = (p[0]**2 / a**2) + (p[1]**2 / b**2) - 1
    discriminante = B**2 - 4 * A * C
    if discriminante < 0: return None
    t1 = (-B + np.sqrt(discriminante)) / (2 * A)
    t2 = (-B - np.sqrt(discriminante)) / (2 * A)
    return p + max(t1, t2) * d

def normal_elipse(p, a, b):
    """Calcula la normal exterior en un punto de la elipse."""
    n = np.array([p[0] / a**2, p[1] / b**2])
    return n / np.linalg.norm(n)

# ==========================================
# 2. SIMULACIÓN CORREGIDA (ÁNGULO CENTRAL)
# ==========================================

def simular_trayectoria_circulo(alpha_grados, n_rebotes=50, R=1.0):
    """Simula el círculo usando el Ángulo Central entre cuerdas."""
    alpha = np.deg2rad(alpha_grados)
    trayectoria = []

    # CORRECCIÓN: Empezamos en el borde (R, 0)
    p = np.array([R, 0.0])
    # El primer rayo apunta hacia el punto definido por el ángulo central
    p_next = np.array([R * np.cos(alpha), R * np.sin(alpha)])
    d = p_next - p # Vector director inicial

    trayectoria.append(p)

    for _ in range(n_rebotes):
        p_int = interseccion_circulo(p, d, R)
        trayectoria.append(p_int)
        n = p_int / np.linalg.norm(p_int) # Normal en el círculo
        d = reflexion_vectorial(d, n)
        p = p_int

    return np.array(trayectoria)

def simular_trayectoria_elipse(theta_grados, n_rebotes=50, a=2.0, b=1.0):
    """Simula la elipse disparando desde el foco F1 (propiedad del póster)."""
    c = np.sqrt(a**2 - b**2)
    p = np.array([c, 0.0]) # Foco F1
    theta = np.deg2rad(theta_grados)
    d = np.array([np.cos(theta), np.sin(theta)])
    trayectoria = [p]

    for _ in range(n_rebotes):
        p_int = interseccion_elipse(p, d, a, b)
        if p_int is None: break
        trayectoria.append(p_int)
        n = normal_elipse(p_int, a, b)
        d = reflexion_vectorial(d, n)
        p = p_int

    return np.array(trayectoria)

# ==========================================
# 3. VARIACIÓN MANUAL (SLIDERS INTERACTIVOS)
# ==========================================

def plot_interactivo(alpha_circulo, theta_elipse, n_rebotes):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # --- Círculo ---
    tray_c = simular_trayectoria_circulo(alpha_circulo, n_rebotes)
    ax1.plot(tray_c[:, 0], tray_c[:, 1], 'r.-', lw=1, ms=5, alpha=0.7)
    t = np.linspace(0, 2*np.pi, 100)
    ax1.plot(np.cos(t), np.sin(t), 'k--', alpha=0.5)
    ax1.set_title(f'Círculo (Ángulo Central: {alpha_circulo:.1f}°)\n{"Periódico" if 360 % alpha_circulo == 0 else "Cuasiperiódico"}')
    ax1.set_aspect('equal')
    ax1.set_xlim(-1.1, 1.1); ax1.set_ylim(-1.1, 1.1)
    ax1.grid(True, alpha=0.3)

    # --- Elipse ---
    tray_e = simular_trayectoria_elipse(theta_elipse, n_rebotes)
    ax2.plot(tray_e[:, 0], tray_e[:, 1], 'b.-', lw=1, ms=5, alpha=0.7)
    ax2.plot(2*np.cos(t), 1*np.sin(t), 'k--', alpha=0.5)
    ax2.plot([np.sqrt(3), -np.sqrt(3)], [0, 0], 'ko', label='Focos')
    ax2.set_title(f'Elipse (Disparo desde Foco F1 a {theta_elipse:.1f}°)')
    ax2.set_aspect('equal')
    ax2.set_xlim(-2.1, 2.1); ax2.set_ylim(-1.1, 1.1)
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.show()

print("=== SIMULACIÓN INTERACTIVA (VARIACIÓN MANUAL) ===")
interact(plot_interactivo,
         alpha_circulo=FloatSlider(value=120, min=10, max=350, step=1, description='Ángulo Central:'),
         theta_elipse=FloatSlider(value=45, min=0, max=360, step=1, description='Ángulo Elipse:'),
         n_rebotes=IntSlider(value=50, min=5, max=200, step=5, description='Rebotes:'))

# ==========================================
# 4. VARIACIÓN AUTOMÁTICA EN EL TIEMPO (ANIMACIÓN)
# ==========================================

print("\n=== SIMULACIÓN AUTOMÁTICA (VARIACIÓN EN EL TIEMPO) ===")
fig_anim, (ax1_anim, ax2_anim) = plt.subplots(1, 2, figsize=(14, 7))

linea_circ, = ax1_anim.plot([], [], 'r-', lw=1.5)
linea_elip, = ax2_anim.plot([], [], 'b-', lw=1.5)

ax1_anim.set_xlim(-1.1, 1.1); ax1_anim.set_ylim(-1.1, 1.1); ax1_anim.set_aspect('equal')
ax1_anim.set_title('Círculo (Ángulo Central Variando Automáticamente)')
ax2_anim.set_xlim(-2.1, 2.1); ax2_anim.set_ylim(-1.1, 1.1); ax2_anim.set_aspect('equal')
ax2_anim.set_title('Elipse (Trayectoria desde Foco Variando)')

t_circ = np.linspace(0, 2*np.pi, 100)
ax1_anim.plot(np.cos(t_circ), np.sin(t_circ), 'k--', alpha=0.3)
t_elip = np.linspace(0, 2*np.pi, 100)
ax2_anim.plot(2*np.cos(t_elip), 1*np.sin(t_elip), 'k--', alpha=0.3)

estado = {'alpha': 30, 'theta': 20}

def init():
    linea_circ.set_data([], [])
    linea_elip.set_data([], [])
    return linea_circ, linea_elip

def update(frame):
    # Variación constante en el tiempo
    estado['alpha'] += 1.5
    estado['theta'] += 2.0

    if estado['alpha'] > 360: estado['alpha'] = 10
    if estado['theta'] > 360: estado['theta'] = 0

    # Simular y actualizar Círculo
    tray_c = simular_trayectoria_circulo(estado['alpha'], 40)
    linea_circ.set_data(tray_c[:, 0], tray_c[:, 1])
    ax1_anim.set_title(f'Círculo (Ángulo Central: {estado["alpha"]:.0f}°)')

    # Simular y actualizar Elipse
    tray_e = simular_trayectoria_elipse(estado['theta'], 40)
    linea_elip.set_data(tray_e[:, 0], tray_e[:, 1])
    ax2_anim.set_title(f'Elipse (Disparo desde Foco: {estado["theta"]:.0f}°)')

    return linea_circ, linea_elip

anim = FuncAnimation(fig_anim, update, frames=240, init_func=init, interval=50, blit=True)
plt.close()
HTML(anim.to_jshtml())