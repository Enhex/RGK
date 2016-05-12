#include "path_tracer.hpp"

#include "camera.hpp"
#include "scene.hpp"

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/norm.hpp>

Radiance PathTracer::RenderPixel(int x, int y, unsigned int & raycount, bool debug){
    Radiance total;

    // N - rooks
    std::vector<unsigned int> V(multisample);
    for(unsigned int i = 0; i < V.size(); i++) V[i] = i;
    std::random_shuffle(V.begin(), V.end());

    for(unsigned int i = 0; i < V.size(); i++){
        Ray r = camera.GetSubpixelRay(x, y, xres, yres, i, V[i], multisample);
        total += TraceRay(r, raycount, debug);
    }

    return total * (1.0f / multisample);
}

Radiance PathTracer::TraceRay(const Ray& r, unsigned int& raycount, bool debug){
    raycount++;
    Intersection i = scene.FindIntersectKd(r, debug);

    if(i.triangle){

        const Material& mat = i.triangle->GetMaterial();

        glm::vec3 pos = r[i.t];
        glm::vec3 N = i.Interpolate(i.triangle->GetNormalA(),
                                    i.triangle->GetNormalB(),
                                    i.triangle->GetNormalC());
        // Outgoing direction
        glm::vec3 Vr = -r.direction;

        // Tecture sampling
        glm::vec2 texUV;
        if(mat.ambient_texture || mat.diffuse_texture || mat.specular_texture){
            texUV = i.Interpolate(i.triangle->GetTexCoordsA(),
                                  i.triangle->GetTexCoordsB(),
                                  i.triangle->GetTexCoordsC());
        }

        Color diffuse  =  mat.diffuse_texture ?  mat.diffuse_texture->GetPixelInterpolated(texUV,debug) : mat.diffuse ;
        Color specular = mat.specular_texture ? mat.specular_texture->GetPixelInterpolated(texUV,debug) : mat.specular;
        //Color ambient  =  mat.ambient_texture ?  mat.ambient_texture->GetPixelInterpolated(texUV,debug) : mat.ambient ;

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

        Radiance total;

        for(unsigned int qq = 0; qq < lights.size(); qq++){
            const Light& l = lights[qq];

            // Visibility factor
            if(debug) std::cout << pos << std::endl;
            if(debug) std::cout << l.pos << std::endl;

            if(!scene.Visibility(l.pos, pos)) continue;

            // Incoming direction
            glm::vec3 Vi = glm::normalize(l.pos - pos);
            // Ideal specular direction w.r.t. Vi
            glm::vec3 Vs = 2.0f * glm::dot(Vi, N) * N - Vi;

            float c = glm::max(0.0f, glm::cos( glm::angle(Vr,Vs) ) );
            c = glm::pow(c, mat.exponent);

            // BRDF
            Color f = diffuse + specular * c;
            if(debug) std::cout << "f = " << f << std::endl;

            float G = glm::max(0.0f, glm::cos( glm::angle(N, Vi) )) / glm::distance2(l.pos, pos);
            if(debug) std::cout << "G = " << G << std::endl;

            Radiance inc_l = Radiance(l.color) * l.intensity;

            Radiance out = inc_l * f * G;
            if(debug) std::cout << out << std::endl;
            total += out;
        }
        if(debug) std::cerr << std::endl << "total: " << total << std::endl;
        return total;
    }else{
        return Radiance(sky_color);
    }
}
