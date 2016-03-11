#include "primitives.hpp"
#include "scene.hpp"

const Material& Triangle::GetMaterial() const { return parent_scene->materials[mat]; }
const glm::vec3 Triangle::GetVertexA()  const { return parent_scene->vertices[va];  }
const glm::vec3 Triangle::GetVertexB()  const { return parent_scene->vertices[vb];  }
const glm::vec3 Triangle::GetVertexC()  const { return parent_scene->vertices[vc];  }
const glm::vec3 Triangle::GetNormalA()  const { return parent_scene->normals[va];  }
const glm::vec3 Triangle::GetNormalB()  const { return parent_scene->normals[vb];  }
const glm::vec3 Triangle::GetNormalC()  const { return parent_scene->normals[vc];  }
