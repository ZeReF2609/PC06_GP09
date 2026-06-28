#version 330 core

out vec4 FragColor;

in vec3 fragPos;
in vec3 normal;
in vec2 texCoord;
in vec3 localPos;

uniform vec3 lightPos;
uniform vec3 objectColor;
uniform sampler2D planetTexture;
uniform bool useTexture;
uniform bool isLightSource;

void main()
{
    vec3 texColor = objectColor;
    if (useTexture) {
        texColor = texture(planetTexture, texCoord).rgb;
    }

    if (isLightSource) {
        FragColor = vec4(texColor * 1.4, 1.0);
        return;
    }

    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 viewDir = normalize(-fragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);

    float ambient = 0.12;
    float lighting = ambient + diff * 0.78 + spec * 0.1;

    float rim = 1.0 - max(dot(viewDir, norm), 0.0);
    rim = pow(rim, 3.0) * 0.15;

    FragColor = vec4(texColor * lighting + vec3(rim), 1.0);
}
