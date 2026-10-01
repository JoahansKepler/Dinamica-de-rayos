import ipywidgets as widgets

def plot_interactivo(angulo_grados, n_rebotes):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    theta = np.deg2rad(angulo_grados)

    # Círculo
    tray_c = simular_trayectoria('circulo', {'R': 1, 'theta': theta}, n_rebotes)
    ax1.plot(tray_c[:, 0], tray_c[:, 1], 'r.-', lw=1, ms=4)
    t = np.linspace(0, 2*np.pi, 100)
    ax1.plot(np.cos(t), np.sin(t), 'k--', alpha=0.5)
    ax1.set_title(f'Círculo (Ángulo: {angulo_grados:.1f}°)')
    ax1.set_aspect('equal')
    ax1.set_xlim(-1.1, 1.1); ax1.set_ylim(-1.1, 1.1)

    # Elipse
    tray_e = simular_trayectoria('elipse', {'a': 2, 'b': 1, 'theta': theta}, n_rebotes)
    ax2.plot(tray_e[:, 0], tray_e[:, 1], 'b.-', lw=1, ms=4)
    ax2.plot(2*np.cos(t), 1*np.sin(t), 'k--', alpha=0.5)
    ax2.plot([np.sqrt(3), -np.sqrt(3)], [0, 0], 'ko', label='Focos')
    ax2.set_title('Elipse (Disparo desde Foco)')
    ax2.set_aspect('equal')
    ax2.set_xlim(-2.1, 2.1); ax2.set_ylim(-1.1, 1.1)

    plt.tight_layout()
    plt.show()

# Crear controles deslizantes
interact(plot_interactivo,
         angulo_grados=FloatSlider(value=45, min=0, max=360, step=1, description='Ángulo (°):'),
         n_rebotes=IntSlider(value=50, min=5, max=200, step=5, description='Rebotes:'))