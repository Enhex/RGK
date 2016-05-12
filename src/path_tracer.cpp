#include "path_tracer.hpp"

#include "camera.hpp"
#include "scene.hpp"

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/rotate_vector.hpp>

// Returns a cosine-distributed random vector on a hemisphere, such that y > 0.
glm::vec3 HSRandCos(){
    glm::vec2 p = glm::diskRand(1.0f);
    float y = glm::sqrt(1- p.x*p.x - p.y*p.y);
    return glm::vec3(p.x, y, p.y);
}

// Returns a cosine-distributed random vector in direction of V.
glm::vec3 HSRandCosDir(glm::vec3 V){
    glm::vec3 r = HSRandCos();
    glm::vec3 axis = glm::cross(glm::vec3(0.0,1.0,0.0), V);
    // TODO: What it V = -up?
    if(glm::length(axis) < 0.0001f) return r;
    axis = glm::normalize(axis);
    float angle = glm::angle(glm::vec3(0.0,1.0,0.0), V);
    return glm::rotate(r, angle, axis);
}

Radiance PathTracer::RenderPixel(int x, int y, unsigned int & raycount, bool debug){
    Radiance total;

    if(debug) std::cout << std::endl;

    // N - rooks
    std::vector<unsigned int> V(multisample);
    for(unsigned int i = 0; i < V.size(); i++) V[i] = i;
    std::random_shuffle(V.begin(), V.end());

    for(unsigned int i = 0; i < V.size(); i++){
        Ray r;
        if(camera.IsSimple()){
            r = camera.GetSubpixelRay(x, y, xres, yres, i, V[i], multisample);
        }else{
            r = camera.GetSubpixelRayLens(x, y, xres, yres, i, V[i], multisample);
        }
        total += TracePath(r, raycount, debug);
    }

    return total * (1.0f / multisample);
}

Radiance PathTracer::TracePath(const Ray& r, unsigned int& raycount, bool debug){

    // First, generate a path.
    struct PathPoint{
        bool infinity = false;
        Intersection i;
        glm::vec3 pos;
        glm::vec3 N;
        glm::vec3 Vr; // outgoing direction (pointing towards previous path point)
        glm::vec3 Vi; // incoming direction (pointing towards next path point)
        glm::vec2 texUV;
        Radiance to_prev;
    };
    std::vector<PathPoint> path;

    Ray current_ray = r;
    for(int n = 0; n < 2; n++){

        if(debug) std::cout << "Generating path, n = " << n << std::endl;

        raycount++;
        Intersection i = scene.FindIntersectKd(current_ray, debug);
        PathPoint p;
        p.i = i;
        if(!i.triangle){
            p.infinity = true;
            p.Vr = -r.direction;
            path.push_back(p);
            break;
        }else{
            p.pos = current_ray[i.t];
            p.N = i.Interpolate(i.triangle->GetNormalA(),
                                i.triangle->GetNormalB(),
                                i.triangle->GetNormalC());
            p.Vr = -r.direction;
            const Material& mat = i.triangle->GetMaterial();
            if(mat.ambient_texture || mat.diffuse_texture || mat.specular_texture){
                p.texUV = i.Interpolate(i.triangle->GetTexCoordsA(),
                                        i.triangle->GetTexCoordsB(),
                                        i.triangle->GetTexCoordsC());
            }
            if(mat.bump_texture){
                float right = mat.bump_texture->GetSlopeRight(p.texUV);
                float bottom = mat.bump_texture->GetSlopeBottom(p.texUV);
                glm::vec3 tangent = i.Interpolate(i.triangle->GetTangentA(),
                                                  i.triangle->GetTangentB(),
                                                  i.triangle->GetTangentC());
                glm::vec3 bitangent = glm::normalize(glm::cross(p.N,tangent));
                p.N = glm::normalize(p.N + (tangent*right + bitangent*bottom) * bumpmap_scale);
            }
            // Generate next ray

            glm::vec3 dir = HSRandCosDir(p.N);


            glm::vec3 r = HSRandCos();
            glm::vec3 axis = glm::cross(glm::vec3(0.0,1.0,0.0), p.N);
            if(glm::length(axis) < 0.0001f) dir = r;
            else{
                axis = glm::normalize(axis);
                float angle = glm::angle(glm::vec3(0.0,1.0,0.0), p.N);
                dir = glm::rotate(r, angle, axis);
            }

            if(debug) std::cout << "pos: " << p.pos << std::endl;
            if(debug) std::cout << "r: " << r << std::endl;
            if(debug) std::cout << "axis: " << axis << std::endl;
            if(debug) std::cout << "dir: " << dir  << " " << glm::length(dir) << std::endl;
            p.Vr = dir;

            path.push_back(p);

            current_ray = Ray(p.pos + dir * scene.epsilon * 100.0f, dir);
            // Continue for next ray
        }
    }


    for(int n = path.size()-1; n >= 0; n--){
        if(debug) std::cout << "--- Processing PP " << n << std::endl;

        bool last = ((unsigned int)n == path.size()-1);
        PathPoint& pp = path[n];
        if(pp.infinity){
            pp.to_prev = Radiance(sky_color);
        }else{
            const Material& mat = pp.i.triangle->GetMaterial();

            Color diffuse  =  mat.diffuse_texture?mat.diffuse_texture->GetPixelInterpolated(pp.texUV,debug) : mat.diffuse ;
            Color specular = mat.specular_texture?mat.specular_texture->GetPixelInterpolated(pp.texUV,debug): mat.specular;

            Radiance total;

            // Direct lighting, random light
            int light_n = rand()%(lights.size());
            const Light& l = lights[light_n];

            // Visibility factor
            if(scene.Visibility(l.pos, pp.pos)){

                // Incoming direction
                glm::vec3 Vi = glm::normalize(l.pos - pp.pos);
                // Ideal specular direction w.r.t. Vi
                glm::vec3 Vs = 2.0f * glm::dot(Vi, pp.N) * pp.N - Vi;

                // BRDF
                float c = glm::max(0.0f, glm::cos( glm::angle(pp.Vr,Vs) ) );
                c = glm::pow(c, mat.exponent);
                Color f = diffuse + specular * c;

                if(debug) std::cout << "f = " << f << std::endl;

                float G = glm::max(0.0f, glm::cos( glm::angle(pp.N, Vi) )) / glm::distance2(l.pos, pp.pos);
                if(debug) std::cout << "G = " << G << std::endl;

                Radiance inc_l = Radiance(l.color) * l.intensity;

                Radiance out = inc_l * f * G;
                if(debug) std::cout << out << std::endl;
                total += out;
            }

            // indirect lighting
            if(!last){
                // look at next pp's to_prev and incorporate it here
                Radiance incoming = path[n+1].to_prev;

                // Incoming direction
                glm::vec3 Vi = pp.Vi;

                // Ideal specular direction w.r.t. Vi
                glm::vec3 Vsi = 2.0f * glm::dot(Vi, pp.N) * pp.N - Vi;

                // BRDF
                float c = glm::max(0.0f, glm::cos( glm::angle(pp.Vr,Vsi) ) );
                c = glm::pow(c, mat.exponent);
                Color f = diffuse + specular * c;

                total += incoming * f;
            }

            if(debug) std::cerr << std::endl << "total: " << total << std::endl;
            pp.to_prev = total;
        }
    }
    return path[0].to_prev;
}
