#pragma once

#include "glm/glm.hpp"
#include <vector>

namespace PG
{

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

class Vertex
{
public:
    Vertex() : vertex( glm::vec3( 0 ) ), normal( glm::vec3( 0 ) ), uv( glm::vec3( 0 ) )
    {
    }

    Vertex( const glm::vec3& vert, const glm::vec3& norm, const glm::vec2& tex ) :
      vertex( vert ), normal( norm ), uv( tex )
    {
    }

    bool operator==( const Vertex& other ) const
    {
        return vertex == other.vertex && normal == other.normal && uv == other.uv;
    }
    glm::vec3 vertex;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct LODInfo
{
    size_t   startIndex = 0;
    uint32_t numIndices = 0;
    std::vector< uint32_t > indices;
};

class Mesh
{
public:
    Mesh()  = default;

    void Optimize();

    AABB aabb;
    std::vector< Vertex > vertices;
    std::vector< uint32_t > indices;
    uint32_t numVertices = 0;
    uint32_t numIndices  = 0;
    size_t   startIndex  = 0;
    std::vector< LODInfo > lods;
};

} // namespace PG
