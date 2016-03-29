#include "primitives.hpp"
#include "scene.hpp"

const Material& Triangle::GetMaterial() const { return parent_scene->materials[mat]; }
const glm::vec3 Triangle::GetVertexA()  const { return parent_scene->vertices[va];  }
const glm::vec3 Triangle::GetVertexB()  const { return parent_scene->vertices[vb];  }
const glm::vec3 Triangle::GetVertexC()  const { return parent_scene->vertices[vc];  }
const glm::vec3 Triangle::GetNormalA()  const { return parent_scene->normals[va];  }
const glm::vec3 Triangle::GetNormalB()  const { return parent_scene->normals[vb];  }
const glm::vec3 Triangle::GetNormalC()  const { return parent_scene->normals[vc];  }

const glm::vec2 Triangle::GetTexCoordsA()  const { return (parent_scene->n_texcoords <= va) ? glm::vec2(0.0,0.0) : parent_scene->texcoords[va]; }
const glm::vec2 Triangle::GetTexCoordsB()  const { return (parent_scene->n_texcoords <= va) ? glm::vec2(0.0,0.0) : parent_scene->texcoords[vb]; }
const glm::vec2 Triangle::GetTexCoordsC()  const { return (parent_scene->n_texcoords <= va) ? glm::vec2(0.0,0.0) : parent_scene->texcoords[vc]; }

void Triangle::CalculatePlane(){
    glm::vec3 v0 = parent_scene->vertices[va];
    glm::vec3 v1 = parent_scene->vertices[vb];
    glm::vec3 v2 = parent_scene->vertices[vc];

    glm::vec3 d0 = v1 - v0;
    glm::vec3 d1 = v2 - v0;

    glm::vec3 n = glm::normalize(glm::cross(d1,d0));
    float d = - glm::dot(n,v0);

    p = glm::vec4(n.x, n.y, n.z, d);
}

bool Triangle::TestIntersection(const Ray& __restrict__ r, /*out*/ float& t, float& a, float& b, bool debug) const{
    // Currently using Badoulel's algorithm

    // This implementation is heavily inspired by the example provided by ANL

    const float EPSILON = 0.00001;

    glm::vec3 planeN = p.xyz();

    double dot = glm::dot(r.direction, planeN);

    if(debug) std::cout << "triangle " << GetVertexA() << " " << GetVertexB() << " " << GetVertexC() << " " << std::endl;
    if(debug) std::cout << "dot : " << dot << std::endl;

    /* is ray parallel to plane? */
    if (dot < EPSILON && dot > -EPSILON)
        return false;

    /* find distance to plane and intersection point */
    double dot2 = glm::dot(r.origin,planeN);
    t = -(p.w + dot2) / dot;

    // TODO: Accelerate this
    /* find largest dim*/
    int i1 = 0, i2 = 1;
    glm::vec3 pq = glm::abs(planeN);
    if(pq.x > pq.y && pq.x > pq.z){
        i1 = 1; i2 = 2;
    }else if(pq.y > pq.z){
        i1 = 0; i2 = 2;
    }else{
        i1 = 0; i2 = 1;
    }

    if(debug) std::cout << i1 << "/" << i2 << " ";


    glm::vec3 vert0 = GetVertexA();
    glm::vec3 vert1 = GetVertexB();
    glm::vec3 vert2 = GetVertexC();

    if(debug) std::cerr << vert0 << " " << vert1 << " " << vert2 << std::endl;

    if(debug) std::cerr << "intersection is at: " << r.origin + r.direction*t << std::endl;

    glm::vec2 point(r.origin[i1] + r.direction[i1] * t,
                    r.origin[i2] + r.direction[i2] * t);

    /* begin barycentric intersection algorithm */
    glm::vec2 q0( point[0] - vert0[i1],
                  point[1] - vert0[i2]);
    glm::vec2 q1( vert1[i1] - vert0[i1],
                  vert1[i2] - vert0[i2]);
    glm::vec2 q2( vert2[i1] - vert0[i1],
                  vert2[i2] - vert0[i2]);

    if(debug) std::cerr << q0 << " " << q1 << " " << q2 << std::endl;

    // TODO Return these
    float alpha, beta;

    /* calculate and compare barycentric coordinates */
    if (q1.x > -EPSILON && q1.x < EPSILON ) {		/* uncommon case */
        if(debug) std::cout << "UNCOMMON" << std::endl;
        beta = q0.x / q2.x;
        if (beta < 0 || beta > 1)
            return false;
        alpha = (q0.y - beta * q2.y) / q1.y;
    }
    else {			/* common case, used for this analysis */
        beta = (q0.y * q1.x - q0.x * q1.y) / (q2.y * q1.x - q2.x * q1.y);
        if (beta < 0 || beta > 1)
            return false;

        alpha = (q0.x - beta * q2.x) / q1.x;
    }

    if(debug) std::cout << "A: " << alpha << ", B: " << beta << " A+B: " << alpha+beta << std::endl;

    if (alpha < 0 || (alpha + beta) > 1.0)
        return false;

    a = alpha;
    b = beta;

    return true;

}
