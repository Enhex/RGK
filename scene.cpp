#include "scene.hpp"

#include <iostream>
#include <limits>
#include <cmath>

Scene::~Scene(){
    FreeBuffers();
}

void Scene::FreeBuffers(){
    if(vertices) delete[] vertices;
    n_vertices = 0;
    if(triangles) delete[] triangles;
    n_triangles = 0;
    if(materials) delete[] materials;
    n_materials = 0;
    if(normals) delete[] normals;
    n_normals = 0;
}

void Scene::LoadScene(const aiScene* scene) const{
    // Load materials
    for(unsigned int i = 0; i < scene->mNumMaterials; i++){
        const aiMaterial* mat = scene->mMaterials[i];

        Material m;
        m.parent_scene = this;
        aiColor3D c;
        mat->Get(AI_MATKEY_COLOR_DIFFUSE,c);
        m.diffuse = c;
        mat->Get(AI_MATKEY_COLOR_SPECULAR,c);
        m.specular = c;
        float f;
        mat->Get(AI_MATKEY_SHININESS, f);
        m.exponent = f/4; // This is weird. Why does assimp multiply by 4 in the first place?
        materials_buffer.push_back(m);
    }

    // Load root node
    const aiNode* root = scene->mRootNode;
    LoadNode(scene,root);
}

void Scene::LoadNode(const aiScene* scene, const aiNode* ainode, aiMatrix4x4 current_transform) const{
    std::cout << "Loading node \"" << ainode->mName.C_Str() << "\", it has " << ainode->mNumMeshes << " meshes and " <<
        ainode->mNumChildren << " children" << std::endl;

    aiMatrix4x4 transform = current_transform * ainode->mTransformation;

    // Load meshes
    for(unsigned int i = 0; i < ainode->mNumMeshes; i++)
        LoadMesh(scene->mMeshes[ainode->mMeshes[i]], transform);

    // Load children
    for(unsigned int i = 0; i < ainode->mNumChildren; i++)
        LoadNode(scene, ainode->mChildren[i], transform);

}

void Scene::LoadMesh(const aiMesh* mesh, aiMatrix4x4 current_transform) const{
    std::cout << "-- Loading mesh \"" << mesh->mName.C_Str() << "\" with " << mesh->mNumFaces <<
        " faces and " << mesh->mNumVertices <<  " vertices." << std::endl;

    // Keep the current vertex buffer size.
    unsigned int vertex_index_offset = vertices_buffer.size();

    // Get the material index.
    unsigned int mat = mesh->mMaterialIndex;

    for(unsigned int v = 0; v < mesh->mNumVertices; v++){
        aiVector3D vertex = mesh->mVertices[v];
        vertex *= current_transform;
        vertices_buffer.push_back(vertex);
    }
    for(unsigned int v = 0; v < mesh->mNumVertices; v++){
        aiVector3D normal = mesh->mNormals[v];
        // TODO: current transform rotation?
        normals_buffer.push_back(normal);
    }
    for(unsigned int f = 0; f < mesh->mNumFaces; f++){
        const aiFace& face = mesh->mFaces[f];
        if(face.mNumIndices == 3){
            Triangle t;
            t.parent_scene = this;
            t.va = face.mIndices[0] + vertex_index_offset;
            t.vb = face.mIndices[1] + vertex_index_offset;
            t.vc = face.mIndices[2] + vertex_index_offset;
            t.mat = mat;
            //std::cout << t.va << " " << t.vb << " " << t.vc << " " << std::endl;
            triangles_buffer.push_back(t);
        }else{
            std::cerr << "WARNING: Skipping a face that apparently was not tringulated." << std::endl;
        }
    }

}


void Scene::Commit(){
    FreeBuffers();

    vertices = new glm::vec3[vertices_buffer.size()];
    triangles = new Triangle[triangles_buffer.size()];
    materials = new Material[materials_buffer.size()];
    normals = new glm::vec3[normals_buffer.size()];

    n_vertices = vertices_buffer.size();
    n_triangles = triangles_buffer.size();
    n_materials = materials_buffer.size();
    n_normals = normals_buffer.size();

    // TODO: memcpy
    for(unsigned int i = 0; i < n_vertices; i++)
        vertices[i] = glm::vec3(vertices_buffer[i].x, vertices_buffer[i].y, vertices_buffer[i].z);
    for(unsigned int i = 0; i < n_triangles; i++){
        triangles[i] = triangles_buffer[i];
        CalculateTrianglePlane(triangles[i]);
    }
    for(unsigned int i = 0; i < n_materials; i++){
        materials[i] = materials_buffer[i];
    }
    for(unsigned int i = 0; i < n_normals; i++)
        normals[i] = glm::vec3(normals_buffer[i].x, normals_buffer[i].y, normals_buffer[i].z);

    std::cout << "Commited " << n_vertices << " vertices, " << n_normals << " normals and " << n_triangles <<
        " triangles with " << n_materials << " materials to the scene." << std::endl;

    vertices_buffer.clear();
    triangles_buffer.clear();
    materials_buffer.clear();
    normals_buffer.clear();
}

void Scene::Dump() const{
    for(unsigned int t = 0; t < n_triangles; t++){
        const Triangle& tr = triangles[t];
        const glm::vec3 va = vertices[tr.va];
        const glm::vec3 vb = vertices[tr.vb];
        const glm::vec3 vc = vertices[tr.vc];
        const Color& color = materials[tr.mat].diffuse;
        std::cout << va.x << " " << va.y << " " << va.z << " | ";
        std::cout << vb.x << " " << vb.y << " " << vb.z << " | ";
        std::cout << vc.x << " " << vc.y << " " << vc.z << " [" ;
        std::cout << color.r << " " << color.g << " " << color.b << "]\n" ;
    }
}

Intersection Scene::FindIntersect(const Ray& __restrict__ r) const{
    // TODO: Kd-tree?

    // temporarily: iterate over ALL triangles, find ALL
    // intersections, pick nearest. Super inefficient.

    // create inf result
    Intersection res;
    res.triangle = nullptr;
    res.t = std::numeric_limits<float>::infinity();

    //std::cerr << "Searching " << std::endl;

    for(unsigned int f = 0; f < n_triangles; f++){
        const Triangle& tri = triangles[f];
        float t;
        if(TestTriangleIntersection(tri, r, t)){
            if(t <= r.near || t >= r.far) continue;

            // New intersect
            //std::cerr << t << ", at triangle no " << f << std::endl;
            //std::cerr << "Triangle normal " << tri.generic_normal() << std::endl;
            // Re-test for debug
            //TestTriangleIntersection(tri, r, t, true);
            if(t < res.t){
                // New closest intersect
                res.triangle = &tri;
                res.t = t;
            }
        }
    }
    //std::cerr << " done: " << res.t << std::endl;

    return res;
}

bool Scene::TestTriangleIntersection(const Triangle& __restrict__ tri, const Ray& __restrict__ r, /*out*/ float& t, bool debug) const{
    // Currently using Badoulel's algorithm

    // This implementation is heavily inspired by the example provided by ANL

    const float EPSILON = 0.000001;

    glm::vec3 planeN = tri.p.xyz();

    double dot = glm::dot(r.direction, planeN);

    if(debug) std::cout << "dot : " << dot << std::endl;

    /* is ray parallel to plane? */
    if (dot < EPSILON && dot > -EPSILON)		/* or use culling check */
        return false;

    /* find distance to plane and intersection point */
    double dot2 = glm::dot(r.origin,planeN);
    t = -(tri.p.w + dot2) / dot;

    // TODO: Accelerate this
    /* find largest dim*/
    int i1 = 0, i2 = 1;
    glm::vec3 pq = glm::abs(planeN);
    if(pq.x >= pq.y && pq.x >= pq.z){
        i1 = 1; i2 = 2;
    }else if(pq.y >= pq.x && pq.y >= pq.z){
        i1 = 0; i2 = 2;
    }else if(pq.z >= pq.x && pq.z >= pq.y){
        i1 = 0; i2 = 1;
    }else{
        std::cerr << "NO LARGEST DIM this should not happen" << std::endl;
    }

    if(debug) std::cout << i1 << "/" << i2 << " ";


    glm::vec3 vert0 = tri.parent_scene->vertices[tri.va];
    glm::vec3 vert1 = tri.parent_scene->vertices[tri.vb];
    glm::vec3 vert2 = tri.parent_scene->vertices[tri.vc];

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

    // TODO Return these
    float alpha, beta;

    /* calculate and compare barycentric coordinates */
    if (q1.x == 0) {		/* uncommon case */
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

    if(debug) std::cout << "A: " << alpha << ", B: " << beta << std::endl;

    if (alpha < 0 || (alpha + beta) > 1.0)
        return false;

    return true;

}

void Scene::CalculateTrianglePlane(Triangle& t){

    glm::vec3 v0 = t.parent_scene->vertices[t.va];
    glm::vec3 v1 = t.parent_scene->vertices[t.vb];
    glm::vec3 v2 = t.parent_scene->vertices[t.vc];

    glm::vec3 d0 = v1 - v0;
    glm::vec3 d1 = v2 - v0;

    glm::vec3 n = glm::normalize(glm::cross(d1,d0));
    float d = - glm::dot(n,v0);

    t.p = glm::vec4(n.x, n.y, n.z, d);
}
