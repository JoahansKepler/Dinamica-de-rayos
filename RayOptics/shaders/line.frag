#version 330 core

out vec4 FragColor;

// Color solido en formato RGB, rango [0,1]. Se separa por parametro (en
// vez de fijarlo dentro del shader) para poder reutilizar el MISMO par
// de shaders para dibujar el rayo incidente, los rayos reflejados, las
// normales, la circunferencia, la elipse, etc., cada uno con su propio
// color, sin necesidad de compilar un shader distinto por cada elemento
// visual.
uniform vec3 uColor;

void main()
{
    FragColor = vec4(uColor, 1.0);
}
