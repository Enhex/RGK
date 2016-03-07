#include "scene.hpp"

#include <iostream>

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
}

void Scene::LoadScene(const aiScene* scene) const{
    // Load materials
    for(unsigned int i = 0; i < scene->mNumMaterials; i++){
        const aiMaterial* mat = scene->mMaterials[i];

        Material m;
        m.parent_scene = this;
        mat->Get(AI_MATKEY_COLOR_DIFFUSE,m.diffuse);

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

    vertices = new aiVector3D[vertices_buffer.size()];
    triangles = new Triangle[triangles_buffer.size()];
    materials = new Material[materials_buffer.size()];

    n_vertices = vertices_buffer.size();
    n_triangles = triangles_buffer.size();
    n_materials = materials_buffer.size();

    // TODO: memcpy
    for(unsigned int i = 0; i < n_vertices; i++)
        vertices[i] = vertices_buffer[i];
    for(unsigned int i = 0; i < n_triangles; i++)
        triangles[i] = triangles_buffer[i];
    for(unsigned int i = 0; i < n_materials; i++)
        materials[i] = materials_buffer[i];

    std::cout << "Commited " << n_vertices << " vertices and " << n_triangles <<
        " triangles with " << n_materials << " materials to the scene." << std::endl;

    vertices_buffer.clear();
    triangles_buffer.clear();
    materials_buffer.clear();
}

void Scene::Dump() const{
    for(unsigned int t = 0; t < n_triangles; t++){
        const Triangle& tr = triangles[t];
        const aiVector3D va = vertices[tr.va];
        const aiVector3D vb = vertices[tr.vb];
        const aiVector3D vc = vertices[tr.vc];
        const aiColor3D& color = materials[tr.mat].diffuse;
        std::cout << va.x << " " << va.y << " " << va.z << " | ";
        std::cout << vb.x << " " << vb.y << " " << vb.z << " | ";
        std::cout << vc.x << " " << vc.y << " " << vc.z << " [" ;
        std::cout << color.r << " " << color.g << " " << color.b << "]\n" ;
    }
}
