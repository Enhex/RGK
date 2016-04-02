#include "scene.hpp"
#include "utils.hpp"

#include <iostream>
#include <limits>
#include <cmath>
#include <stack>

// #define NO_COMPRESS

Scene::~Scene(){
    FreeBuffers();
    FreeTextures();
    FreeCompressedTree();
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

    if(uncompressed_root){
        uncompressed_root->FreeRecursivelly();
        delete uncompressed_root;
    }
}

void Scene::FreeTextures(){
    for(const auto& p : textures){
        delete p.second;
    }
    textures.clear();
}

void Scene::FreeCompressedTree(){
    if(compressed_triangles) delete[] compressed_triangles;
    compressed_triangles_size = 0;
    if(compressed_array) delete[] compressed_array;
    compressed_array_size = 0;
}

void Scene::LoadScene(const aiScene* scene){
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
        mat->Get(AI_MATKEY_COLOR_AMBIENT,c);
        m.ambient = c;
        float f;
        mat->Get(AI_MATKEY_SHININESS, f);
        m.exponent = f/4; // This is weird. Why does assimp multiply by 4 in the first place?

        aiString as; std::string s;
        Texture* tex;
        int n;
        n = mat->GetTextureCount(aiTextureType_DIFFUSE);
        if(n > 0){
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &as); s = as.C_Str();
            if(s != ""){
                // std::cout << "Material has diffuse texture " << s << std::endl;
                tex = GetTexture(s);
                m.diffuse_texture = tex;
            }
        }
        n = mat->GetTextureCount(aiTextureType_SPECULAR);
        if(n > 0){
            mat->GetTexture(aiTextureType_SPECULAR, 0, &as); s = as.C_Str();
            if(s != ""){
                // std::cout << "Material has specular texture " << s << std::endl;
                tex = GetTexture(s);
                m.specular_texture = tex;
            }
        }
        n = mat->GetTextureCount(aiTextureType_AMBIENT);
        if(n > 0){
            mat->GetTexture(aiTextureType_AMBIENT, 0, &as); s = as.C_Str();
            if(s != ""){
                // std::cout << "Material has ambient texture " << s << std::endl;
                tex = GetTexture(s);
                m.ambient_texture = tex;
            }
        }

        materials_buffer.push_back(m);
    }

    // Load root node
    const aiNode* root = scene->mRootNode;
    LoadNode(scene,root);
}

void Scene::LoadNode(const aiScene* scene, const aiNode* ainode, aiMatrix4x4 current_transform){
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

void Scene::LoadMesh(const aiMesh* mesh, aiMatrix4x4 current_transform){
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
            triangles_buffer.emplace(
               triangles_buffer.end(),
               this,
               face.mIndices[0] + vertex_index_offset,
               face.mIndices[1] + vertex_index_offset,
               face.mIndices[2] + vertex_index_offset,
               mat);
        }else{
            std::cerr << "WARNING: Skipping a face that apparently was not tringulated." << std::endl;
        }
    }
    if(mesh->mTextureCoords[0]){
        for(unsigned int v = 0; v < mesh->mNumVertices; v++){
            aiVector3D uv = mesh->mTextureCoords[0][v];
            texcoords_buffer.push_back(uv);
        }
    }
}

Texture* Scene::GetTexture(std::string name){
    if(name == "") return nullptr;
    auto it = textures.find(name);
    if(it == textures.end()){
        auto p = Utils::GetFileExtension(name);
        Texture* t = nullptr;
        if(p.second == "PNG" || p.second == "png"){
            t = Texture::CreateNewFromPNG(texture_directory + name);
        }else if(p.second == "JPG" || p.second == "jpg" || p.second == "JPEG" || p.second == "jpeg"){
            t = Texture::CreateNewFromJPEG(texture_directory + name);
        }else{
            std::cerr << "ERROR: Texture format '" << p.second << "' is not supported!" << std::endl;
        }
        if(!t){
            std::cerr << "Failed to load texture '" << name << "' , ignoring it." << std::endl;
        }
        textures[name] = t;
        return t;
    }
    return nullptr;
}

void Scene::Commit(){
    FreeBuffers();

    vertices = new glm::vec3[vertices_buffer.size()];
    triangles = new Triangle[triangles_buffer.size()];
    materials = new Material[materials_buffer.size()];
    normals = new glm::vec3[normals_buffer.size()];
    texcoords = new glm::vec2[texcoords_buffer.size()];

    n_vertices = vertices_buffer.size();
    n_triangles = triangles_buffer.size();
    n_materials = materials_buffer.size();
    n_normals = normals_buffer.size();
    n_texcoords = texcoords_buffer.size();

    // TODO: memcpy
    for(unsigned int i = 0; i < n_vertices; i++)
        vertices[i] = glm::vec3(vertices_buffer[i].x, vertices_buffer[i].y, vertices_buffer[i].z);
    for(unsigned int i = 0; i < n_triangles; i++){
        triangles[i] = triangles_buffer[i];
        triangles[i].CalculatePlane();
    }
    for(unsigned int i = 0; i < n_materials; i++){
        materials[i] = materials_buffer[i];
    }
    for(unsigned int i = 0; i < n_normals; i++)
        normals[i] = glm::vec3(normals_buffer[i].x, normals_buffer[i].y, normals_buffer[i].z);
    for(unsigned int i = 0; i < n_texcoords; i++)
        texcoords[i] = glm::vec2(texcoords_buffer[i].x, texcoords_buffer[i].y);

    std::cout << "Commited " << n_vertices << " vertices, " << n_normals << " normals,  " << n_triangles <<
        " triangles with " << n_materials << " materials and " << textures.size() <<  " textures to the scene." << std::endl;

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
    const float BB_epsilon = 0.001f;
    xBB = std::make_pair(*p.first - BB_epsilon, *p.second + BB_epsilon);
    yBB = std::make_pair(*q.first - BB_epsilon, *q.second + BB_epsilon);
    zBB = std::make_pair(*r.first - BB_epsilon, *r.second + BB_epsilon);

    std::cout << "The scene is bounded by [" << xBB.first << ", " << xBB.second << "], " <<
                                         "[" << yBB.first << ", " << yBB.second << "], " <<
                                         "[" << zBB.first << ", " << zBB.second << "]."  << std::endl;

    std::cout << "Total avg cost with no kd-tree: " << ISECT_COST * n_triangles << std::endl;

    uncompressed_root = new UncompressedKdNode;
    uncompressed_root->parent_scene = this;
    for(unsigned int i = 0; i < n_triangles; i++) uncompressed_root->triangle_indices.push_back(i);
    uncompressed_root->xBB = xBB;
    uncompressed_root->yBB = yBB;
    uncompressed_root->zBB = zBB;

    // Prepare kd-tree
    int l = std::log2(n_triangles) + 8;
    //l = 1;
    std::cout << "Building kD-tree with max depth " << l << std::endl;
    uncompressed_root->Subdivide(l);

    auto totals = uncompressed_root->GetTotals();
    std::cout << "Total triangles in tree: " << std::get<0>(totals) << ", total leafs: " << std::get<1>(totals) << ", total nodes: " << std::get<2>(totals) << ", total dups: " << std::get<3>(totals) << std::endl;
    std::cout << "Average triangles per leaf: " << std::get<0>(totals)/(float)std::get<1>(totals) << std::endl;

    std::cout << "Total avg cost with kd-tree: " << uncompressed_root->GetCost() << std::endl;

#ifndef NO_COMPRESS
    Compress();

    uncompressed_root->FreeRecursivelly();
    delete uncompressed_root;
    uncompressed_root = nullptr;
#endif
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

Intersection Scene::FindIntersect(const Ray& __restrict__ r, bool debug) const{

    if(compressed_array != nullptr) return FindIntersectKdCompressed(r, debug);
    if(uncompressed_root != nullptr) return FindIntersectKdUncompressed(r, debug);

    // fallback solution: iterate over ALL triangles, find ALL
    // intersections, pick nearest. Super inefficient.

    // create inf result
    Intersection res;
    res.triangle = nullptr;
    res.t = std::numeric_limits<float>::infinity();

    for(unsigned int f = 0; f < n_triangles; f++){
        const Triangle& tri = triangles[f];
        float t, a, b;
        if(tri.TestIntersection(r, t, a, b)){
            if(t <= r.near || t >= r.far) continue;

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

    return res;
}

void UncompressedKdNode::Subdivide(unsigned int max_depth){
    if(depth >= max_depth) return; // Do not subdivide further.

    unsigned int n = triangle_indices.size();
    if(n < 2) return; // Do not subdivide further.

    //std::cerr << "--- Subdividing " << n << " faces" << std::endl;

    // Choose the axis for subdivision.
    // TODO: Do it faster
    float sizes[3] = {xBB.second - xBB.first, yBB.second - yBB.first, zBB.second - zBB.first};
    unsigned int axis = std::max_element(sizes, sizes+3) - sizes;

    //std::cerr << "Using axis " << axis << std::endl;
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


    // SAH, inspired by the pbrt book.
    const std::pair<float,float>* axbds[3] = {&xBB,&yBB,&zBB};
    const std::pair<float,float>& axis_bounds = *axbds[axis];
    const float BBsize[3] = {xBB.second - xBB.first, yBB.second - yBB.first, zBB.second - zBB.first};

    int best_offset = -1;
    float best_cost = std::numeric_limits<float>::infinity();
    float best_pos  = std::numeric_limits<float>::infinity();
    float nosplit_cost = ISECT_COST * n; // The estimated traversal costs of this node if we choose not to split it
    unsigned int axis2 = (axis + 1) % 3, axis3 = (axis + 2) % 3;
    float invTotalSA = 1.f / (2.f * (BBsize[0]*BBsize[1] + BBsize[0]*BBsize[2] + BBsize[1]*BBsize[2]));
    int n_before = 0, n_after = n;
    for(unsigned int i = 0; i < 2*n; i++){
        if(events[i].type == BBEvent::END)
            n_after--;
        float pos = events[i].pos;
        // Ignore splits at positions outside current bounding box
        if(pos > axis_bounds.first && pos < axis_bounds.second){
            // Hopefully CSE cleans this up
            float below_surface_area = 2 * (BBsize[axis2]             * BBsize[axis3] +
                                            (pos - axis_bounds.first) * BBsize[axis2] +
                                            (pos - axis_bounds.first) * BBsize[axis3]
                                            );
            float above_surface_area = 2 * (BBsize[axis2]              * BBsize[axis3] +
                                            (axis_bounds.second - pos) * BBsize[axis2] +
                                            (axis_bounds.second - pos) * BBsize[axis3]
                                            );
            float p_before = below_surface_area * invTotalSA;
            float p_after  = above_surface_area * invTotalSA;
            float bonus = (n_before == 0 || n_after == 0) ? EMPTY_BONUS : 0.f;
            float cost = TRAV_COST +
                         ISECT_COST * (1.f - bonus) * (p_before * n_before + p_after * n_after);

            if (cost < best_cost)  {
                best_cost = cost;
                best_offset = i;
                best_pos = pos;
                prob0 = p_before;
                prob1 = p_after;
            }
        }
        if(events[i].type == BBEvent::BEGIN)
            n_before++;
    }

    // TODO: If no reasonable split was found at all, try a different axis.
    // TODO: Allow some bad refines, just not too much recursivelly.
    if(best_offset == -1 ||        // No suitable split position found at all.
       best_cost > nosplit_cost || // It is cheaper to not split at all
       false){
        //std::cerr << "Not splitting, best cost = " << best_cost << ", nosplit cost = " << nosplit_cost << std::endl;
        return;
    }

    // Note: It is much better to split at a sorted event position
    // rather than a splitting plane position.  This is because many
    // begins/ends may have the same coordinate (in tested axis). The
    // SAH chooses how to split optimally them even though they are at
    // the same position.

    //std::cerr << "Splitting at " << best_pos << " ( " << best_offset <<  " ) " <<std::endl;

    // Toggle node type
    type = UncompressedKdNode::INTERNAL;

    ch0 = new UncompressedKdNode();
    ch1 = new UncompressedKdNode();
    ch0->parent_scene = parent_scene;
    ch1->parent_scene = parent_scene;
    ch0->depth = depth+1;
    ch1->depth = depth+1;

    split_axis = axis;
    split_pos = best_pos;

    for (unsigned int i = 0; i < (unsigned int)best_offset; ++i)
        if (events[i].type == BBEvent::BEGIN)
            ch0->triangle_indices.push_back(events[i].triangleID);
    for (unsigned int i = best_offset + 1; i < 2*n; ++i)
        if (events[i].type == BBEvent::END)
            ch1->triangle_indices.push_back(events[i].triangleID);

    //std::cerr << "After split " << ch0->triangle_indices.size() << " " << ch1->triangle_indices.size() << std::endl;

    // Remove triangles from this node
    triangle_indices = std::vector<unsigned int>(triangle_indices);

    // Prepare new BBs for children
    ch0->xBB = (axis == 0) ? std::make_pair(xBB.first,best_pos) : xBB;
    ch0->yBB = (axis == 1) ? std::make_pair(yBB.first,best_pos) : yBB;
    ch0->zBB = (axis == 2) ? std::make_pair(zBB.first,best_pos) : zBB;
    ch1->xBB = (axis == 0) ? std::make_pair(best_pos,xBB.second) : xBB;
    ch1->yBB = (axis == 1) ? std::make_pair(best_pos,yBB.second) : yBB;
    ch1->zBB = (axis == 2) ? std::make_pair(best_pos,zBB.second) : zBB;

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

Intersection Scene::FindIntersectKdUncompressed(const Ray& __restrict__ r, bool debug) const{
    //if(debug) std::cerr << "------ Test for intersection " << r.origin << " " << r.direction << std::endl;
    (void)debug;

    Intersection res;
    res.triangle = nullptr;
    res.t = std::numeric_limits<float>::infinity();

    if(uncompressed_root == nullptr){
        std::cout << "CRITICAL: FindIntersectKdUncompressed called while uncompressed_root is not yet ready or was already removed." << std::endl;
        exit(1);
        return res;
    }

    // First, check whether the ray intersects with our BB at all.

    const std::pair<float,float>* bb[3] = {&xBB,&yBB,&zBB};

    // Inspired by pbrt's bbox intersection
    float t0 = r.near, t1 = r.far;
    for (int i = 0; i < 3; ++i) {
        float invRayDir = 1.f / r.direction[i];
        float tNear = ((*bb[i]).first  - r.origin[i]) * invRayDir;
        float tFar  = ((*bb[i]).second - r.origin[i]) * invRayDir;
        if (tNear > tFar) std::swap(tNear, tFar);
        t0 = tNear > t0 ? tNear : t0;
        t1 = tFar  < t1 ? tFar  : t1;
        if (t0 > t1) return res; // No intersection.
    }

    struct NodeToDo{
        const UncompressedKdNode* node;
        float tmin, tmax;
    };

    glm::vec3 invDir(1.f/r.direction.x, 1.f/r.direction.y, 1.f/r.direction.z);

    std::stack<NodeToDo> todo;
    todo.push(NodeToDo{uncompressed_root, t0, t1});

    //if(debug) std::cerr << "Is in the box. " << std::endl;

    while(!todo.empty()){
        const UncompressedKdNode* node = todo.top().node;
        float tmin = todo.top().tmin;
        float tmax = todo.top().tmax;
        todo.pop();

        // Abort if we got too far.
        if(r.far < tmin) break;

        if(node->type == 0){ // leaf node
            if(debug) std::cerr << " -- Testing a leaf node " << tmin << " " << tmax << ", triangles: " << node->triangle_indices.size() << std::endl;

            bool hit = false;
            int hit_i = -1;
            // Search for intersections with triangles inside this node
            for(const int& i : node->triangle_indices){
                // For each triangle ...
                const Triangle& tri = triangles[i];
                float t, a, b;
                //  ... test for an intersection
                bool d = debug && i == 8;
                if(d) std::cout << "Debugging intersection with 8th triangle" << std::endl;
                if(tri.TestIntersection(r, t, a, b, d)){
                    if(t < tmin - 0.00001f || t > tmax + 0.0001f){
                        if(debug) std::cerr << "Skipping t " << t << " at triangle " << i << std::endl;
                        continue;
                    }
                    if(t < res.t){
                        // New closest intersect!
                        res.triangle = &tri;
                        res.t = t;

                        float c = 1.0f - a - b;
                        res.a = c;
                        res.b = a;
                        res.c = b;

                        hit = true;
                        hit_i = i;

                        if(debug) std::cerr << "HIT " << r[res.t] << " triangle " << hit_i << std::endl;
                    }
                }
            }

            if(hit){
                if(debug) std::cerr << "BEST HIT " << r[res.t] << " triangle " << hit_i << std::endl;
                return res;
            }
            if(debug) std::cerr << "No hit" << std::endl;

        }else{ // internal node
            if(debug) std::cerr << " -- Testing an internal node " << tmin << " " << tmax << std::endl;

            // Compute parametric distance along ray to split plane
            int axis = node->split_axis;
            float tplane = (node->split_pos - r.origin[axis]) * invDir[axis];

            if(debug) std::cerr << "split axis: " << axis <<  " tplane: " << tplane << std::endl;

            // Get node children pointers for ray
            const UncompressedKdNode *firstChild, *secondChild;
            int belowFirst = (r.origin[axis] <  node->split_pos) ||
                             (r.origin[axis] == node->split_pos && r.direction[axis] <= 0);
            if (belowFirst) {
                firstChild = node->ch0;
                secondChild = node->ch1;
            }else{
                firstChild = node->ch1;
                secondChild = node->ch0;
            }

            // Advance to next child node, possibly enqueue other child
            if (tplane > tmax || tplane <= 0)
                todo.push(NodeToDo{firstChild, tmin, tmax});
            else if (tplane < tmin)
                todo.push(NodeToDo{secondChild, tmin, tmax});
            else {
                // Enqueue _secondChild_ in todo list
                todo.push(NodeToDo{secondChild, tplane, tmax});
                todo.push(NodeToDo{firstChild,  tmin, tplane});
            }
        }
    }

    // No hit found at all.
    return res;
}


void Scene::Compress(){
    if(uncompressed_root == nullptr) return;

    FreeCompressedTree();

    auto totals = uncompressed_root->GetTotals();
    compressed_array_size = std::get<2>(totals);
    compressed_triangles_size = std::get<0>(totals);

    compressed_array = new CompressedKdNode[compressed_array_size];
    compressed_triangles = new unsigned int[compressed_triangles_size];

    unsigned int array_pos = 0, triangle_pos = 0;
    CompressRec(uncompressed_root, array_pos, triangle_pos);

    // Asserts
    if(array_pos != compressed_array_size){
        std::cout << "Compression failed, array_pos = " << array_pos << ", array_size = " << compressed_array_size << std::endl;
        return;
    }
    if(triangle_pos != compressed_triangles_size){
        std::cout << "Compression failed, triangle_pos = " << triangle_pos << ", triangles_size = " << compressed_triangles_size << std::endl;
        return;
    }
    std::cout << "Compression appears successful!" << std::endl;
    std::cout << "Uncompressed node size: " << sizeof(UncompressedKdNode) << "B " << std::endl;
    std::cout << "Compressed node size: " << sizeof(CompressedKdNode) << "B " << std::endl;
    std::cout << "Total compressed Kd tree size: " << sizeof(CompressedKdNode)*compressed_array_size/1024 << "kiB " << std::endl;

}

void Scene::CompressRec(const UncompressedKdNode *node, unsigned int &array_pos, unsigned int &triangle_pos){
    if(node->type == UncompressedKdNode::LEAF){
        // Leaf node
        compressed_array[array_pos] = CompressedKdNode(node->triangle_indices.size(), compressed_triangles + triangle_pos);
        array_pos++;
        // Fill in triangles
        for(unsigned int t : node->triangle_indices)
            compressed_triangles[triangle_pos++] = t;
    }else{
        // Internal node
        unsigned int my_pos = array_pos;
        compressed_array[array_pos] = CompressedKdNode(node->split_axis, node->split_pos);
        array_pos++;
        // Left child
        CompressRec(node->ch0, array_pos, triangle_pos);
        // Store right child pos to parent
        compressed_array[my_pos].SetOtherChild(array_pos);
        // Right child
        CompressRec(node->ch1, array_pos, triangle_pos);
    }
}


Intersection Scene::FindIntersectKdCompressed(const Ray& __restrict__ r, bool debug) __restrict__ const{
    (void)debug;

    Intersection res;
    res.triangle = nullptr;
    res.t = std::numeric_limits<float>::infinity();


    // First, check whether the ray intersects with our BB at all.

    const  std::pair<float,float>* __restrict  bb[3] = {&xBB,&yBB,&zBB};

    // Inspired by pbrt's bbox intersection
    float t0 = r.near, t1 = r.far;
    for (int i = 0; i < 3; ++i) {
        float invRayDir = 1.f / r.direction[i];
        float tNear = ((*bb[i]).first  - r.origin[i]) * invRayDir;
        float tFar  = ((*bb[i]).second - r.origin[i]) * invRayDir;
        if (tNear > tFar) std::swap(tNear, tFar);
        t0 = tNear > t0 ? tNear : t0;
        t1 = tFar  < t1 ? tFar  : t1;
        if (t0 > t1) return res; // No intersection.
    }

    struct NodeToDo{
        const CompressedKdNode* node;
        float tmin, tmax;
    };


    glm::vec3 invDir(1.f/r.direction.x, 1.f/r.direction.y, 1.f/r.direction.z);

    NodeToDo todo[200];
    int todo_size = 1;
    todo[0] = NodeToDo{compressed_array, t0, t1};

    while(todo_size > 0){
        todo_size--;
        const CompressedKdNode* node = todo[todo_size].node;
        float tmin = todo[todo_size].tmin;
        float tmax = todo[todo_size].tmax;

        // Abort if we got too far.
        if(r.far < tmin) break;

        if(node->IsLeaf()){ // leaf node
            //if(debug) std::cerr << " -- Testing a leaf node " << tmin << " " << tmax << ", triangles: " << node->GetTrianglesN() << std::endl;

            bool hit = false;
            //int hit_i = -1;
            // Search for intersections with triangles inside this node
            unsigned int n = node->GetTrianglesN();
            unsigned int* tri_start = node->GetFirstTrianglePos();
            for(unsigned int p = 0; p < n; p++){
                // For each triangle ...
                unsigned int i = *(tri_start + p);
                const Triangle& tri = triangles[i];
                float t, a, b;
                //  ... test for an intersection
                if(tri.TestIntersection(r, t, a, b)){
                    if(t < tmin - 0.00001f || t > tmax + 0.0001f){
                        //if(debug) std::cerr << "Skipping t " << t << " at triangle " << i << std::endl;
                        continue;
                    }
                    if(t < res.t){
                        // New closest intersect!
                        res.triangle = &tri;
                        res.t = t;

                        float c = 1.0f - a - b;
                        res.a = c;
                        res.b = a;
                        res.c = b;

                        hit = true;
                        //hit_i = i;

                        //if(debug) std::cerr << "HIT " << r[res.t] << " triangle " << hit_i << std::endl;
                    }
                }
            }

            if(hit){
                //if(debug) std::cerr << "BEST HIT " << r[res.t] << " triangle " << hit_i << std::endl;
                return res;
            }
            //if(debug) std::cerr << "No hit" << std::endl;

        }else{ // internal node
            //if(debug) std::cerr << " -- Testing an internal node " << tmin << " " << tmax << std::endl;

            // Compute parametric distance along ray to split plane
            int axis = node->GetSplitAxis();
            float tplane = (node->GetSplitPlane() - r.origin[axis]) * invDir[axis];

            //if(debug) std::cerr << "split axis: " << axis <<  " tplane: " << tplane << std::endl;

            // Get node children pointers for ray
            const CompressedKdNode *firstChild, *secondChild;
            int belowFirst = (r.origin[axis] <  node->GetSplitPlane()) ||
                             (r.origin[axis] == node->GetSplitPlane() && r.direction[axis] <= 0);
            if (belowFirst) {
                firstChild = node + 1;
                secondChild = compressed_array + node->GetOtherChildIndex();
            }else{
                firstChild = compressed_array + node->GetOtherChildIndex();
                secondChild = node + 1;
            }

            // Advance to next child node, possibly enqueue other child
            if (tplane > tmax || tplane <= 0)
                todo[todo_size++] = NodeToDo{firstChild, tmin, tmax};
            else if (tplane < tmin)
                todo[todo_size++] = NodeToDo{secondChild, tmin, tmax};
            else {
                // Enqueue _secondChild_ in todo list
                todo[todo_size++] = NodeToDo{secondChild, tplane, tmax};
                todo[todo_size++] = NodeToDo{firstChild,  tmin, tplane};
            }
        }
    }

    // No hit found at all.
    return res;
}
