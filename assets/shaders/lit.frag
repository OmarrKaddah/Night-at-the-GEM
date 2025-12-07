#version 330 core

#define MAX_LIGHTS 8
#define DIRECTIONAL 0
#define POINT 1
#define SPOT 2

in Varyings {
    vec3 world_pos;
    vec3 world_normal;
    vec2 tex_coord;
    vec4 color;
} fs_in;

out vec4 frag_color;

// Material uniforms
struct Material {
    sampler2D albedo_map;
    sampler2D specular_map;
    sampler2D roughness_map;
    sampler2D ao_map;
    sampler2D emissive_map;
    
    int use_albedo_map;
    int use_specular_map;
    int use_roughness_map;
    int use_ao_map;
    int use_emissive_map;
    
    vec3 albedo;
    vec3 specular;
    vec3 emissive;
    float roughness;
    float ao;
};

// Light uniforms
struct Light {
    int type;          // 0=directional, 1=point, 2=spot
    vec3 position;
    vec3 direction;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
    float inner_angle;  // cos(angle)
    float outer_angle;
};

uniform Material material;
uniform Light lights[MAX_LIGHTS];
uniform int light_count;
uniform vec3 camera_pos;
uniform vec3 ambient_light;

// Calculate light contribution
vec3 calc_light(Light light, vec3 normal, vec3 view_dir, vec3 albedo, vec3 spec_color, float rough) {
    vec3 light_dir;
    float attenuation = 1.0;
    
    if (light.type == DIRECTIONAL) {
        light_dir = normalize(-light.direction);
    } else {
        light_dir = normalize(light.position - fs_in.world_pos);
        float dist = length(light.position - fs_in.world_pos);
        attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);
        
        // Spotlight falloff
        if (light.type == SPOT) {
            float theta = dot(light_dir, normalize(-light.direction));
            float epsilon = light.inner_angle - light.outer_angle;
            float intensity = clamp((theta - light.outer_angle) / epsilon, 0.0, 1.0);
            attenuation *= intensity;
        }
    }
    
    // Diffuse (Lambert)
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 diffuse = diff * albedo;
    
    // Specular (Blinn-Phong)
    vec3 halfway = normalize(light_dir + view_dir);
    float shininess = (1.0 - rough) * 128.0 + 8.0;
    float spec = pow(max(dot(normal, halfway), 0.0), shininess);
    vec3 specular = spec * spec_color;
    
    return (diffuse + specular) * light.color * attenuation;
}

void main() {
    vec3 normal = normalize(fs_in.world_normal);
    vec3 view_dir = normalize(camera_pos - fs_in.world_pos);
    
    // Sample material properties
    vec3 albedo = material.albedo;
    if (material.use_albedo_map == 1) {
        albedo *= texture(material.albedo_map, fs_in.tex_coord).rgb;
    }
    
    // Only multiply by vertex color if it's not black (OBJ files often have black vertex colors)
    vec3 vertColor = fs_in.color.rgb;
    float colorSum = vertColor.r + vertColor.g + vertColor.b;
    if (colorSum > 0.01) {
        albedo *= vertColor;
    }
    
    vec3 spec_color = material.specular;
    if (material.use_specular_map == 1) {
        spec_color *= texture(material.specular_map, fs_in.tex_coord).rgb;
    }
    
    float rough = material.roughness;
    if (material.use_roughness_map == 1) {
        rough *= texture(material.roughness_map, fs_in.tex_coord).r;
    }
    
    float ao_val = material.ao;
    if (material.use_ao_map == 1) {
        ao_val *= texture(material.ao_map, fs_in.tex_coord).r;
    }
    
    vec3 emissive = material.emissive;
    if (material.use_emissive_map == 1) {
        emissive *= texture(material.emissive_map, fs_in.tex_coord).rgb;
    }
    
    // Ambient
    vec3 result = ambient_light * albedo * ao_val;
    
    // Add contribution from each light
    for (int i = 0; i < light_count && i < MAX_LIGHTS; i++) {
        result += calc_light(lights[i], normal, view_dir, albedo, spec_color, rough);
    }
    
    // Add emissive
    result += emissive;
    
    frag_color = vec4(result, 1.0);
}
