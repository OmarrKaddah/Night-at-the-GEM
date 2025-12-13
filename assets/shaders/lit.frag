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
        
        // Spotlight falloff with elliptical shape (oval)
        if (light.type == SPOT) {
            vec3 light_forward = normalize(-light.direction);
            float theta = dot(light_dir, light_forward);
            
            // Create elliptical falloff (wider horizontally for realistic flashlight)
            // Get the right and up vectors relative to light direction
            vec3 light_right = normalize(cross(light_forward, vec3(0.0, 1.0, 0.0)));
            if (length(light_right) < 0.1) {
                light_right = normalize(cross(light_forward, vec3(1.0, 0.0, 0.0)));
            }
            vec3 light_up = normalize(cross(light_right, light_forward));
            
            // Project light_dir onto the plane perpendicular to light_forward
            vec3 to_surface = light_dir - light_forward * theta;
            float horizontal_dist = abs(dot(to_surface, light_right));
            float vertical_dist = abs(dot(to_surface, light_up));
            
            // Create slightly oval shape: closer to circle but not perfect (1.3x wider horizontally)
            // Use a simpler approach: slight horizontal stretch for realistic flashlight
            float oval_radius = sqrt(horizontal_dist * horizontal_dist * 1.3 + vertical_dist * vertical_dist * 0.9);
            
            // Calculate angle from center using the oval shape
            float angle_from_center = atan(oval_radius / max(theta, 0.01));
            float inner_angle_rad = acos(light.inner_angle);
            float outer_angle_rad = acos(light.outer_angle);
            
            // More realistic smooth falloff with exponential curve
            float angle_factor = (angle_from_center - inner_angle_rad) / (outer_angle_rad - inner_angle_rad);
            float intensity = 1.0 - smoothstep(0.0, 1.0, clamp(angle_factor, 0.0, 1.0));
            // Use exponential falloff for more realistic light distribution
            intensity = pow(intensity, 2.0); // More gradual, realistic falloff
            
            attenuation *= intensity;
        }
    }
    
    // Diffuse (Lambert) - more realistic with smoother falloff
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
                float bias = 0.01;
                
                // Check if current fragment is in shadow
                // In OpenGL depth buffer: 0 = near plane (close to light), 1 = far plane (far from light)
                // Objects closer to light have smaller depth values
                // If currentDepth > closestDepth, we're farther from light than the closest object = in shadow
                // closestDepth of 1.0 means empty space (no object blocking), so not in shadow
                
                // The key: we're checking if THIS fragment (on the ground/wall) is behind something
                // that was rendered to the shadow map (like a zombie)
                // When a zombie is between the light and the ground, the ground's currentDepth will be
                // greater than the zombie's closestDepth in the shadow map
                
                // Try a simpler, more aggressive check
                if (closestDepth < 0.999) {
                    // There's something in the shadow map at this location
                    if (currentDepth > closestDepth + bias) {
                        // We're behind that something = in shadow
                        shadow = 0.0; // Completely black shadow
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
