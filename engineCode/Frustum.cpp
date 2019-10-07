#include "Frustum.h"
#include <iostream>

void Frustum::UpdatePlanesTrig( float fov, float nearPlane, float farPlane, float aspect,
        const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right )
{
    glm::vec3 ntl, ntr, nbl, nbr, ftl, ftr, fbr, fbl;
    float nearHeight, nearWidth, farHeight, farWidth;
    float angle = 0.5f * fov;

    nearHeight = nearPlane * tanf( angle );
    farHeight  = farPlane * tanf( angle );
    nearWidth  = aspect * nearHeight;
    farWidth   = aspect * farHeight;

    glm::vec3 nc = pos + nearPlane * forward;
    glm::vec3 fc = pos + farPlane * forward;

    glm::vec3 Y = up;
    glm::vec3 X = right;
    ntl         = nc + Y * nearHeight - X * nearWidth;
    ntr         = nc + Y * nearHeight + X * nearWidth;
    nbl         = nc - Y * nearHeight - X * nearWidth;
    nbr         = nc - Y * nearHeight + X * nearWidth;
    ftl         = fc + Y * farHeight - X * farWidth;
    ftr         = fc + Y * farHeight + X * farWidth;
    fbl         = fc - Y * farHeight - X * farWidth;
    fbr         = fc - Y * farHeight + X * farWidth;

    SetPlane( 0, nbl, fbl, ntl ); // left
    SetPlane( 1, nbr, ntr, fbr ); // right
    SetPlane( 2, ntr, ntl, ftl ); // top
    SetPlane( 3, nbl, nbr, fbr ); // bottom
    SetPlane( 4, ntl, ntr, nbr ); // near
    SetPlane( 5, ftr, ftl, fbl ); // far

    camDir = forward;
    camPos = pos;
}

void Frustum::SetPlane( int i, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3 )
{
    glm::vec3 p12 = p2 - p1;
    glm::vec3 p13 = p3 - p1;

    glm::vec3 n = glm::normalize( glm::cross( p12, p13 ) );
    float d     = glm::dot( n, -p1 );
    planes[i]   = glm::vec4( n, d );
}

// https://www.braynzarsoft.net/viewtutorial/q16390-34-aabb-cpu-side-frustum-culling
// https://stackoverflow.com/questions/12836967/extracting-view-frustum-planes-hartmann-gribbs-method
void Frustum::UpdatePlanesExtract( const glm::mat4& VP )
{
    // left plane
    // 4th col + 1st col
    planes[0].x = VP[0][3] + VP[0][0];
    planes[0].y = VP[1][3] + VP[1][0];
    planes[0].z = VP[2][3] + VP[2][0];
    planes[0].w = VP[3][3] + VP[3][0];

    // right plane
    // 4th col - 1st col
    planes[1].x = VP[0][3] - VP[0][0];
    planes[1].y = VP[1][3] - VP[1][0];
    planes[1].z = VP[2][3] - VP[2][0];
    planes[1].w = VP[3][3] - VP[3][0];

    // top plane
    // 4th col - 2nd col
    planes[2].x = VP[0][3] - VP[0][1];
    planes[2].y = VP[1][3] - VP[1][1];
    planes[2].z = VP[2][3] - VP[2][1];
    planes[2].w = VP[3][3] - VP[3][1];

    // bottom plane
    // 4th col + 2nd col
    planes[3].x = VP[0][3] + VP[0][1];
    planes[3].y = VP[1][3] + VP[1][1];
    planes[3].z = VP[2][3] + VP[2][1];
    planes[3].w = VP[3][3] + VP[3][1];

    // near plane
    // 4th col + 3rd col 
    planes[4].x = VP[0][3] + VP[0][2];
    planes[4].y = VP[1][3] + VP[1][2];
    planes[4].z = VP[2][3] + VP[2][2];
    planes[4].w = VP[3][3] + VP[3][2];

    // far plane
    // 4th col - 3rd col
    planes[5].x = VP[0][3] - VP[0][2];
    planes[5].y = VP[1][3] - VP[1][2];
    planes[5].z = VP[2][3] - VP[2][2];
    planes[5].w = VP[3][3] - VP[3][2];

    // planes arent normalized yet
    for ( int i = 0; i < 6; ++i )
    {
        glm::vec4& p = planes[i];
        float L = std::sqrt( p.x * p.x + p.y * p.y + p.z * p.z );
        p.x /= L;
        p.y /= L;
        p.z /= L;
        p.w /= L;
    }
}

void Frustum::Print() const
{
    std::cout << "Frustum planes:" << std::endl;
    for ( int i = 0; i < 6; ++i )
    {
        std::cout << "\tplane[" << i << "] = " << planes[i] << std::endl;
    }
}

bool Frustum::AABBIntersect( const glm::vec3& min, const glm::vec3& max ) const
{
    for ( int i = 0; i < 6; ++i )
    {
        glm::vec3 P = min;
        if ( planes[i].x >= 0 )
            P.x = max.x;
        if ( planes[i].y >= 0 )
            P.y = max.y;
        if ( planes[i].z >= 0 )
            P.z = max.z;

        if ( P.x * planes[i].x + P.y * planes[i].y + P.z * planes[i].z + planes[i].w < 0 )
        {
            return false;
        }
    }

    return true;
}
