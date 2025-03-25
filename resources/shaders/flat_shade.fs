#version 330

in vec3 fragPosition;
in vec3 fragNormal;

uniform vec4 albedoColor;
uniform vec3 viewPos;

out vec4 finalColor;

void main() {
    vec3 lightPos = vec3(0.0);
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diffuseIntensity = max(0.0, dot(normalize(fragNormal), lightDir));
    vec3 lightCol = vec3(1.0) * diffuseIntensity;
    finalColor = vec4(normalize(fragNormal), 1.0);
}
