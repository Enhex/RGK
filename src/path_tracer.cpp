#include "path_tracer.hpp"

#include "camera.hpp"
#include "scene.hpp"
#include "global_config.hpp"

#include "glm.hpp"
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/constants.hpp>

PathTracer::PathTracer(const Scene& scene,
                       const Camera& camera,
                       unsigned int xres,
                       unsigned int yres,
                       unsigned int multisample,
                       unsigned int depth,
                       Color sky_color,
                       float sky_brightness,
                       float clamp,
                       float russian,
                       float bumpmap_scale,
                       std::set<const Material*> thinglass,
                       Random rnd)
: Tracer(scene, camera, xres, yres, multisample, bumpmap_scale),
  clamp(clamp),
  russian(russian),
  depth(depth),
  thinglass(thinglass),
  rnd(rnd)
{
    sky_radiance = Radiance(sky_color) * sky_brightness;
}

Radiance PathTracer::RenderPixel(int x, int y, unsigned int & raycount, bool debug){
    Radiance total;

    IFDEBUG std::cout << std::endl;

    // N - rooks
    std::vector<unsigned int> V(multisample);
    for(unsigned int i = 0; i < V.size(); i++) V[i] = i;
    std::random_shuffle(V.begin(), V.end());

    for(unsigned int i = 0; i < V.size(); i++){
        Ray r;
        if(camera.IsSimple()){
            r = camera.GetSubpixelRay(x, y, xres, yres, i, V[i], multisample);
        }else{
            r = camera.GetSubpixelRayLens(x, y, xres, yres, i, V[i], multisample, rnd);
        }
        total += TracePath(r, raycount, debug);
    }

    IFDEBUG std::cout << "-----> pixel average: " << total/multisample << std::endl;

    return total / multisample;
}

float Fresnel(glm::vec3 I, glm::vec3 N, float ior){
    float cosi = glm::dot(I, N);
    float etai = 1.0f, etat = ior;
    if (cosi > 0) { std::swap(etai, etat); }
    // Snell's law
    float sint = etai / etat * glm::sqrt(glm::max(0.f, 1.0f - cosi * cosi));
    if(sint >= 1){
        // Total internal reflection
        return 1.0f;
    }else{
        float cost = sqrtf(std::max(0.f, 1 - sint * sint));
        cosi = fabsf(cosi);
        float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
        float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
        return (Rs * Rs + Rp * Rp) / 2.0f;
    }
}

Radiance PathTracer::TracePath(const Ray& r, unsigned int& raycount, bool debug){

    // This function has the structure of unrolled and flattened recursion.
    // Phase 1 (path creation) corresponds to entering recursive calls.
    // Phase 2 (light transmission) gathers result and returns values
    // upwards. PathPoint represents a single stack frame, and the path
    // vector corresponds to the stack.
    //
    // I never really intented to write it this way, it just naturally came
    // to be so. It would be reasonable to rewrite it as a proper recursion
    // at some point.

    struct PathPoint{
        enum Type{
            SCATTERED,
            REFLECTED,
            ENTERED,
            LEFT,
        };
        Type type;
        bool infinity = false;
        Intersection i;
        glm::vec3 pos;
        glm::vec3 lightN;
        glm::vec3 faceN;
        glm::vec3 Vr; // reflected direction (pointing towards previous path point)
        glm::vec3 Vi; // incoming  direction (pointing towards next path point)
        glm::vec2 texUV;
        Radiance to_prev; // Radiance of light transferred to previous path point
        std::vector<std::pair<const Triangle*,float>> thinglass_isect;
        float sampleP;
        BRDF::BRDFSamplingType sampling_type;
    };
    std::vector<PathPoint> path;

    // ===== 1st Phase =======
    // Generate a light path.

    IFDEBUG std::cout << "Ray direction: " << r.direction << std::endl;

    Ray current_ray = r;
    unsigned int n = 0, n2 = 0;
    // Temporarily setting this to true ensures that russian roulette will not terminate (once).
    bool skip_russian = false;
    // Used for tracking index of refraction
    // float current_ior = 1.0f;
    const Triangle* last_triangle = nullptr;
    while(true){
        n++; n2++;
        if(n2 >= 20) break; // hard limit
        if(russian >= 0.0f){
            // Russian roulette path termination
            if(n > 1 && !skip_russian && rnd.Get01() > russian) break;
            skip_russian = false;
        }else{
            // Fixed depth path termination
            if(n > depth) break;
        }

        IFDEBUG std::cout << "Generating path, n = " << n << std::endl;

        raycount++;
        Intersection i;
        if(thinglass.size() == 0){
            // This variant is a bit faster.
            i = scene.FindIntersectKdOtherThan(current_ray, last_triangle);
        }else{
            i = scene.FindIntersectKdOtherThanWithThinglass(current_ray, last_triangle, thinglass);
        }
        PathPoint p;
        p.i = i;
        p.thinglass_isect = i.thinglass; // This is only used in 2nd phase.
        if(!i.triangle){
            p.infinity = true;
            p.Vr = -current_ray.direction;
            path.push_back(p);
            break;
        }else{
            if(i.triangle == last_triangle){
                // std::cerr << "Ray collided with source triangle. This should never happen." << std::endl;
            }
            // Prepare normal
            assert(NEAR(glm::length(current_ray.direction),1.0f));
            p.pos = current_ray[i.t];
            p.faceN = i.Interpolate(i.triangle->GetNormalA(),
                                    i.triangle->GetNormalB(),
                                    i.triangle->GetNormalC());
            p.faceN = glm::normalize(p.faceN);

            // Prepare incoming direction
            p.Vr = -current_ray.direction;

            const Material& mat = i.triangle->GetMaterial();

            bool fromInside = false;
            if(glm::dot(p.faceN, p.Vr) < 0){
                fromInside = true;
                // Pretend correction.
                p.faceN = -p.faceN;
            }

            // Interpolate textures
            if(mat.ambient_texture || mat.diffuse_texture || mat.specular_texture || mat.bump_texture){
                IFDEBUG
                    std::cout << "Calculating texUV" << std::endl;
                glm::vec2 a = i.triangle->GetTexCoordsA();
                glm::vec2 b = i.triangle->GetTexCoordsB();
                glm::vec2 c = i.triangle->GetTexCoordsC();
                IFDEBUG std::cout << "a = " << a << std::endl;
                IFDEBUG std::cout << "a = " << i.triangle->va << std::endl;
                IFDEBUG std::cout << "a = " << scene.texcoords[i.triangle->va] << std::endl;
                p.texUV = i.Interpolate(a,b,c);
            }
            // Tilt normal using bump texture
            if(mat.bump_texture){
                float right = mat.bump_texture->GetSlopeRight(p.texUV);
                float bottom = mat.bump_texture->GetSlopeBottom(p.texUV);
                glm::vec3 tangent = i.Interpolate(i.triangle->GetTangentA(),
                                                  i.triangle->GetTangentB(),
                                                  i.triangle->GetTangentC());
                if(tangent.x*tangent.x + tangent.y*tangent.y + tangent.z*tangent.z < 0.001f){
                    // Well, so, apparently, sometimes assimp generates invalid tangents. They seem okay
                    // on their own, but they interpolate weird, because tangents at two coincident vertices
                    // are opposite. Thus if it happens that interpolated tangent is zero, and therefore can't be
                    // normalized, we just silently ignore the bump map in this point. I'll have little effect on the
                    // entire pixel anyway.
                    p.lightN = p.faceN;
                }else{
                    tangent = glm::normalize(tangent);
                    glm::vec3 bitangent = glm::normalize(glm::cross(p.faceN,tangent));
                    glm::vec3 tangent2 = glm::cross(bitangent,p.faceN);
                    p.lightN = glm::normalize(p.faceN + (tangent2*right + bitangent*bottom) * bumpmap_scale);
                    IFDEBUG std::cout << "faceN " << p.faceN << std::endl;
                    IFDEBUG std::cout << "lightN " << p.lightN << std::endl;
                    // This still happend.
                    if(glm::isnan(p.lightN.x)){
                        p.lightN = p.faceN;
                    }
                    assert(glm::length(p.lightN) > 0);
                    float dot2 = glm::dot(p.faceN,tangent2);
                    assert(dot2 >= -0.001f);
                    float dot3 = glm::dot(p.faceN,bitangent);
                    assert(dot3 >= -0.001f);
                    float dot = glm::dot(p.faceN,p.lightN);
                    assert(dot > 0);
                }
            }else{
                p.lightN = p.faceN;
            }


            // Determine point type
            if(mat.translucency > 0.001f){
                // This is a translucent material.
                if(fromInside){
                    // Ray leaves the object
                    p.type = PathPoint::LEFT;
                }else{
                    if(rnd.Get01() < mat.translucency){
                        // Fresnell refraction/reflection
                        // TODO: The fresnell function assumes eta1 = 1.0. For eg. underwater reflections this is
                        // not correct, really.
                        float q = Fresnel(p.Vr, p.lightN, 1.0/mat.refraction_index);
                        IFDEBUG std::cout << "Angle = " << glm::angle(p.Vr, p.lightN)*180.0f/glm::pi<float>() << std::endl;
                        IFDEBUG std::cout << "Fresnel = " << q << std::endl;
                        if(rnd.Get01() < q){
                            p.type = PathPoint::REFLECTED;
                        }else{
                            p.type = PathPoint::ENTERED;
                        }
                    }else{
                        p.type = PathPoint::SCATTERED;
                    }
                }
            }else{
                // Not a translucent material.
                if(mat.reflective){
                    if(rnd.Get01() < mat.reflection_strength){
                        p.type = PathPoint::REFLECTED;
                    }else{
                        p.type = PathPoint::SCATTERED;
                    }
                // TODO: Fresnel reflecion on opaque surfaces
                }else{
                    float q = Fresnel(p.Vr, p.lightN, 1.0/mat.refraction_index);
                    IFDEBUG std::cout << "Vr " << p.Vr << std::endl;
                    IFDEBUG std::cout << "lightN " << p.lightN << std::endl;
                    IFDEBUG std::cout << "IOR " << mat.refraction_index << std::endl;
                    IFDEBUG std::cout << "Fresnel " << q << std::endl;
                    if(rnd.Get01() < q){
                        p.type = PathPoint::REFLECTED;
                    }else{
                        p.type = PathPoint::SCATTERED;
                    }
                }
            }

            if(p.type == PathPoint::REFLECTED ||
               p.type == PathPoint::ENTERED ||
               p.type == PathPoint::LEFT){
                // Do not count this point into depth. Never russian-terminate path at this point.
                n--; skip_russian = true;
            }

            IFDEBUG std::cout << "Ray hit material " << mat.name << " at " << p.pos << " and ";
            // Generate next ray direction
            glm::vec3 dir;
            switch(p.type){
            case PathPoint::REFLECTED:
                IFDEBUG std::cout << "REFLECTED." << std::endl;
                dir = 2.0f * glm::dot(p.Vr, p.lightN) * p.lightN - p.Vr;
                if(glm::dot(dir, p.faceN) > 0.0f)
                    break;
                // Otherwise, this reflected ray would enter inside the face.
                // Therefore, pretend it's a scatter ray. Thus:
                /* FALLTHROUGH */
            case PathPoint::SCATTERED:
                IFDEBUG std::cout << "SCATTERED." << std::endl;
                do{
                    //dir = rnd.GetHSCosDir(p.lightN);
                    std::tie(dir, p.sampleP, p.sampling_type) = mat.brdf->GetRay(p.lightN, rnd);
                    // TODO: Use glm::dot instaed of angle...
                }while(glm::angle(dir, p.faceN) > glm::pi<float>()/2.0f);
                break;
            case PathPoint::ENTERED:
                // TODO: Refraction
                dir = glm::refract(p.Vr, p.lightN, 1.0f/mat.refraction_index);
                IFDEBUG std::cout << "ENTERED medium." << std::endl;
                if(glm::length(dir) < 0.001f || glm::isnan(dir.x)){
                    // Internal reflection
                    IFDEBUG std::cout << "internally reflected." << std::endl;
                    p.type = PathPoint::REFLECTED;
                    dir = 2.0f * glm::dot(p.Vr, p.lightN) * p.lightN - p.Vr;
                }
                // current_ior *= mat.refraction_index
                break;
            case PathPoint::LEFT:
                // TODO: Refraction
                IFDEBUG std::cout << "LEFT medium." << std::endl;
                dir = glm::refract(p.Vr, p.lightN, mat.refraction_index);
                if(glm::length(dir) < 0.001f){
                    // Internal reflection
                    IFDEBUG std::cout << "internally reflected." << std::endl;
                    p.type = PathPoint::REFLECTED;
                    dir = 2.0f * glm::dot(p.Vr, p.lightN) * p.lightN - p.Vr;
                }
                dir = -p.Vr;
                // current_ior /= mat.refraction_index;
                break;
            }
            p.Vi = dir;

            path.push_back(p);

            current_ray = Ray(p.pos +
                              p.faceN * scene.epsilon * 10.0f *
                              ((p.type == PathPoint::ENTERED)?-1.0f:1.0f)
                              , glm::normalize(dir));

            IFDEBUG std::cout << "Next ray will be from " << p. pos << " dir " << dir << std::endl;

            last_triangle = i.triangle;
            // Continue for next ray
        }
    }

    // ============== 2nd phase ==============
    // Calculate light transmitted over path.

    for(int n = path.size()-1; n >= 0; n--){
        IFDEBUG std::cout << "--- Processing PP " << n << std::endl;

        bool last = ((unsigned int)n == path.size()-1);
        PathPoint& p = path[n];
        if(p.infinity){
            IFDEBUG std::cout << "This a sky ray, total: " << sky_radiance << std::endl;

            // TODO: Apply filtering

            p.to_prev = sky_radiance;
        }else{
            const Material& mat = p.i.triangle->GetMaterial();

            IFDEBUG std::cout << "Hit material: " << mat.name << std::endl;

            IFDEBUG std::cout << "texUV " << p.texUV << std::endl;

            Color diffuse  =  mat.diffuse_texture?mat.diffuse_texture->GetPixelInterpolated(p.texUV,debug) : mat.diffuse ;
            Color specular = mat.specular_texture?mat.specular_texture->GetPixelInterpolated(p.texUV,debug): mat.specular;

            IFDEBUG std::cout << "Diffuse: " << diffuse << std::endl;
            IFDEBUG std::cout << "Diffuse: " << mat.diffuse_texture << std::endl;
            IFDEBUG std::cout << "Diffuse: " << p.texUV << std::endl;

            Radiance total(0.0,0.0,0.0);

            if(p.type == PathPoint::SCATTERED){

                // ==================================
                // Direct lighting, for each point light and area light, choose a light point
                std::vector<Light> random_lights;
                for(const Light& p : scene.pointlights) random_lights.push_back(p);
                for(const auto& p : scene.areal_lights){
                    Light l = p.second.GetRandomLight(rnd, scene);
                    random_lights.push_back(l);
                }

                for(const Light& l : random_lights){
                    glm::vec3 lightpos = l.pos + rnd.GetSphere(l.size);

                    IFDEBUG std::cout << "Incorporating direct lighting component, lightpos: " << lightpos << std::endl;

                    std::vector<std::pair<const Triangle*, float>> thinglass_isect;
                    // Visibility factor
                    if((thinglass.size() == 0 && scene.Visibility(lightpos, p.pos)) ||
                       (thinglass.size() != 0 && scene.VisibilityWithThinglass(lightpos, p.pos, thinglass, thinglass_isect))){

                        IFDEBUG std::cout << "====> Light is visible" << std::endl;

                        // Incoming direction
                        glm::vec3 Vi = glm::normalize(lightpos - p.pos);

                        //Radiance f = mat.brdf(p.lightN, diffuse, specular, Vi, p.Vr, mat.exponent, 1.0, mat.refraction_index);
                        Radiance f = mat.brdf->Apply(diffuse, specular, p.lightN, Vi, p.Vr, debug);

                        IFDEBUG std::cout << "f = " << f << std::endl;

                        float G = glm::max(0.0f, glm::cos( glm::angle(p.lightN, Vi) )) / glm::distance2(lightpos, p.pos);
                        IFDEBUG std::cout << "G = " << G << ", angle " << glm::angle(p.lightN, Vi) << std::endl;
                        IFDEBUG std::cout << "lightN = " << p.lightN << ", Vi " << Vi << std::endl;

                        Radiance inc_l = Radiance(l.color) * l.intensity;

                        IFDEBUG std::cout << "incoming light: " << inc_l << std::endl;
                        IFDEBUG std::cout << "filters: " << thinglass_isect.size() << std::endl;

                        float ct = -1.0f;
                        for(int n = thinglass_isect.size()-1; n >= 0; n--){
                            const Triangle* trig = thinglass_isect[n].first;
                            IFDEBUG std::cout << trig << std::endl;
                            IFDEBUG std::cout << "ct " <<  ct << std::endl;
                            // Ignore repeated triangles within epsillon radius from previous
                            // thinglass - they are probably clones of the same triangle in kd-tree.
                            float newt = thinglass_isect[n].second;
                            IFDEBUG std::cout << "newt " << newt << std::endl;
                            if(newt <= ct + scene.epsilon) continue;
                            IFDEBUG std::cout << "can apply." << std::endl;
                            ct = newt;
                            // This is just to check triangle orientation,
                            // so that we only apply color filter when the
                            // ray is entering glass.
                            glm::vec3 N = trig->generic_normal();
                            if(glm::dot(N,Vi) > 0){
                                IFDEBUG std::cout << "APPLYING" << std::endl;
                                // TODO: Use translucency filter instead of diffuse!
                                inc_l = inc_l * trig->GetMaterial().diffuse;
                            }
                        }

                        IFDEBUG std::cout << "incoming light with filters: " << inc_l << std::endl;

                        Radiance out = inc_l * f * G;
                        IFDEBUG std::cout << "total direct lighting: " << out << std::endl;
                        total += out;
                    }else{
                        IFDEBUG std::cout << "Light not visible" << std::endl;
                    }
                }

                // =================
                // Indirect lighting
                if(!last){
                    // look at next pp's to_prev and incorporate it here
                    Radiance incoming = path[n+1].to_prev;
                    IFDEBUG std::cout << "Incorporating indirect lighting - incoming radiance: " << incoming << std::endl;

                    if(russian > 0.0f) incoming = incoming / russian;

                    IFDEBUG std::cout << "With russian: " << incoming << std::endl;

                    // Incoming direction
                    glm::vec3 Vi = p.Vi;

                    IFDEBUG std::cout << "Indirect incoming from: " << Vi << std::endl;

                    Radiance inc = incoming;
                    Radiance f = mat.brdf->Apply(diffuse, specular, p.lightN, Vi, p.Vr,debug);
                    float cos = glm::dot(p.lightN, Vi);

                    // Branch prediction should optimize-out these conditional jump during runtime.
                    if(p.sampling_type != BRDF::SAMPLING_COSINE){
                        // All sampling types use cosine, but for cosine sampling probability
                        // density is equal to cosine, so they cancel out.
                        IFDEBUG std::cout << "Mult by cos" << std::endl;
                        inc *= cos;
                    }else{
                        // Cosine sampling p = cos/pi. Don't divide by cos, as it was
                        // skipped, instead just multiply by pi.
                        inc *= glm::pi<float>();
                    }
                    if(p.sampling_type != BRDF::SAMPLING_BRDF){
                        // All sampling types use brdf, but for brdf sampling probability
                        // density is equal to brdf, so they cancel out.
                        IFDEBUG std::cout << "Mult by f" << std::endl;
                        inc *= f;
                    }
                    if(p.sampling_type != BRDF::SAMPLING_UNIFORM){
                        // NOP
                    }else{
                        // Probability density for uniform sampling.
                        IFDEBUG std::cout << "Div by P" << std::endl;
                        inc /= (0.5f/glm::pi<float>());
                    }

                    IFDEBUG std::cout << "Incoming * brdf * cos(...) / sampleP = " << inc << std::endl;


                    total += inc;
                }
            }else if(p.type == PathPoint::REFLECTED){
                if(path.size() > (unsigned int)n+1){
                    Radiance incoming = path[n+1].to_prev;
                    total += incoming;
                }
            }else if(p.type == PathPoint::ENTERED){
                if(path.size() > (unsigned int)n+1){
                    Radiance incoming = path[n+1].to_prev;
                    total += incoming * diffuse;
                }
            }else if(p.type == PathPoint::LEFT){
                if(path.size() > (unsigned int)n+1){
                    Radiance incoming = path[n+1].to_prev;
                    total += incoming;
                }
            }

            if(mat.emissive){
                total += Radiance(mat.emission);
            }

            IFDEBUG std::cerr << "total: " << total << std::endl;

            if(total.r > clamp) total.r = clamp;
            if(total.g > clamp) total.g = clamp;
            if(total.b > clamp) total.b = clamp;

            // Whoops.
            if(glm::isnan(total.r) || total.r < 0.0f) total.r = 0.0f;
            if(glm::isnan(total.g) || total.g < 0.0f) total.g = 0.0f;
            if(glm::isnan(total.b) || total.b < 0.0f) total.b = 0.0f;

            // Finally, apply thinglass filters that were encountered
            // when we were looking for intersection and stumbled upon this particular PP.

            float ct = -1.0f;
            for(int n = p.thinglass_isect.size()-1; n >= 0; n--){
                const Triangle* trig = p.thinglass_isect[n].first;
                // Ignore repeated triangles within epsillon radius from previous
                // thinglass - they are probably clones of the same triangle in kd-tree.
                float newt = p.thinglass_isect[n].second;
                if(newt <= ct + scene.epsilon) continue;
                ct = newt;
                // This is just to check triangle orientation,
                // so that we only apply color filter when the
                // ray is entering glass.
                glm::vec3 N = trig->generic_normal();
                if(glm::dot(N,p.Vr) >= 0){
                    // TODO: Use translucency filter instead of diffuse!
                    total = total * trig->GetMaterial().diffuse;
                }
            }

            IFDEBUG std::cerr << "total with thinglass filters: " << total << std::endl;

            p.to_prev = total;
        }
    }
    IFDEBUG std::cerr << "PATH TOTAL" << path[0].to_prev << std::endl << std::endl;
    return path[0].to_prev;
}
