#include "path_tracer.hpp"

#include "camera.hpp"
#include "scene.hpp"

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/constants.hpp>

// Returns a cosine-distributed random vector on a hemisphere, such that y > 0.
glm::vec3 HSRandCos(){
    glm::vec2 p = glm::diskRand(1.0f);
    float y = glm::sqrt(glm::max(0.0f, 1- p.x*p.x - p.y*p.y));
    return glm::vec3(p.x, y, p.y);
}

// Returns a cosine-distributed random vector in direction of V.
glm::vec3 HSRandCosDir(glm::vec3 V){
    glm::vec3 r = HSRandCos();
    glm::vec3 axis = glm::cross(glm::vec3(0.0,1.0,0.0), V);
    // TODO: What if V = -up?
    if(glm::length(axis) < 0.0001f) return (V.y > 0) ? r : -r;
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

    if(debug) std::cout << "-----> pixel average: " << total/multisample << std::endl;

    return total / multisample;
}

Radiance PathTracer::TracePath(const Ray& r, unsigned int& raycount, bool debug){

    // First, generate a path.
    struct PathPoint{
        bool infinity = false;
        Intersection i;
        glm::vec3 pos;
        glm::vec3 lightN;
        glm::vec3 faceN;
        glm::vec3 Vr; // outgoing direction (pointing towards previous path point)
        glm::vec3 Vi; // incoming direction (pointing towards next path point)
        glm::vec2 texUV;
        Radiance to_prev;
    };
    std::vector<PathPoint> path;

    Ray current_ray = r;
    unsigned int n = 0;
    const Triangle* last_triangle = nullptr;
    while(true){
        n++;
        if(russian >= 0.0f){
            // Russian roulette path termination
            if(n > 1 && glm::linearRand(0.0f, 1.0f) > russian) break;
        }else{
            // Fixed depth path termination
            if(n > depth) break;
        }

        if(debug) std::cout << "Generating path, n = " << n << std::endl;

        raycount++;
        Intersection i = scene.FindIntersectKdOtherThan(current_ray, last_triangle, debug);
        PathPoint p;
        p.i = i;
        if(!i.triangle){
            p.infinity = true;
            p.Vr = -r.direction;
            path.push_back(p);
            break;
        }else{
            if(i.triangle == last_triangle){
                std::cerr << "Ray collided with source triangle. This should never happen." << std::endl;
            }
            p.pos = current_ray[i.t];
            p.faceN = i.Interpolate(i.triangle->GetNormalA(),
                                    i.triangle->GetNormalB(),
                                    i.triangle->GetNormalC());

            p.Vr = -r.direction;

            // Flip normal if tay comes from face's inside
            // if(glm::dot(p.faceN, p.Vr) < 0) p.faceN = -p.faceN;

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
                glm::vec3 bitangent = glm::normalize(glm::cross(p.faceN,tangent));
                p.lightN = glm::normalize(p.faceN + (tangent*right + bitangent*bottom) * bumpmap_scale);
            }else{
                p.lightN = p.faceN;
            }
            // Generate next ray

            glm::vec3 dir = HSRandCosDir(p.faceN);
            while(glm::angle(dir, p.lightN) > glm::pi<float>()/2.0f)
                dir = HSRandCosDir(p.faceN);

            if(debug) std::cout << "pos: " << p.pos << std::endl;
            if(debug) std::cout << "dir: " << dir  << " " << glm::length(dir) << std::endl;
            p.Vi = dir;

            path.push_back(p);

            current_ray = Ray(p.pos + p.faceN * scene.epsilon * 10.0f, dir);

            last_triangle = i.triangle;
            // Continue for next ray
        }
    }


    for(int n = path.size()-1; n >= 0; n--){
        if(debug) std::cout << "--- Processing PP " << n << std::endl;

        bool last = ((unsigned int)n == path.size()-1);
        PathPoint& pp = path[n];
        if(pp.infinity){
            if(debug) std::cout << "This a sky ray, total: " << sky_radiance << std::endl;
            pp.to_prev = sky_radiance;
        }else{
            const Material& mat = pp.i.triangle->GetMaterial();

            if(debug) std::cout << "Hit material: " << mat.name << std::endl;

            Color diffuse  =  mat.diffuse_texture?mat.diffuse_texture->GetPixelInterpolated(pp.texUV,debug) : mat.diffuse ;
            Color specular = mat.specular_texture?mat.specular_texture->GetPixelInterpolated(pp.texUV,debug): mat.specular;

            Radiance total;

            // Direct lighting, random light
            int light_n = rand()%(lights.size());
            const Light& l = lights[light_n];
            glm::vec3 lightpos = l.pos + glm::sphericalRand(l.size);

            if(debug) std::cout << "Incorporating direct lighting component, lightpos: " << lightpos << std::endl;

            // Visibility factor
            if(scene.Visibility(lightpos, pp.pos)){

                if(debug) std::cout << "Light is visible" << std::endl;

                // Incoming direction
                glm::vec3 Vi = glm::normalize(lightpos - pp.pos);

                Radiance f = mat.brdf(pp.lightN, diffuse, specular, Vi, pp.Vr, mat.exponent, 1.0, mat.refraction_index);

                if(debug) std::cout << "f = " << f << std::endl;

                float G = glm::max(0.0f, glm::cos( glm::angle(pp.lightN, Vi) )) / glm::distance2(lightpos, pp.pos);
                if(debug) std::cout << "G = " << G << ", angle " << glm::angle(pp.lightN, Vi) << std::endl;
                if(debug) std::cout << "lightN = " << pp.lightN << ", Vi " << Vi << std::endl;

                Radiance inc_l = Radiance(l.color) * l.intensity;

                Radiance out = inc_l * f * G;
                if(debug) std::cout << "total direct lighting: " << out << std::endl;
                total += out;
            }

            // indirect lighting
            if(!last){
                // look at next pp's to_prev and incorporate it here
                Radiance incoming = path[n+1].to_prev;
                if(debug) std::cout << "Incorporating indirect lighting - incoming radiance: " << incoming << std::endl;

                if(russian > 0.0f) incoming = incoming / russian;

                if(debug) std::cout << "With russian: " << incoming << std::endl;

                // Incoming direction
                glm::vec3 Vi = pp.Vi;

                if(debug) std::cout << "Indirect incoming from: " << Vi << std::endl;

                Radiance f = mat.brdf(pp.lightN, diffuse, specular, Vi, pp.Vr, mat.exponent, 1.0, mat.refraction_index);

                if(debug) std::cout << "BRDF: " << f << std::endl;

                Radiance inc = incoming * f * glm::pi<float>(); // * glm::dot(pp.lightN, Vi);

                if(debug) std::cout << "Incoming * brdf * pi = " << inc << std::endl;


                total += inc;
            }

            if(debug) std::cerr << "total: " << total << std::endl;

            if(total.r > clamp) total.r = clamp;
            if(total.g > clamp) total.g = clamp;
            if(total.b > clamp) total.b = clamp;

            // Whoops.
            if(glm::isnan(total.r) || total.r < 0.0f) total.r = 0.0f;
            if(glm::isnan(total.g) || total.g < 0.0f) total.g = 0.0f;
            if(glm::isnan(total.b) || total.b < 0.0f) total.b = 0.0f;


            pp.to_prev = total;
        }
    }
    if(debug) std::cerr << "PATH TOTAL" << path[0].to_prev << std::endl << std::endl;
    return path[0].to_prev;
}
