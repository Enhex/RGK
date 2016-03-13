#include "scene.hpp"
#include "utils.hpp"

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

    // Clearing vectors this way forces memory to be freed.
    vertices_buffer  = std::vector<aiVector3D>();
    triangles_buffer = std::vector<Triangle>();
    materials_buffer = std::vector<Material>();
    normals_buffer   = std::vector<aiVector3D>();

    // Computing x/y/z bounds for all triangles.
    xevents.resize(2 * n_triangles);
    yevents.resize(2 * n_triangles);
    zevents.resize(2 * n_triangles);
    auto bound_fill_func = [this](unsigned int axis, std::vector<float>& buf){
        for(unsigned int i = 0; i < n_triangles; i++){
            const Triangle& t = triangles[i];
            auto p = std::minmax({t.GetVertexA()[axis], t.GetVertexB()[axis], t.GetVertexC()[axis]});
            buf[2*i + 0] = p.first;
            buf[2*i + 1] = p.second;
        }
    };
    bound_fill_func(0,xevents);
    bound_fill_func(1,yevents);
    bound_fill_func(2,zevents);

    // Global bounding box
    auto p = std::minmax_element(xevents.begin(), xevents.end());
    auto q = std::minmax_element(yevents.begin(), yevents.end());
    auto r = std::minmax_element(zevents.begin(), zevents.end());
    xBB = std::make_pair(*p.first, *p.second);
    yBB = std::make_pair(*q.first, *q.second);
    zBB = std::make_pair(*r.first, *r.second);

    std::cout << "The scene is bounded by [" << xBB.first << ", " << xBB.second << "], " <<
                                         "[" << yBB.first << ", " << yBB.second << "], " <<
                                         "[" << zBB.first << ", " << zBB.second << "]."  << std::endl;

    std::cout << "Total avg cost with no kd-tree: " << ISECT_COST * n_triangles << std::endl;

    UncompressedKdNode root;
    root.parent_scene = this;
    for(unsigned int i = 0; i < n_triangles; i++) root.triangle_indices.push_back(i);
    root.xBB = xBB;
    root.yBB = yBB;
    root.zBB = zBB;

    // Prepare kd-tree
    int l = std::log2(n_triangles) + 8;
    //l = 4;
    std::cout << "Building kD-tree with max depth " << l << std::endl;
    root.Subdivide(l);

    auto totals = root.GetTotals();
    std::cout << "Total triangles in tree: " << std::get<0>(totals) << ", total leafs: " << std::get<1>(totals) << ", total nodes: " << std::get<2>(totals) << ", total dups: " << std::get<3>(totals) << std::endl;
    std::cout << "Average triangles per leaf: " << std::get<0>(totals)/(float)std::get<1>(totals) << std::endl;
    // TODO: Compress


    std::cout << "Total avg cost with kd-tree: " << root.GetCost() << std::endl;

    root.FreeRecursivelly();
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
        float t, a, b;
        if(TestTriangleIntersection(tri, r, t, a, b)){
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
                float c = 1.0f - a - b;

                res.a = c;
                res.b = a;
                res.c = b;
            }
        }
    }
    //std::cerr << " done: " << res.t << std::endl;

    return res;
}

bool Scene::TestTriangleIntersection(const Triangle& __restrict__ tri, const Ray& __restrict__ r, /*out*/ float& t, float& a, float& b, bool debug) const{
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

    a = alpha;
    b = beta;

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

void UncompressedKdNode::Subdivide(unsigned int max_depth){
    if(depth >= max_depth) return; // Do not subdivide further.

    unsigned int n = triangle_indices.size();
    if(n < 2) return; // Do not subdivide further.

    std::cerr << "--- Subdividing " << n << " faces" << std::endl;

    // Choose the axis for subdivision.
    // TODO: Do it faster
    float sizes[3] = {xBB.second - xBB.first, yBB.second - yBB.first, zBB.second - zBB.first};
    unsigned int axis = std::max_element(sizes, sizes+3) - sizes;

    std::cerr << "Using axis " << axis << std::endl;
    const std::vector<float>* evch[3] = {&parent_scene->xevents, &parent_scene->yevents, &parent_scene->zevents};
    const std::vector<float>& all_events = *evch[axis];

    // Prepare BB events.
    struct BBEvent{
        float pos;
        int triangleID;
        enum {BEGIN, END} type;
    };
    std::vector<BBEvent> events(2*n);
    for(unsigned int i = 0; i < n; i++){
        int t = triangle_indices[i];
        //std::cerr << "Adding events for triangle no " << t << std::endl;
        events[2*i + 0] = BBEvent{ all_events[2*t + 0], t, BBEvent::BEGIN };
        events[2*i + 1] = BBEvent{ all_events[2*t + 1], t, BBEvent::END   };
    }
    std::sort(events.begin(), events.end(), [](const BBEvent& a, const BBEvent& b){
            if(a.pos == b.pos) return a.type < b.type;
            return a.pos < b.pos;
        });
    //for(const auto& bbe : events){
    //    std::cerr << bbe.pos << " (" << bbe.triangleID << ") ";
    //}
    //std::cerr << std::endl;

    // Now, find the splitting plane
    /*.
    float split = 0.0f;
    unsigned int nth = n-1;
    std::nth_element(selected_events.begin(), selected_events.begin() + nth, selected_events.end());
    if(n % 2 == 1){
        split = selected_events[nth];
    }else{
        // Find the lowest element in the upper half.
        float f1 = selected_events[nth];
        float f2 = *std::min_element(selected_events.begin() + nth + 1, selected_events.end());
        split = (f1+f2)/2.0f;
    }
    */


    // SAH heuristics. Inspired by the pbrt book.
    const std::pair<float,float>* axbds[3] = {&xBB,&yBB,&zBB};
    const std::pair<float,float>& axis_bounds = *axbds[axis];
    const float d[3] = {xBB.second - xBB.first, yBB.second - yBB.first, zBB.second - zBB.first};

    int best_offset = -1;
    float best_cost = std::numeric_limits<float>::infinity();
    float best_pos = std::numeric_limits<float>::infinity();
    float nosplit_cost = ISECT_COST * n;
    unsigned int otherAxis0 = (axis + 1) % 3, otherAxis1 = (axis + 2) % 3;
    float invTotalSA = 1.f / (2.f * (d[0]*d[1] + d[0]*d[2] + d[1]*d[2]));
    int before = 0, after = n;
    for(unsigned int i = 0; i < 2*n; i++){
        if(events[i].type == BBEvent::END) after--;
        float pos = events[i].pos;
        // Ignore splits at positions outside current bounding box
        if(pos > axis_bounds.first && pos < axis_bounds.second){
            // Hopefully CSE cleans this up
            float belowSA = 2 * (d[otherAxis0]             * d[otherAxis1] +
                                 (pos - axis_bounds.first) * d[otherAxis0] +
                                 (pos - axis_bounds.first) * d[otherAxis1]
                );
            float aboveSA = 2 * (d[otherAxis0]              * d[otherAxis1] +
                                 (axis_bounds.second - pos) * d[otherAxis0] +
                                 (axis_bounds.second - pos) * d[otherAxis1]
                );
            float p_before = belowSA * invTotalSA;
            float p_after = aboveSA * invTotalSA;
            float eb = (before == 0 || after == 0) ? EMPTY_BONUS : 0.f;
            float cost = TRAV_COST +
                         ISECT_COST * (1.f - eb) * (p_before * before + p_after * after);

            //std::cerr << "Potential split at " << pos << " cost " << cost << std::endl;
            //std::cerr << "Before " << before << " After " << after << std::endl;
            if (cost < best_cost)  {
                best_cost = cost;
                best_offset = i;
                best_pos = pos;
                prob0 = p_before;
                prob1 = p_after;
            }
        }else{
            //std::cerr << "Ignoring split at " << pos << std::endl;
            //std::cerr << xBB.first << " " << xBB.second << std::endl;
        }
        if(events[i].type == BBEvent::BEGIN) before++;
    }

    // TODO: If no reasonable split was found at all, try a different axis.
    // TODO: Allow some bad refines, just not too much recursivelly.
    if(best_offset == -1 ||        // No suitable split position found at all.
       best_cost > nosplit_cost || // It is cheaper to not split at all
       false){
        std::cerr << "Not splitting, best cost = " << best_cost << ", nosplit cost = " << nosplit_cost << std::endl;
        return;
    }

    // Note: It is much better to split at a sorted event position
    // rather than a splitting plane position.  This is because many
    // begins/ends may have the same coordinate (in tested axis). The
    // SAH chooses how to split optimally them even though they are at
    // the same position.

    std::cerr << "Splitting at " << best_pos << " ( " << best_offset <<  " ) " <<std::endl;

    // Toggle node type
    type = 1;

    ch0 = new UncompressedKdNode();
    ch1 = new UncompressedKdNode();
    ch0->parent_scene = parent_scene;
    ch1->parent_scene = parent_scene;
    ch0->depth = depth+1;
    ch1->depth = depth+1;

    /*
    std::vector<int> triangles_left;
    dups = 0;
    for(unsigned int i = 0; i < triangle_indices.size(); i++){
        // Check bounds for this triangle
        float e0 = all_events[2*triangle_indices[i] + 0];
        float e1 = all_events[2*triangle_indices[i] + 1];
        if(e1 < split){
            ch0->triangle_indices.push_back(i);
        }else if(e0 > split){
            ch1->triangle_indices.push_back(i);
        }else{
            // Duplicating the triangle as it is, apparently, in both children nodes.
            ch0->triangle_indices.push_back(i);
            ch1->triangle_indices.push_back(i);
            dups++;
            // Leave this triangle in this node.
            //triangles_left.push_back(i);

        }
    }
    */
    for (unsigned int i = 0; i < (unsigned int)best_offset; ++i)
        if (events[i].type == BBEvent::BEGIN)
            ch0->triangle_indices.push_back(events[i].triangleID);
    for (unsigned int i = best_offset + 1; i < 2*n; ++i)
        if (events[i].type == BBEvent::END)
            ch1->triangle_indices.push_back(events[i].triangleID);

    std::cerr << "After split " << ch0->triangle_indices.size() << " " << ch1->triangle_indices.size() << std::endl;

    // Remove triangles from this node
    triangle_indices = std::vector<int>(triangle_indices);
    //triangle_indices = std::move(triangles_left);

    // Prepare new BBs for children
    ch0->xBB = (axis == 0) ? std::make_pair(xBB.first,best_pos) : xBB;
    ch0->yBB = (axis == 1) ? std::make_pair(yBB.first,best_pos) : yBB;
    ch0->zBB = (axis == 2) ? std::make_pair(zBB.first,best_pos) : zBB;
    ch1->xBB = (axis == 0) ? std::make_pair(best_pos,xBB.second) : xBB;
    ch1->yBB = (axis == 1) ? std::make_pair(best_pos,yBB.second) : yBB;
    ch1->zBB = (axis == 2) ? std::make_pair(best_pos,zBB.second) : zBB;

    // If too many duplicates, do not subdivide further.
    if(dups >= n*1.0) {
        std::cerr << "Duplicates : " << dups << ", total to divide: " << n << ", giving up. " << std::endl;
        return;
    }

    // Recursivelly subdivide
    ch0->Subdivide(max_depth);
    ch1->Subdivide(max_depth);

}
void UncompressedKdNode::FreeRecursivelly(){
    if(type == 1){
        ch0->FreeRecursivelly();
        delete ch0;
        ch1->FreeRecursivelly();
        delete ch1;
    }
}

std::tuple<int, int, int, int> UncompressedKdNode::GetTotals() const{
    if(type == 0){ // leaf
        return std::make_tuple(triangle_indices.size(), 1, 1, dups);
    }else{
        auto p0 = ch0->GetTotals();
        auto p1 = ch1->GetTotals();
        int total_triangles = std::get<0>(p0) + std::get<0>(p1);
        int total_leafs = std::get<1>(p0) + std::get<1>(p1);
        int total_nodes = std::get<2>(p0) + std::get<2>(p1) + 1;
        int total_dups = std::get<3>(p0) + std::get<3>(p1) + dups;
        return std::make_tuple(total_triangles, total_leafs, total_nodes, total_dups);
    }
}

float UncompressedKdNode::GetCost() const{
    if(type == 0){ // leaf
        return ISECT_COST * triangle_indices.size();
    }else{
        return TRAV_COST  + prob0 * ch0->GetCost() + prob1 * ch1->GetCost();
    }
}
