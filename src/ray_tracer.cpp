#include "ray_tracer.hpp"

#include "camera.hpp"
#include "scene.hpp"

Radiance RayTracer::RenderPixel(int x, int y, unsigned int & raycount, bool debug){
    Radiance pixel_total(0.0, 0.0, 0.0);
    for(unsigned int my = 0; my < multisample; my++){
        for(unsigned int mx = 0; mx < multisample; mx++){
            unsigned int mx2 = (my % 2 == 0) ? (multisample - mx -1) : mx;
            Ray r;
            if(camera.IsSimple()){
                r = camera.GetSubpixelRay(x, y, xres, yres, mx2, my, multisample);
            }else{
                r = camera.GetRandomRayLens(x, y, xres, yres);
                //r = camera.GetSubpixelRayLens(x, y, task.xres, task.yres, mx2, my, m);
            }
            pixel_total += Radiance(TraceRay(r, depth, raycount, debug));
        }
    }
    return pixel_total * (1.0f / (multisample*multisample));
}


Color RayTracer::TraceRay(const Ray& r, unsigned int depth, unsigned int& raycount, bool debug){
    if(debug) std::cerr << "Debugging a ray. " << std::endl;
    if(debug) std::cerr << r.origin << " " << r.direction << std::endl;
    raycount++;
    Intersection i = scene.FindIntersectKd(r, debug);

    if(i.triangle){
        if(debug) std::cerr << "Intersection found. " << std::endl;
        const Material& mat = i.triangle->GetMaterial();
        Color total(0.0, 0.0, 0.0);

        glm::vec3 ipos = r[i.t];
        glm::vec3 N = i.Interpolate(i.triangle->GetNormalA(),
                                    i.triangle->GetNormalB(),
                                    i.triangle->GetNormalC());
        glm::vec3 V = -r.direction; // incoming direction

        glm::vec2 texUV;
        if(mat.ambient_texture || mat.diffuse_texture || mat.specular_texture){
            texUV = i.Interpolate(i.triangle->GetTexCoordsA(),
                                  i.triangle->GetTexCoordsB(),
                                  i.triangle->GetTexCoordsC());
        }

        Color diffuse  =  mat.diffuse_texture ?  mat.diffuse_texture->GetPixelInterpolated(texUV,debug) : mat.diffuse ;
        Color specular = mat.specular_texture ? mat.specular_texture->GetPixelInterpolated(texUV,debug) : mat.specular;
        Color ambient  =  mat.ambient_texture ?  mat.ambient_texture->GetPixelInterpolated(texUV,debug) : mat.ambient ;

        if(mat.bump_texture){
            //diffuse = Color(0.5, 0.5, 0.5);

            float right = mat.bump_texture->GetSlopeRight(texUV);
            float bottom = mat.bump_texture->GetSlopeBottom(texUV);
            glm::vec3 tangent = i.Interpolate(i.triangle->GetTangentA(),
                                              i.triangle->GetTangentB(),
                                              i.triangle->GetTangentC());
            glm::vec3 bitangent = glm::normalize(glm::cross(N,tangent));

            N = glm::normalize(N + (tangent*right + bitangent*bottom) * bumpmap_scale);
        }

        if(debug) std::cerr << "Was hit. color is " << diffuse << std::endl;

        for(unsigned int qq = 0; qq < lights.size(); qq++){
            const Light& l = lights[qq];
            glm::vec3 L = glm::normalize(l.pos - ipos);
            const Triangle* shadow_triangle = nullptr;
            if(depth > 0){
                // Search for shadow triangle.
                Ray ray_to_light(ipos, l.pos, scene.epsilon * 2.0f * glm::length(ipos - l.pos));
                // First, try looking within shadow cache.
                if(debug) std::cout << "raytolight origin:" << ray_to_light.origin << ", dir" << ray_to_light.direction << std::endl;
                for(const Triangle* tri : shadow_cache[qq]){
                    float t,a,b;
                    raycount++;
                    if(tri->TestIntersection(ray_to_light, t, a, b, debug)){
                        if(t < ray_to_light.near - scene.epsilon || t > ray_to_light.far + scene.epsilon) continue;
                        // Intersection found.
                        if(debug) std::cout << "Shadow found in cache at " << tri << "." << std::endl;
                        if(debug) std::cout << "Triangle " << tri->GetVertexA() << std::endl;
                        if(debug) std::cout << "Triangle " << tri->GetVertexB() << std::endl;
                        if(debug) std::cout << "Triangle " << tri->GetVertexC() << std::endl;
                        if(debug) std::cout << "t " << t << std::endl;
                        shadow_triangle = tri;
                        break;
                    }
                }
                // Skip manual search when a shadow triangle was found in cache
                if(!shadow_triangle){
                    raycount++;
                    shadow_triangle = scene.FindIntersectKdAny(ray_to_light);
                }
            }
            if(!shadow_triangle){ // no intersection found
                //TODO: use interpolated normals

                float distance = glm::length(ipos - l.pos); // The distance to light
                if(debug) std::cout << "Distance to light : " << distance << std::endl;
                float d = distance * distance;

                /*
                float dist_func = 1.0f/(2.5f + d); // Light intensity falloff function
                if(debug) std::cout << "Dist func : " << dist_func << std::endl;
                float intens_factor = l.intensity * 0.18f * dist_func;
                */

                float dist_func = 1.0f/(3.0f + d)/4.85f; // Light intensity falloff function
                if(debug) std::cout << "Dist func : " << dist_func << std::endl;
                float intens_factor = l.intensity * 1.0f * dist_func;

                if(debug) std::cerr << "No shadow, distance: " << distance << std::endl;

                float kD = glm::dot(N, L);
                kD = glm::max(0.0f, kD);
                total += intens_factor * l.color * diffuse * kD;


                if(debug) std::cerr << "N " << N << std::endl;
                if(debug) std::cerr << "L " << L << std::endl;
                if(debug) std::cerr << "kD " << kD << std::endl;
                if(debug) std::cerr << "Total: " << total << std::endl;


                if(mat.exponent > 1.0f){
                    glm::vec3 R = 2.0f * glm::dot(L, N) * N - L;
                    float a = glm::dot(R, V);
                    a = glm::max(0.0f, a);
                    if(debug) std::cerr << "a: " << a << std::endl;
                    if(debug) std::cerr << "specular: " << specular << std::endl;
                    float kS = glm::pow(a, mat.exponent);
                    // if(std::isnan(kS)) std::cout << glm::dot(R,V) << "/" << mat.exponent << std::endl;
                    if(debug) std::cerr << "spec add: " << intens_factor * l.color * specular * kS * 1.0f << std::endl;
                    total += intens_factor * l.color * specular * kS * 1.0f;
                }

            }else{
                if(debug) std::cerr << "Shadow found." << std::endl;
                // Update the shadow buffer for this light source
                shadow_cache[qq].Use(shadow_triangle);
            }
        }

        // Special case for when there is no light
        if(lights.size() == 0){
            total += diffuse;
        }

        // Ambient lighting
        total += ambient * 0.1;

        // Next ray
        if(depth >= 2 && mat.exponent < 1.0f){
            glm::vec3 refl = 2.0f * glm::dot(V, N) * N - V;
            Ray refl_ray(ipos, ipos + refl, 0.01);
            refl_ray.far = 1000.0f;
            Color reflection = TraceRay(refl_ray, depth-1, raycount);
            total = mat.exponent * reflection + (1.0f - mat.exponent) * total;
        }
        if(debug) std::cout << "Total: " << total << std::endl;
        return total;
    }else{
        // Black background for void spaces
        return sky_color;
    }
}
