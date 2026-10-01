from matplotlib.animation import FuncAnimation
from IPython.display import HTML

# Configuración de la figura
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 7))

# Inicializar elementos gráficos
linea_circ, = ax1.plot([], [], 'r-', lw=1)
linea_elip, = ax2.plot([], [], 'b-', lw=1)

# Configurar límites
ax1.set_xlim(-1.1, 1.1); ax1.set_ylim(-1.1, 1.1); ax1.set_aspect('equal')
ax1.set_title('Círculo (Ángulo Variando en el Tiempo)')
ax2.set_xlim(-2.1, 2.1); ax2.set_ylim(-1.1, 1.1); ax2.set_aspect('equal')
ax2.set_title('Elipse (Trayectoria desde Foco)')

# Dibujar fronteras
t_circ = np.linspace(0, 2*np.pi, 100)
ax1.plot(np.cos(t_circ), np.sin(t_circ), 'k--', alpha=0.3)
t_elip = np.linspace(0, 2*np.pi, 100)
ax2.plot(2*np.cos(t_elip), 1*np.sin(t_elip), 'k--', alpha=0.3)

# Parámetros iniciales
estado = {'angulo': 0}

def init():
    linea_circ.set_data([], [])
    linea_elip.set_data([], [])
    return linea_circ, linea_elip

def update(frame):
    # Modificamos el ángulo constantemente (variación automática)
    estado['angulo'] += 0.02
    theta = estado['angulo']

    # Simular círculo
    tray_c = simular_trayectoria('circulo', {'R': 1, 'theta': theta}, 30)
    linea_circ.set_data(tray_c[:, 0], tray_c[:, 1])

    # Simular elipse
    tray_e = simular_trayectoria('elipse', {'a': 2, 'b': 1, 'theta': theta}, 30)
    linea_elip.set_data(tray_e[:, 0], tray_e[:, 1])

    return linea_circ, linea_elip

anim = FuncAnimation(fig, update, frames=200, init_func=init, interval=50, blit=True)
plt.close() # Evita que se muestre la gráfica estática
HTML(anim.to_jshtml()) # Renderiza el video interactivo en Colab