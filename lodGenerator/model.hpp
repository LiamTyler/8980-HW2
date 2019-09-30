#pragma once

#include "mesh.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace PG
{

class Material;

struct ModelCreateInfo
{
    std::string filename    = "";
    std::string materialDir = "";
};

class Model
{
public:
    Model() = default;

    bool Serialize( std::ofstream& outFile ) const;
    bool Deserialize( char*& buffer );
    
    bool LoadFromObj( ModelCreateInfo* createInfo );
    void Optimize();

    std::string name;
    AABB aabb;
    // Gfx::Buffer vertexBuffer;
    // Gfx::Buffer indexBuffer;
    std::vector< Mesh > meshes;
    std::vector< std::shared_ptr< Material > > materials;
};

} // namespace PG
