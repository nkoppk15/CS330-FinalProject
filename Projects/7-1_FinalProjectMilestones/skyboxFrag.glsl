#version 330 core

in vec3 TexCoords;

out vec4 FragColor;

uniform samplerCube skybox;

void main()
{
   // Force alpha to 1.0 so blending doesn't wash out the skybox	
   FragColor = vec4(texture(skybox, TexCoords).rgb, 1.0);
}