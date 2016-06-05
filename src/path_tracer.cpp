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
                       bool opaque_fresnell,
                       unsigned int reverse,
                       std::set<const Material*> thinglass,
                       Random rnd)
: Tracer(scene, camera, xres, yres, multisample, bumpmap_scale),
  clamp(clamp),
  russian(russian),
  depth(depth),
  thinglass(thinglass),
  opaque_fresnell(opaque_fresnell),
  reverse(reverse),
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


Radiance PathTracer::ApplyThinglass(Radiance input, const ThinglassIsections& isections, glm::vec3 ray_direction) const {
    Radiance result = input;
    float ct = -1.0f;
    for(int n = isections.size()-1; n >= 0; n--){
        const Triangle* trig = isections[n].first;
        // Ignore repeated triangles within epsillon radius from
        // previous thinglass - they are probably clones of the same
        // triangle in kd-tree.
        float newt = isections[n].second;
        if(newt <= ct + scene.epsilon) continue;
        ct = newt;
        // This is just to check triangle orientation, so that we only
        // apply color filter when the ray is entering glass.
        glm::vec3 N = trig->generic_normal();
        if(glm::dot(N,ray_direction) >= 0){
            // TODO: Use translucency filter instead of diffuse!
            result = result * trig->GetMaterial().diffuse;
        }
    }
    return result;
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


std::vector<PathTracer::PathPoint> PathTracer::GeneratePath(Ray r, unsigned int& raycount, unsigned int depth__, float russian__, bool debug) const {

    std::vector<PathPoint> path;

    IFDEBUG std::cout << "Ray direction: " << r.direction << std::endl;

    Radiance cumulative_transfer_coeff = Radiance(1.0f, 1.0f, 1.0f);

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
        if(russian__ >= 0.0f){
            // Russian roulette path termination
            if(n > 1 && !skip_russian && rnd.Get01() > russian__) break;
            skip_russian = false;
        }else{
            // Fixed depth path termination
            if(n > depth__) break;
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
        p.thinglass_isect = i.thinglass;
        if(!i.triangle){
            // A sky ray!
            p.infinity = true;
            p.Vr = -current_ray.direction;
            path.push_back(p);
            // End path.
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
            p.mat = &mat;

            bool fromInside = false;
            if(glm::dot(p.faceN, p.Vr) < 0){
                fromInside = true;
                // Pretend correction.
                p.faceN = -p.faceN;
            }

            glm::vec2 texUV;
            // Interpolate textures
            if(mat.ambient_texture || mat.diffuse_texture || mat.specular_texture || mat.bump_texture){
                glm::vec2 a = i.triangle->GetTexCoordsA();
                glm::vec2 b = i.triangle->GetTexCoordsB();
                glm::vec2 c = i.triangle->GetTexCoordsC();
                texUV = i.Interpolate(a,b,c);
            }
            // Get colors from texture
            p.diffuse  =  mat.diffuse_texture?mat.diffuse_texture->GetPixelInterpolated(texUV,debug) : mat.diffuse ;
            p.specular = mat.specular_texture?mat.specular_texture->GetPixelInterpolated(texUV,debug): mat.specular;
            // Tilt normal using bump texture
            if(mat.bump_texture){
                float right = mat.bump_texture->GetSlopeRight(texUV);
                float bottom = mat.bump_texture->GetSlopeBottom(texUV);
                glm::vec3 tangent = i.Interpolate(i.triangle->GetTangentA(),
                                                  i.triangle->GetTangentB(),
                                                  i.triangle->GetTangentC());
                if(tangent.x*tangent.x + tangent.y*tangent.y + tangent.z*tangent.z < 0.001f){
                    // Well, so apparently, sometimes assimp generates invalid tangents. They seem okay
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


            // Randomly determine point type
            if(mat.translucency > 0.001f){
                // This is a translucent material.
                if(fromInside){
                    // Ray leaves the object
                    p.type = PathPoint::LEFT;
                }else{
                    float q = Fresnel(p.Vr, p.lightN, 1.0/mat.refraction_index);
                    if(rnd.Get01() < q) p.type = PathPoint::REFLECTED;
                    else{
                        if(rnd.Get01() < mat.translucency) p.type = PathPoint::ENTERED;
                        else p.type = PathPoint::SCATTERED;
                    }
                }
            }else{
                // Not a translucent material.
                float t = (opaque_fresnell)? Fresnel(p.Vr, p.lightN, 1.0/mat.refraction_index) : 0.0f;
                float q = (mat.reflective)? mat.reflection_strength : t;
                if(rnd.Get01() < q) p.type = PathPoint::REFLECTED;
                else                p.type = PathPoint::SCATTERED;
            }

            // Skip roulette if the ray has priviledge
            if(p.type == PathPoint::REFLECTED ||
               p.type == PathPoint::ENTERED ||
               p.type == PathPoint::LEFT){
                // Do not count this point into depth. Never russian-terminate path at this point.
                n--; skip_russian = true;
            }

            BRDF::BRDFSamplingType sampling_type = BRDF::SAMPLING_COSINE;
            // Compute next ray direction
            IFDEBUG std::cout << "Ray hit material " << mat.name << " at " << p.pos << " and ";
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
                // Revert to face normal in case this ray would enter from inside
                if(glm::dot(p.lightN, p.Vr) <= 0.0f) p.lightN = p.faceN;
                do{
                    std::tie(dir, std::ignore, sampling_type) = mat.brdf->GetRay(p.lightN, p.Vr, rnd);
                }while(glm::dot(dir, p.faceN) <= 0.0f);
                break;
            case PathPoint::ENTERED:
                dir = glm::refract(p.Vr, p.lightN, 1.0f/mat.refraction_index);
                IFDEBUG std::cout << "ENTERED medium." << std::endl;
                if(glm::length(dir) < 0.001f || glm::isnan(dir.x)){
                    // Internal reflection
                    IFDEBUG std::cout << "internally reflected." << std::endl;
                    p.type = PathPoint::REFLECTED;
                    dir = 2.0f * glm::dot(p.Vr, p.lightN) * p.lightN - p.Vr;
                }
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
                break;
            }
            p.Vi = dir;

            // Store russian coefficient
            if(russian__ > 0.0f) p.russian_coefficient = 1.0f/russian__;
            else p.russian_coefficient = 1.0f;

            // Calculate transfer coefficients (BRFD, cosine, etc.)
            p.transfer_coefficients = Radiance(1.0f, 1.0f, 1.0f);

            if(p.type == PathPoint::SCATTERED){
                // Branch prediction should optimize-out these conditional jump during runtime.
                if(sampling_type != BRDF::SAMPLING_COSINE){
                    // All sampling types use cosine, but for cosine sampling probability
                    // density is equal to cosine, so they cancel out.
                    IFDEBUG std::cout << "Mult by cos" << std::endl;
                    float cos = glm::dot(p.lightN, p.Vi);
                    p.transfer_coefficients *= cos;
                }else{
                    // Cosine sampling p = cos/pi. Don't divide by cos, as it was
                    // skipped, instead just multiply by pi.
                    p.transfer_coefficients *= glm::pi<float>();
                }
                if(sampling_type != BRDF::SAMPLING_BRDF){
                    // All sampling types use brdf, but for brdf sampling probability
                    // density is equal to brdf, so they cancel out.
                    IFDEBUG std::cout << "Mult by f" << std::endl;
                    Radiance f = mat.brdf->Apply(p.diffuse, p.specular, p.lightN, p.Vi, p.Vr, debug);
                    p.transfer_coefficients *= f;
                }
                if(sampling_type != BRDF::SAMPLING_UNIFORM){
                    // NOP
                }else{
                    // Probability density for uniform sampling.
                    IFDEBUG std::cout << "Div by P" << std::endl;
                    p.transfer_coefficients *= glm::pi<float>()/0.5;
                }
            }

            // TODO: Terminate when cumulative_coeff gets too low
            cumulative_transfer_coeff *= p.russian_coefficient;
            cumulative_transfer_coeff *= p.transfer_coefficients;
            IFDEBUG std::cout << "Path cumulative transfer coeff: " << cumulative_transfer_coeff << std::endl;

            // Commit the path point to the path
            path.push_back(p);

            // Prepate next ray
            current_ray = Ray(p.pos +
                              p.faceN * scene.epsilon * 10.0f *
                              ((p.type == PathPoint::ENTERED)?-1.0f:1.0f)
                              , glm::normalize(dir));

            IFDEBUG std::cout << "Next ray will be from " << p. pos << " dir " << dir << std::endl;

            last_triangle = i.triangle;
            // Continue for next ray
        }
    }

    return path;
}

Radiance PathTracer::TracePath(const Ray& r, unsigned int& raycount, bool debug){

    // ===== 1st Phase =======
    // Generate a forward path.
    IFDEBUG std::cout << "== FORWARD PATH" << std::endl;
    std::vector<PathPoint> path = GeneratePath(r, raycount, depth, russian, debug);

    // Choose a light source.
    const Light& l = scene.GetRandomLight(rnd);
    glm::vec3 lightpos = l.pos;
    glm::vec3 lightdir;
    if(l.type == Light::FULL_SPHERE){
        glm::vec3 q = rnd.GetSphere(l.size);
        lightpos += q;
        if(glm::length(q) < 0.01f) q = rnd.GetSphere(1.0f);
        lightdir = rnd.GetHSCosDir(glm::normalize(q));
    }else{
        lightdir = rnd.GetHSCosDir(l.normal);
    }

    // Generate backward path (from light)
    IFDEBUG std::cout << "== LIGHT PATH" << std::endl;
    Ray light_ray(lightpos + scene.epsilon * l.normal * 100.0f, lightdir);
    std::vector<PathPoint> light_path = GeneratePath(light_ray, raycount, reverse, -1.0f, debug);
    IFDEBUG std::cout << "Light path size " << light_path.size() << std::endl;

    // ============== 2nd phase ==============
    // Calculate light transmitted over light path.
    Radiance light_carried;

    IFDEBUG std::cout << " === Carrying light along light path" << std::endl;

    for(unsigned int n = 0; n < light_path.size(); n++){
        PathPoint& p = light_path[n];

        if(n == 0){
            //glm::vec3 Vi = glm::normalize(lightpos - p.pos);
            //float G = glm::max(0.0f, glm::dot(p.lightN, Vi)) / glm::distance2(lightpos, p.pos);
            //IFDEBUG std::cout << "G = " << G << std::endl;
            IFDEBUG std::cout << "lightpos = " << lightpos << std::endl;
            IFDEBUG std::cout << "p.pos = " << p.pos << std::endl;
            light_carried = Radiance(l.color) * l.intensity;// * G;
        }

        light_carried = ApplyThinglass(light_carried, p.thinglass_isect, p.Vr);

        p.light_from_source = light_carried;

        light_carried *= p.transfer_coefficients * p.russian_coefficient;

        IFDEBUG std::cout << "After light point " << n << ", carried light:" << light_carried << std::endl;
    }

    // ============== 3rd phase ==============
    // Calculate light transmitted over view path.

    Radiance from_next;

    for(int n = path.size()-1; n >= 0; n--){
        IFDEBUG std::cout << "--- Processing PP " << n << std::endl;

        bool last = ((unsigned int)n == path.size()-1);
        const PathPoint& p = path[n];
        if(p.infinity){
            IFDEBUG std::cout << "This a sky ray, total: " << sky_radiance << std::endl;
            from_next = ApplyThinglass(sky_radiance, p.thinglass_isect, -p.Vr);
            continue;
        }

        const Material& mat = *p.mat;

        IFDEBUG std::cout << "Hit material: " << mat.name << std::endl;

        Radiance total(0.0,0.0,0.0);

        if(p.type == PathPoint::SCATTERED){
            // ==========
            // Direct lighting


            IFDEBUG std::cout << "Incorporating direct lighting component, lightpos: " << lightpos << std::endl;

            std::vector<std::pair<const Triangle*, float>> thinglass_isect;
            // Visibility factor
            if((thinglass.size() == 0 && scene.Visibility(lightpos, p.pos)) ||
               (thinglass.size() != 0 && scene.VisibilityWithThinglass(lightpos, p.pos, thinglass, thinglass_isect))){

                IFDEBUG std::cout << "====> Light is visible" << std::endl;

                // Incoming direction
                glm::vec3 Vi = glm::normalize(lightpos - p.pos);

                Radiance f = mat.brdf->Apply(p.diffuse, p.specular, p.lightN, Vi, p.Vr, debug);

                IFDEBUG std::cout << "f = " << f << std::endl;

                float G = glm::max(0.0f, glm::dot(p.lightN, Vi)) / glm::distance2(lightpos, p.pos);
                IFDEBUG std::cout << "G = " << G << ", angle " << glm::angle(p.lightN, Vi) << std::endl;
                Radiance inc_l = Radiance(l.color) * l.intensity;
                inc_l = ApplyThinglass(inc_l, thinglass_isect, Vi);

                IFDEBUG std::cout << "incoming light with filters: " << inc_l << std::endl;

                Radiance out = inc_l * f * G;
                IFDEBUG std::cout << "total direct lighting: " << out << std::endl;
                total += out;
            }else{
                IFDEBUG std::cout << "Light not visible" << std::endl;
            }

            // Reverse light
            for(unsigned int q = 0; q < light_path.size(); q++){
                const PathPoint& l = light_path[q];
                // TODO: Thinglass?
                if(!l.infinity && scene.Visibility(l.pos, p.pos)){
                    glm::vec3 light_to_p = glm::normalize(p.pos - l.pos);
                    glm::vec3 p_to_light = -light_to_p;
                    Radiance f_light = l.mat->brdf->Apply(l.diffuse, l.specular, l.lightN, light_to_p, l.Vr, debug);
                    Radiance f_point = p.mat->brdf->Apply(p.diffuse, p.specular, p.lightN, p.Vr, p_to_light, debug);
                    float G = glm::max(0.0f, glm::dot(p.lightN, p_to_light)) / glm::distance2(l.pos, p.pos);
                    total += l.light_from_source * f_light * f_point * G;
                }// not visible from each other.
            }

            // =================
            // Indirect lighting
            if(!last){
                // look at next pp's to_prev and incorporate it here
                Radiance inc = from_next;
                IFDEBUG std::cout << "Incorporating indirect lighting - incoming radiance: " << inc << std::endl;
                inc = inc * p.russian_coefficient * p.transfer_coefficients;
                IFDEBUG std::cout << "Incoming * brdf * cos(...) / sampleP = " << inc << std::endl;
                total += inc;
            }
        }else if(p.type == PathPoint::REFLECTED || p.type == PathPoint::LEFT){
            assert(path.size() >= (unsigned int)n+1);
            total += from_next;
        }else if(p.type == PathPoint::ENTERED){
            assert(path.size() >= (unsigned int)n+1);
            // Note: Cannot use Kt factor from mtl file as assimp does not support it.
            total += from_next * p.diffuse;
        }

        if(mat.emissive){
            total += Radiance(mat.emission);
        }

        // Finally, apply thinglass filters that were encountered
        // when we were looking for intersection and stumbled upon this particular PP.

        total = ApplyThinglass(total, p.thinglass_isect, p.Vr);

        // Clamp.
        if(total.r > clamp) total.r = clamp;
        if(total.g > clamp) total.g = clamp;
        if(total.b > clamp) total.b = clamp;

        // Safeguard against any accidental nans or negative values.
        if(glm::isnan(total.r) || total.r < 0.0f) total.r = 0.0f;
        if(glm::isnan(total.g) || total.g < 0.0f) total.g = 0.0f;
        if(glm::isnan(total.b) || total.b < 0.0f) total.b = 0.0f;

        IFDEBUG std::cerr << "total with thinglass filters: " << total << std::endl;

        from_next = total;

    } // for each point on path
    IFDEBUG std::cerr << "PATH TOTAL" << from_next << std::endl << std::endl;
    return from_next;
}
