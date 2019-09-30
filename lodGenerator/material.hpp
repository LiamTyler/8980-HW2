#pragma once

#include "glm/glm.hpp"
#include <string>
#include <fstream>
#include <vector>

namespace PG
{

class Material
{
public:
    Material() = default;

    bool Serialize( std::ofstream& outFile ) const;
    bool Deserialize( char*& buffer );
    // static bool LoadMtlFile( std::vector< Material >& materials, const std::string& fname );

    std::string name;
    glm::vec3 Ka;
    glm::vec3 Kd;
    glm::vec3 Ks;
    glm::vec3 Ke;
    float Ns;
    std::string map_Kd;
};

} // namespace PG
