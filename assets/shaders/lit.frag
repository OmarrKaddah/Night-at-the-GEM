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
uniform sampler2D shadow_map;
uniform mat4 light_space_matrix;
uniform int use_shadows;
uniform int flashlight_index;

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

        if (light.type == SPOT) {
            vec3 light_forward = normalize(-light.direction);
            float theta = dot(light_dir, light_forward);
            
            // EXPLICIT FIXED: Kill any light projecting backwards
            if (theta <= 0.0) {
                 attenuation = 0.0;
            } else {
                float epsilon = light.inner_angle - light.outer_angle;
                float intensity = clamp((theta - light.outer_angle) / epsilon, 0.0, 1.0);
                attenuation *= intensity;
            }
        }
    }

    // Diffuse (Lambert) - standard one-sided lighting
    float diff = max(dot(normal, light_dir), 0.0);
    // Add slight rim lighting for more realistic appearance
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 2.0) * 0.1;
    vec3 diffuse = (diff + rim) * albedo;

    // Specular (Blinn-Phong) with more realistic distribution
    vec3 halfway = normalize(light_dir + view_dir);
    float shininess = (1.0 - rough) * 128.0 + 8.0;
    float spec = pow(max(dot(normal, halfway), 0.0), shininess);
    // Reduce specular intensity for more realistic flashlight
    vec3 specular = spec * spec_color * 0.7;

    // More realistic light contribution with distance-based intensity
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
    
    // Disabled vertex color multiplication - causes issues with some meshes
    // vec3 vertColor = fs_in.color.rgb;
    // float colorSum = vertColor.r + vertColor.g + vertColor.b;
    // if (colorSum > 0.01) {
    //     albedo *= vertColor;
    // }
    
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

    // Calculate shadow factor for flashlight
    float shadow = 1.0;

    if (use_shadows == 1 && flashlight_index >= 0 && flashlight_index < light_count && lights[flashlight_index].type == SPOT) {
        // Transform fragment position to light space
        vec4 fragPosLightSpace = light_space_matrix * vec4(fs_in.world_pos, 1.0);

        // Check if behind the light (w < 0 means behind)
        if (fragPosLightSpace.w > 0.0) {
            // Perspective divide
            vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            // Transform to [0,1] range
            projCoords = projCoords * 0.5 + 0.5;

            // Check if fragment is in shadow map bounds
            if (projCoords.x >= 0.0 && projCoords.x <= 1.0 && 
                projCoords.y >= 0.0 && projCoords.y <= 1.0 &&
                projCoords.z >= 0.0 && projCoords.z <= 1.0) {

                // Sample shadow map depth (stored in red channel)
                float closestDepth = texture(shadow_map, projCoords.xy).r;
                float currentDepth = projCoords.z;

                // TEMPORARY DEBUG: Visualize shadow map to verify it's working
                // Uncomment the next 2 lines to see the shadow map as a gradient
                // If you see a gradient pattern, the shadow map IS working!
                // frag_color = vec4(closestDepth, closestDepth, closestDepth, 1.0);
                // return;

                // Also try visualizing currentDepth to see if coordinates are correct
                // frag_color = vec4(currentDepth, currentDepth, currentDepth, 1.0);
                // return;

                // Shadow bias to prevent shadow acne
                float bias = 0.005;

                // Check if current fragment is in shadow
                if (closestDepth < 0.999) {
                    // There's something in the shadow map at this location
                    if (currentDepth > closestDepth + bias) {
                        // We're behind that something = in shadow
                        shadow = 0.2; // Dark shadow but not completely black
                    } else {
                        shadow = 1.0; // Not in shadow
                    }
                } else {
                    // Empty space in shadow map = not in shadow
                    shadow = 1.0;
                }
            }
        }
    }

    // Add contribution from each light
    for (int i = 0; i < light_count && i < MAX_LIGHTS; i++) {
        vec3 lightContribution = calc_light(lights[i], normal, view_dir, albedo, spec_color, rough);
        // Apply shadow only to the flashlight
        if (i == flashlight_index && use_shadows == 1) {
            lightContribution *= shadow; // Multiply by shadow factor (1.0 = lit, 0.3 = shadowed)
        }
        result += lightContribution;
    }

    // Add emissive
    result += emissive;
    
    frag_color = vec4(result, 1.0);
}