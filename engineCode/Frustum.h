#pragma once

#include "glm/glm.hpp"
#include <iostream>

inline std::ostream& operator<<( std::ostream& out, const glm::vec3& v )
{
    return out << v.x << " " << v.y << " " << v.z;
}

inline std::ostream& operator<<( std::ostream& out, const glm::vec4& v )
{
    return out << v.x << " " << v.y << " " << v.z << " " << v.w;
}

inline std::ostream& operator<<( std::ostream& out, const glm::mat4& mm )
{
    auto m = glm::transpose(mm);
    return out << m[0] << "\n" << m[1] << "\n" << m[2] << "\n" << m[3];
}

class Frustum
{
public:
    void UpdatePlanesTrig( float fov, float nearPlane, float farPlane, float aspect,
                           const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right );
    void SetPlane( int i, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3 );
    void UpdatePlanesExtract( const glm::mat4& VP );
    void Print() const;
    bool AABBIntersect( const glm::vec3& min, const glm::vec3& max ) const;

    glm::vec4 planes[6];
    glm::vec3 camPos;
    glm::vec3 camDir;
};
