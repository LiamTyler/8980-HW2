#include "model.hpp"
#include "material.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "serialize.hpp"
#include <limits>
#include <iostream>

namespace PG
{

bool Model::Serialize( std::ofstream& out ) const
{
    // serialize::Write( out, name );
    serialize::Write( out, aabb.min );
    serialize::Write( out, aabb.max );

    uint32_t numMeshes = static_cast< uint32_t >( meshes.size() );
    serialize::Write( out, numMeshes );
    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        if ( !materials[i]->Serialize( out ) )
        {
            return false;
        }
    }

    size_t totalVertices = 0;
    size_t totalIndices  = 0;
    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        serialize::Write( out, meshes[i].aabb.min );
        serialize::Write( out, meshes[i].aabb.max );
        size_t numLODs = meshes[i].lods.size();
        serialize::Write( out, numLODs );
        for ( size_t lod = 0; lod < numLODs; ++lod )
        {
            size_t indexPos = totalIndices + meshes[i].lods[lod].startIndex;
            serialize::Write( out, indexPos );
            serialize::Write( out, meshes[i].lods[lod].numIndices );
        }

        size_t indexPos = totalIndices + meshes[i].startIndex;
        serialize::Write( out, indexPos );
        serialize::Write( out, meshes[i].numIndices );
        serialize::Write( out, meshes[i].numVertices );
        totalVertices += meshes[i].vertices.size();
        totalIndices  += meshes[i].indices.size();
    }

    serialize::Write( out, totalVertices );
    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        serialize::Write( out, (char*) meshes[i].vertices.data(), sizeof( Vertex ) * meshes[i].numVertices );
    }
    serialize::Write( out, totalIndices );
    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        size_t numIndices = meshes[i].indices.size();
        serialize::Write( out, (char*) meshes[i].indices.data(), sizeof( uint32_t ) * numIndices );
    }


    return !out.fail();
}

bool Model::Deserialize( char*& buffer )
{
    // serialize::Read( buffer, name );
    serialize::Read( buffer, aabb.min );
    serialize::Read( buffer, aabb.max );
    uint32_t numMeshes;
    serialize::Read( buffer, numMeshes );

    meshes.resize( numMeshes );
    materials.resize( numMeshes );
    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        auto mat = std::make_shared< Material >();
        if ( !mat->Deserialize( buffer ) )
        {
            return false;
        }
        materials[i] = mat;
    }

    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        serialize::Read( buffer, meshes[i].aabb.min );
        serialize::Read( buffer, meshes[i].aabb.max );
        size_t numLODs;
        serialize::Read( buffer, numLODs );
        meshes[i].lods.resize( numLODs );
        for ( size_t lod = 0; lod < numLODs; ++lod )
        {
            serialize::Read( buffer, meshes[i].lods[lod].startIndex );
            serialize::Read( buffer, meshes[i].lods[lod].numIndices );
            //std::cout << "Mesh[" << i << "].lod[" << lod << "].startIndex = " << meshes[i].lods[lod].startIndex << std::endl;
            //std::cout << "Mesh[" << i << "].lod[" << lod << "].numIndices = " << meshes[i].lods[lod].numIndices << std::endl;
        }

        serialize::Read( buffer, meshes[i].startIndex );
        serialize::Read( buffer, meshes[i].numIndices );
        serialize::Read( buffer, meshes[i].numVertices );
    }

    size_t totalVertices;
    size_t totalIndices;
    serialize::Read( buffer, totalVertices );

    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        meshes[i].vertices.resize( meshes[i].numVertices );
        serialize::Read( buffer, (char*)meshes[i].vertices.data(), meshes[i].numVertices * sizeof( PG::Vertex ) );
    }

    serialize::Read( buffer, totalIndices );
    for ( uint32_t i = 0; i < numMeshes; ++i )
    {
        for ( size_t lod = 0; lod < meshes[i].lods.size(); ++lod )
        {
            meshes[i].lods[lod].indices.resize( meshes[i].lods[lod].numIndices );
            serialize::Read( buffer, (char*)meshes[i].lods[lod].indices.data(), meshes[i].lods[lod].numIndices * sizeof( uint32_t ) );
        }
    }

    return true;
}

bool Model::LoadFromObj( ModelCreateInfo* createInfo )
{
    tinyobj::attrib_t attrib;
    std::vector< tinyobj::shape_t > shapes;
    std::vector< tinyobj::material_t > tiny_materials;
    std::string err;
    std::string warn;
    bool ret = tinyobj::LoadObj( &attrib, &shapes, &tiny_materials, &warn, &err, createInfo->filename.c_str(), createInfo->materialDir.c_str(), true );

    if ( !err.empty() )
    {
        std::cout << "TinyObj loader warning: " << err << std::endl;
    }

    if ( !ret )
    {
        std::cout << "Failed to load the obj file: " << createInfo->filename << std::endl;
        return false;
    }

    meshes.clear();
    materials.clear();

    aabb.min = glm::vec3( std::numeric_limits< float >::max() );
    aabb.max = glm::vec3( -std::numeric_limits< float >::max() );

    for ( int currentMaterialID = -1; currentMaterialID < (int) tiny_materials.size();
          ++currentMaterialID )
    {
        auto currentMaterial = std::make_shared< Material >();
        if ( currentMaterialID == -1 )
        {
            currentMaterial->name = "default";
        }
        else
        {
            tinyobj::material_t& mat = tiny_materials[currentMaterialID];
            currentMaterial->name    = mat.name;
            currentMaterial->Ka      = glm::vec3( mat.ambient[0], mat.ambient[1], mat.ambient[2] );
            currentMaterial->Kd      = glm::vec3( mat.diffuse[0], mat.diffuse[1], mat.diffuse[2] );
            currentMaterial->Ks      = glm::vec3( mat.specular[0], mat.specular[1], mat.specular[2] );
            currentMaterial->Ke      = glm::vec3( mat.emission[0], mat.emission[1], mat.emission[2] );
            currentMaterial->Ns      = mat.shininess;
            currentMaterial->map_Kd = mat.diffuse_texname;
        }

        bool missingNormals = true;
        Mesh currentMesh;
        glm::vec3 min = glm::vec3( std::numeric_limits< float >::max() );
        glm::vec3 max = glm::vec3( -std::numeric_limits< float >::max() );
        for ( const auto& shape : shapes )
        {
            // Loop over faces(polygon)
            for ( size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++ )
            {
                if ( shape.mesh.material_ids[f] == currentMaterialID )
                {
                    // Loop over vertices in the face. Each face should have 3 vertices from the
                    // LoadObj triangulation
                    for ( size_t v = 0; v < 3; v++ )
                    {
                        Vertex vertex;

                        tinyobj::index_t idx = shape.mesh.indices[3 * f + v];
                        tinyobj::real_t vx   = attrib.vertices[3 * idx.vertex_index + 0];
                        tinyobj::real_t vy   = attrib.vertices[3 * idx.vertex_index + 1];
                        tinyobj::real_t vz   = attrib.vertices[3 * idx.vertex_index + 2];

                        vertex.vertex = glm::vec3( vx, vy, vz );
                        min = glm::min( min, vertex.vertex );
                        max = glm::max( max, vertex.vertex );

                        if ( idx.normal_index != -1 )
                        {
                            missingNormals = false;
                            vertex.normal.x = attrib.normals[3 * idx.normal_index + 0];
                            vertex.normal.y = attrib.normals[3 * idx.normal_index + 1];
                            vertex.normal.z = attrib.normals[3 * idx.normal_index + 2];
                        }

                        if ( idx.texcoord_index != -1 )
                        {
                            vertex.uv.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                            vertex.uv.y = attrib.texcoords[2 * idx.texcoord_index + 1];
                        }

                        currentMesh.vertices.emplace_back( vertex );
                        currentMesh.indices.push_back( static_cast< uint32_t >( currentMesh.vertices.size() ) );
                    }
                }
            }
        }

        if ( currentMesh.vertices.size() )
        {
            if ( missingNormals )
            {
                auto& verts   = currentMesh.vertices;
                auto& indices = currentMesh.indices;

                for ( size_t i = 0; i < indices.size(); i += 3 )
                {
                    glm::vec3 v1 = verts[indices[i + 0]].vertex;
                    glm::vec3 v2 = verts[indices[i + 1]].vertex;
                    glm::vec3 v3 = verts[indices[i + 2]].vertex;
                    glm::vec3 n  = glm::cross( v2 - v1, v3 - v1 );
                    verts[indices[i + 0]].normal += n;
                    verts[indices[i + 1]].normal += n;
                    verts[indices[i + 2]].normal += n;
                }

                for ( auto& vertex : verts )
                {
                    vertex.normal = glm::normalize( vertex.normal );
                }
            }

            currentMesh.aabb = { min, max };
            aabb.min         = glm::min( min, aabb.min );
            aabb.max         = glm::max( max, aabb.max );

            currentMesh.Optimize();

            materials.push_back( currentMaterial );
            meshes.push_back( std::move( currentMesh ) );
        }
    }

    return true;
}

void Model::Optimize()
{
    for ( Mesh& mesh : meshes )
    {
        mesh.Optimize();
    }
}

} // namespace PG
