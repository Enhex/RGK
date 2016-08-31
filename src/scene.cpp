#include "scene.hpp"

#include <iostream>
#include <limits>
#include <cmath>
#include <stack>

#include "utils.hpp"
#include "out.hpp"

// #define NO_COMPRESS

Scene::~Scene(){
    FreeBuffers();
    FreeTextures();
    FreeMaterials();
    FreeCompressedTree();
}

void Scene::FreeBuffers(){
    if(vertices) delete[] vertices;
    n_vertices = 0;
    if(triangles) delete[] triangles;
    n_triangles = 0;
    if(normals) delete[] normals;
    n_normals = 0;

    if(uncompressed_root){
        uncompressed_root->FreeRecursivelly();
        delete uncompressed_root;
    }
}

void Scene::FreeMaterials(){
    for(Material* m : materials) delete m;
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

void Scene::LoadAiSceneMaterials(const aiScene* scene, std::string default_brdf, std::string texture_directory, bool override_materials){
    // Load materials
    for(unsigned int i = 0; i < scene->mNumMaterials; i++){
        LoadAiMaterial(scene->mMaterials[i], default_brdf, texture_directory, override_materials);
    }
}

void Scene::LoadAiSceneMeshes(const aiScene* scene, std::string force_mat){
    // Load root node
    const aiNode* root = scene->mRootNode;
    LoadAiNode(scene,root,aiMatrix4x4(),force_mat);
}


void Scene::LoadAiMaterial(const aiMaterial* mat, std::string brdf, std::string texture_directory, bool override){

    Material m;
    aiString name;
    mat->Get(AI_MATKEY_NAME, name);
    m.name = name.C_Str();
    aiColor3D c;
    mat->Get(AI_MATKEY_COLOR_DIFFUSE,c);
    m.diffuse = c;
    mat->Get(AI_MATKEY_COLOR_SPECULAR,c);
    m.specular = c;
    mat->Get(AI_MATKEY_COLOR_AMBIENT,c);
    m.ambient = c;
    mat->Get(AI_MATKEY_COLOR_EMISSIVE,c);
    m.emission = c;
    float f;
    mat->Get(AI_MATKEY_SHININESS, f);
    m.exponent = f/4; // This is weird. Why does assimp multiply by 4 in the first place?
    mat->Get(AI_MATKEY_REFRACTI, f);
    m.refraction_index = f;
    mat->Get(AI_MATKEY_OPACITY, f);
    m.translucency = 1.0f - f;

    aiString as; std::string s;
    Texture* tex;
    int n;
    n = mat->GetTextureCount(aiTextureType_DIFFUSE);
    if(n > 0){
        mat->GetTexture(aiTextureType_DIFFUSE, 0, &as); s = as.C_Str();
        if(s != ""){
            out::cout(5) << "Material has diffuse texture " << s << std::endl;
            tex = GetTexture(texture_directory + s);
            m.diffuse_texture = tex;
        }
    }
    n = mat->GetTextureCount(aiTextureType_SPECULAR);
    if(n > 0){
        mat->GetTexture(aiTextureType_SPECULAR, 0, &as); s = as.C_Str();
        if(s != ""){
            out::cout(5) << "Material has specular texture " << s << std::endl;
            tex = GetTexture(texture_directory + s);
            m.specular_texture = tex;
        }
    }
    n = mat->GetTextureCount(aiTextureType_AMBIENT);
    if(n > 0){
        mat->GetTexture(aiTextureType_AMBIENT, 0, &as); s = as.C_Str();
        if(s != ""){
            out::cout(5) << "Material has ambient texture " << s << std::endl;
            tex = GetTexture(texture_directory + s);
            m.ambient_texture = tex;
        }
    }
    n = mat->GetTextureCount(aiTextureType_HEIGHT);
    if(n > 0){
        mat->GetTexture(aiTextureType_HEIGHT, 0, &as); s = as.C_Str();
        if(s != ""){
            out::cout(5) << "Material has bump texture " << s << std::endl;
            tex = GetTexture(texture_directory + s);
            m.bump_texture = tex;
        }
    }

    if(m.emission.r > 0.0f || m.emission.g > 0.0f || m.emission.b > 0.0f){
        out::cout(4) << "Material is emissive" << std::endl;
        m.emissive = true;
    }

    // Supposedly we may support different brdfs for each material.
    if(brdf == "diffuseuniform"){
        m.brdf = std::make_unique<BRDFDiffuseUniform>();
    }else if(brdf == "diffusecosine"){
        m.brdf = std::make_unique<BRDFDiffuseCosine>();
    }else if(brdf == "cooktorr"){
        m.brdf = std::make_unique<BRDFCookTorr>(m.exponent, m.refraction_index);
    }else if(brdf == "ltc_beckmann"){
        m.brdf = std::make_unique<BRDFLTCBeckmann>(m.exponent);
    }else if(brdf == "ltc_ggx"){
        m.brdf = std::make_unique<BRDFLTCGGX>(m.exponent);
    }else if(brdf == "phongenergy"){
        m.brdf = std::make_unique<BRDFPhongEnergy>(m.exponent);
    }else{
        assert(0 * (size_t)"Unsupported BRDF id in config!");
    }

    out::cout(4) << "Read material: " << m.name << std::endl;
    RegisterMaterial(m, override);
}

void Scene::RegisterMaterial(const Material &mat, bool override){
    Material* material = new Material(mat);
    auto it = materials_by_name.find(material->name);
    if(it != materials_by_name.end()){
        // Already exists
        if(!override){
            return;
            // throw std::runtime_error("Cannot register a duplicate material: \"" + material->name + "\"");
        }else{
            // Substitute the old material with the new one
            Material* oldmat = it->second;
            // Find it in the vector and remove...
            materials.erase(std::remove(materials.begin(), materials.end(), oldmat), materials.end());
            // Insert the new one
            materials.push_back(material);
            materials_by_name[material->name] = material;
            // Free the old one
            delete oldmat;
        }
    }else{
        // New material
        materials.push_back(material);
        materials_by_name[material->name] = material;
    }
}

void Scene::LoadAiNode(const aiScene* scene, const aiNode* ainode, aiMatrix4x4 current_transform, std::string force_mat){

    aiMatrix4x4 transform = current_transform * ainode->mTransformation;

    // Load meshes
    for(unsigned int i = 0; i < ainode->mNumMeshes; i++)
        LoadAiMesh(scene, scene->mMeshes[ainode->mMeshes[i]], transform, force_mat);

    // Load children
    for(unsigned int i = 0; i < ainode->mNumChildren; i++)
        LoadAiNode(scene, ainode->mChildren[i], transform, force_mat);

}

void Scene::LoadAiMesh(const aiScene* scene, const aiMesh* mesh, aiMatrix4x4 current_transform, std::string force_mat){
    out::cout(4) << "-- Loading a mesh with " << mesh->mNumFaces <<
       " faces and " << mesh->mNumVertices <<  " vertices." << std::endl;

    // Keep the current vertex buffer size.
    unsigned int vertex_index_offset = vertices_buffer.size();

    // Get the material index.
    unsigned int mat_id = mesh->mMaterialIndex;
    Material* material;
    // Find the material by name.
    if(force_mat == ""){
        aiString mat_name;
        scene->mMaterials[mat_id]->Get(AI_MATKEY_NAME,mat_name);
        material = GetMaterialByName(mat_name.C_Str());
    }else{
        material = GetMaterialByName(force_mat);
    }

    bool light_source = material->emissive;
    ArealLight al;

    for(unsigned int v = 0; v < mesh->mNumVertices; v++){
        aiVector3D vertex = mesh->mVertices[v];
        vertex *= current_transform;
        vertices_buffer.push_back(glm::vec3(vertex.x, vertex.y, vertex.z));
    }
    for(unsigned int v = 0; v < mesh->mNumVertices; v++){
        aiVector3D normal = mesh->mNormals[v];
        normal *= aiMatrix3x3(current_transform);
        normals_buffer.push_back(glm::vec3(normal.x, normal.y, normal.z));
    }
    for(unsigned int f = 0; f < mesh->mNumFaces; f++){
        const aiFace& face = mesh->mFaces[f];
        if(face.mNumIndices < 3) continue; // Ignore degenerated faces
        if(face.mNumIndices == 3){
            Triangle t(this,
                       face.mIndices[0] + vertex_index_offset,
                       face.mIndices[1] + vertex_index_offset,
                       face.mIndices[2] + vertex_index_offset,
                       material);
            triangles_buffer.push_back(t);
            int n = triangles_buffer.size() - 1;
            if(light_source){
                al.triangles_with_areas.push_back(std::make_pair(0.0f, n));
            }
        }else{
            std::cerr << "WARNING: Skipping a face that apparently was not triangulated (" << face.mNumIndices << ")." << std::endl;
        }
    }
    if(mesh->mTextureCoords[0]){
        for(unsigned int v = 0; v < mesh->mNumVertices; v++){
            aiVector3D uv = mesh->mTextureCoords[0][v];
            texcoords_buffer.push_back(glm::vec2(uv.x, uv.y));
        }
    }else{
        // Fill with empty coords
        texcoords_buffer.resize(vertices_buffer.size());
    }
    if(mesh->mTangents){
        for(unsigned int v = 0; v < mesh->mNumVertices; v++){
            aiVector3D tangent = mesh->mTangents[v];
            tangent *= aiMatrix3x3(current_transform);
            tangents_buffer.push_back(glm::vec3(tangent.x, tangent.y, tangent.z));
        }
    }else{
        // Fill with empty coords
        tangents_buffer.resize(vertices_buffer.size());
    }

    assert(normals_buffer.size() == vertices_buffer.size());
    assert(texcoords_buffer.size() == vertices_buffer.size());
    assert(tangents_buffer.size() == vertices_buffer.size());

    if(light_source && al.triangles_with_areas.size() > 0){
        areal_lights.push_back(std::make_pair(0.0f, al));
    }
}


void Scene::AddPrimitive(const primitive_data& primitive, glm::mat4 transform, std::string material){
    qassert_true(primitive.size() % 3 == 0);
    out::cout(4) << "-- Adding a primitive with " << primitive.size()/3 << " faces." << std::endl;
    unsigned int vertex_index_offset = vertices_buffer.size();
    Material* mat = GetMaterialByName(material);
    bool light_source = mat->emissive;
    ArealLight al;

    for(unsigned int i = 0; i < primitive.size(); i++){
        glm::vec3 vertex, normal, tangent; glm::vec2 texcoords;
        std::tie(vertex, normal, texcoords, tangent) = primitive[i];
        vertex = (transform*glm::vec4(vertex, 1.0f)).xyz();
        normal = glm::normalize((transform*glm::vec4(normal, 0.0f)).xyz());
        tangent = glm::normalize((transform*glm::vec4(tangent, 0.0f)).xyz());
        vertices_buffer.push_back(vertex);
        normals_buffer.push_back(normal);
        tangents_buffer.push_back(tangent);
        texcoords_buffer.push_back(texcoords);
    }
    for(unsigned int i = 0; i < primitive.size()/3; i++){
        Triangle t(this,
                   vertex_index_offset + 0 + i*3,
                   vertex_index_offset + 1 + i*3,
                   vertex_index_offset + 2 + i*3,
                   mat);
        triangles_buffer.push_back(t);
        int n = triangles_buffer.size() - 1;
        if(light_source){
            al.triangles_with_areas.push_back(std::make_pair(0.0f, n));
        }
    }

    assert(normals_buffer.size() == vertices_buffer.size());
    assert(texcoords_buffer.size() == vertices_buffer.size());
    assert(tangents_buffer.size() == vertices_buffer.size());

    if(light_source && al.triangles_with_areas.size() > 0){
        areal_lights.push_back(std::make_pair(0.0f, al));
    }
}

Texture* Scene::GetTexture(std::string path){
    if(path == "") return nullptr;
    auto it = textures.find(path);
    if(it == textures.end()){
        auto p = Utils::GetFileExtension(path);
        Texture* t = nullptr;
        if(p.second == "PNG" || p.second == "png"){
            t = Texture::CreateNewFromPNG(path);
        }else if(p.second == "JPG" || p.second == "jpg" || p.second == "JPEG" || p.second == "jpeg"){
            t = Texture::CreateNewFromJPEG(path);
        }else{
            std::cerr << "ERROR: Texture format '" << p.second << "' is not supported!" << std::endl;
        }
        if(!t){
            std::cerr << "Failed to load texture '" << path << "' , ignoring it." << std::endl;
        }else{
            textures[path] = t;
        }
        return t;
    }else{
        return it->second;
    }
}

Material* Scene::GetMaterialByName(std::string name) const{
    auto it = materials_by_name.find(name);
    if(it == materials_by_name.end()){
        throw std::runtime_error("Error: Material named \"" + name + "\" was not defined");
    }
    return it->second;
}

void Scene::Commit(){
    FreeBuffers();

    vertices = new glm::vec3[vertices_buffer.size()];
    triangles = new Triangle[triangles_buffer.size()];
    normals = new glm::vec3[normals_buffer.size()];
    texcoords = new glm::vec2[texcoords_buffer.size()];
    tangents = new glm::vec3[tangents_buffer.size()];

    n_vertices = vertices_buffer.size();
    n_triangles = triangles_buffer.size();
    n_normals = normals_buffer.size();
    n_texcoords = texcoords_buffer.size();
    n_tangents = tangents_buffer.size();

    // TODO: memcpy
    for(unsigned int i = 0; i < n_vertices; i++)
        vertices[i] = glm::vec3(vertices_buffer[i].x, vertices_buffer[i].y, vertices_buffer[i].z);
    for(unsigned int i = 0; i < n_triangles; i++){
        triangles[i] = triangles_buffer[i];
        triangles[i].CalculatePlane();
    }
    for(unsigned int i = 0; i < n_normals; i++)
        normals[i] = glm::vec3(normals_buffer[i].x, normals_buffer[i].y, normals_buffer[i].z);
    for(unsigned int i = 0; i < n_tangents; i++)
        tangents[i] = glm::vec3(tangents_buffer[i].x, tangents_buffer[i].y, tangents_buffer[i].z);
    for(unsigned int i = 0; i < n_texcoords; i++)
        texcoords[i] = glm::vec2(texcoords_buffer[i].x, texcoords_buffer[i].y);

    // It is safe now to calculate all light areas.
    total_areal_power = 0.0f;
    for(auto& q : areal_lights){
        ArealLight& al = q.second;
        for(auto& p : al.triangles_with_areas){
            Triangle t = triangles[p.second];
            float area = t.GetArea();
            p.first = area;
            al.total_area += area;
        }
        al.emission = triangles[al.triangles_with_areas[0].second].GetMaterial().emission;
        // Sort descending
        std::sort(al.triangles_with_areas.rbegin(), al.triangles_with_areas.rend());
        float p = al.total_area * (al.emission.r + al.emission.g + al.emission.b);
        al.power = p;
        q.first = p;
        total_areal_power += p;
    }
    total_point_power = 0.0f;
    for(auto& l : pointlights){
        total_point_power += l.intensity * 4.0f * glm::pi<float>();
    }

    out::cout(3) << "Total areal lights power: " << total_areal_power << "W" << std::endl;
    out::cout(3) << "Total point lights power: " << total_point_power << "W" << std::endl;

    out::cout(2) << "Commited " << n_vertices << " vertices, " << n_normals << " normals, " << n_triangles <<
        " triangles with " << textures.size() <<  " textures, as well as " << pointlights.size() << " pointlights and " << areal_lights.size() << " areal lights to the scene." << std::endl;

    // Clearing vectors this way forces memory to be freed.
    vertices_buffer  = std::vector<glm::vec3>();
    triangles_buffer = std::vector<Triangle>();
    normals_buffer   = std::vector<glm::vec3>();
    tangents_buffer  = std::vector<glm::vec3>();
    texcoords_buffer = std::vector<glm::vec2>();

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

    float xsize = *p.second - *p.first;
    float ysize = *q.second - *q.first;
    float zsize = *r.second - *r.first;
    float diameter = glm::sqrt(xsize*xsize + ysize*ysize + zsize*zsize);

    epsilon = 0.00001f * diameter;
    out::cout(3) << "Using dynamic epsilon: " << epsilon << std::endl;

    xBB = std::make_pair(*p.first - epsilon, *p.second + epsilon);
    yBB = std::make_pair(*q.first - epsilon, *q.second + epsilon);
    zBB = std::make_pair(*r.first - epsilon, *r.second + epsilon);

    out::cout(3) << "The scene is bounded by [" << xBB.first << ", " << xBB.second << "], " <<
                                         "[" << yBB.first << ", " << yBB.second << "], " <<
                                         "[" << zBB.first << ", " << zBB.second << "]."  << std::endl;

    uncompressed_root = new UncompressedKdNode;
    uncompressed_root->parent_scene = this;
    for(unsigned int i = 0; i < n_triangles; i++) uncompressed_root->triangle_indices.push_back(i);
    uncompressed_root->xBB = xBB;
    uncompressed_root->yBB = yBB;
    uncompressed_root->zBB = zBB;

    // Prepare kd-tree
    int l = std::log2(n_triangles) + 8;
    //l = 1;
    out::cout(2) << "Building kD-tree with max depth " << l << "..." << std::endl;
    uncompressed_root->Subdivide(l);

    auto totals = uncompressed_root->GetTotals();
    out::cout(3) << "Total triangles in tree: " << std::get<0>(totals) << ", total leafs: " << std::get<1>(totals) << ", total nodes: " << std::get<2>(totals) << ", total dups: " << std::get<3>(totals) << std::endl;
    out::cout(3) << "Average triangles per leaf: " << std::get<0>(totals)/(float)std::get<1>(totals) << std::endl;

    out::cout(3) << "Total avg cost with no kd-tree: " << ISECT_COST * n_triangles << std::endl;
    out::cout(3) << "Total avg cost with kd-tree: " << uncompressed_root->GetCost() << std::endl;

#ifndef NO_COMPRESS
    out::cout(2) << "Compressing kD-tree..." << std::endl;
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
        const Color& color = tr.mat->diffuse;
        out::cout(4) << va.x << " " << va.y << " " << va.z << " | ";
        out::cout(4) << vb.x << " " << vb.y << " " << vb.z << " | ";
        out::cout(4) << vc.x << " " << vc.y << " " << vc.z << " [" ;
        out::cout(4) << color.r << " " << color.g << " " << color.b << "]\n" ;
    }
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

    unsigned int retries = 0;
 retry: // Return point for retrying with a different axis

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

    // TODO: Allow some bad refines, just not too much recursivelly.
    if(best_offset == -1 ||        // No suitable split position found at all.
       best_cost > nosplit_cost || // It is cheaper to not split at all
       false){
        if(retries < 2){
            // If no reasonable split was found at all, try a different axis.
            retries++;
            axis = (axis+1)%3;
            goto retry;
            }
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
    out::cout(3) << "Compression appears successful!" << std::endl;
    out::cout(3) << "Uncompressed node size: " << sizeof(UncompressedKdNode) << "B " << std::endl;
    out::cout(3) << "Compressed node size: " << sizeof(CompressedKdNode) << "B " << std::endl;
    out::cout(2) << "Total compressed Kd tree size: " << sizeof(CompressedKdNode)*compressed_array_size/1024 << "kiB " << std::endl;

}

void Scene::CompressRec(const UncompressedKdNode *node, unsigned int &array_pos, unsigned int &triangle_pos){
    if(node->type == UncompressedKdNode::LEAF){
        // Leaf node
        compressed_array[array_pos] = CompressedKdNode(node->triangle_indices.size(), triangle_pos);
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

std::set<const Material*> Scene::MakeMaterialSet(std::vector<std::string> phrases) const{
    std::set<const Material*> res;
    for(Material* m : materials){
        for(const std::string& phrase : phrases){
            if(m->name.find(phrase) != std::string::npos){
                res.insert(m);
                break;
            }
        }
    }
    return res;
}

bool Scene::Visibility(glm::vec3 a, glm::vec3 b) __restrict__ const {
    Ray r(a, b, epsilon * 20.0f);
    return !FindIntersectKd(r).triangle;
}
bool Scene::VisibilityWithThinglass(glm::vec3 a, glm::vec3 b, const std::set<const Material *>& thinglass, std::vector<std::pair<const Triangle*, float>>& out) __restrict__ const {
    Ray r(a, b, epsilon * 20.0f);
    auto i = FindIntersectKdOtherThanWithThinglass(r,nullptr,thinglass);
    if(i.triangle != nullptr) return false;
    out = i.thinglass;
    return true;
}


void Scene::AddPointLight(Light l){
    pointlights.push_back(l);
}
Light Scene::GetRandomLight(Random& rnd) const{
    float total_power = total_point_power + total_areal_power;
    if(total_power <= 0.0f){
        // Sigh. Return just anything for compatibility.
        return Light();
    }
    float q = rnd.GetFloat(0.0f, total_power);
    if(q < total_point_power){
        // Choose pointlight
        for(unsigned int i = 0; i < pointlights.size(); i++){
            q -= pointlights[i].intensity * 4.0f * glm::pi<float>();
            if(q <= 0.0f){
                Light res = pointlights[i];
                // TODO: Fix relative light intensities, as we area importance sampling.
                // res.intensity = 1.0f;
                return res;
            }
        }
        out::cout(4) << "Internal error: GetRandomLight out of bounds for point lights." << std::endl;
        // Sigh. Return just anything for compatibility.
        return Light();
    }else{
        // Choose areal light
        q = rnd.GetFloat(0.0f, total_areal_power);

        for(unsigned int i = 0; i < areal_lights.size(); i++){
            q -= areal_lights[i].first;
            if(q <= 0.0f){
                const ArealLight& al = areal_lights[i].second;
                // Choose a random triangle.
                return al.GetRandomLight(rnd,*this);
            }
        }
        out::cout(4) << "Internal error: GetRandomLight out of bounds for areal lights." << std::endl;
        // Sigh. Return just anything for compatibility.
        return Light();
    }
}

Light Scene::ArealLight::GetRandomLight(Random& rnd, const Scene& parent) const{
    float p = rnd.GetFloat(0.0f, total_area);
    for(unsigned int j = 0; j < triangles_with_areas.size(); j++){
        p -= triangles_with_areas[j].first;
        if(p <= 0.0f){
            const Triangle& t = parent.triangles[triangles_with_areas[j].second];
            glm::vec3 p = t.GetRandomPoint(rnd);
            Light res;
            res.type = Light::HEMISPHERE;
            res.pos = p;
            res.color = emission;
            res.intensity = 1.0f;
            // TODO: interpolated normal at p
            res.normal = t.GetNormalA();
            return res;
        }
    }
    out::cout(4) << "Internal error: GetRandomLight out of bounds for triangle areas." << std::endl;
    // Sigh. Return just anything for compatibility.
    return Light();
}
