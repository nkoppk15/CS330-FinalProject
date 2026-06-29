#version 330 core

layout(location = 0) in vec3 inVertexPosition;

out vec3 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = inVertexPosition;
    vec4 pos = projection * view * vec4(inVertexPosition, 1.0);
    gl_Position = pos.xyww; // forces depth to 1.0 
}