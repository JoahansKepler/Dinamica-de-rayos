#version 330 core

// Posicion del vertice en coordenadas del MUNDO FISICO (las mismas
// unidades que usa Physics::Ray), no en coordenadas de pantalla ni NDC.
layout (location = 0) in vec2 aPos;

// Camara ortografica 2D: define que region del mundo es visible.
// La transformacion es una simple normalizacion afin, sin necesidad de
// una matriz 4x4 completa, porque la camara es una "caja" alineada con
// los ejes (sin rotacion, sin perspectiva): exactamente lo que hace
// falta para un simulador 2D de optica geometrica.
uniform vec2 uCameraCenter;       // centro de la camara, en coords. del mundo
uniform vec2 uCameraHalfExtents;  // semi-ancho y semi-alto visibles, en coords. del mundo

void main()
{
    // Paso 1: trasladar para que el centro de camara quede en el origen.
    // Paso 2: escalar por 1/halfExtents para que la caja visible
    //         [-halfExtents, +halfExtents] quede mapeada a [-1, +1],
    //         que es exactamente el rango de Coordenadas de Dispositivo
    //         Normalizado (NDC) que OpenGL espera en gl_Position.
    vec2 ndc = (aPos - uCameraCenter) / uCameraHalfExtents;

    gl_Position = vec4(ndc, 0.0, 1.0);
}
