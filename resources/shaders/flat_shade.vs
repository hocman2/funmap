#version 330

layout (location = 0) in vec3 vertexPosition;
layout (location = 2) in vec3 vertexNormal;

uniform mat4 matView;
uniform mat4 matProjection;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 fragPosition;
out vec3 fragNormal;

void main() {
  fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));
  fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));

  gl_Position = matProjection * matView * matModel * vec4(vertexPosition, 1.0);
}
