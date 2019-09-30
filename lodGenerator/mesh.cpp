#include "mesh.hpp"
#include "meshoptimizer/src/meshoptimizer.h"

void Mesh::Optimize()
{
    // const size_t kCacheSize = 16;
    // meshopt_VertexCacheStatistics vcs = meshopt_analyzeVertexCache( &mesh.indices[0],
    // mesh.indices.size(), mesh.vertices.size(), kCacheSize, 0, 0);
    // meshopt_OverdrawStatistics os = meshopt_analyzeOverdraw(&mesh.indices[0],
    // mesh.indices.size(), &vertices[0].vertex.x, mesh.vertices.size(), sizeof(Vertex));
    // meshopt_VertexFetchStatistics vfs = meshopt_analyzeVertexFetch(&mesh.indices[0],
    // mesh.indices.size(), mesh.vertices.size(), sizeof(Vertex) );
    // LOG( "Before:" );
    // LOG( "ACMR: ", vcs.acmr, ", ATVR: ", vcs.atvr, ", avg overdraw: ", os.overdraw, " avg # fetched: ", vfs.overfetch);
    
    size_t index_count = indices.size();
    std::vector< uint32_t > remap( index_count );
    size_t vertex_count = meshopt_generateVertexRemap( &remap[0], NULL, index_count,
            &vertices[0], index_count, sizeof( Vertex ) );
    meshopt_remapIndexBuffer( &indices[0], NULL, index_count, &remap[0] );
    std::vector< Vertex > opt_vertices( vertex_count );
    meshopt_remapVertexBuffer( &opt_vertices[0], &vertices[0], index_count, sizeof( Vertex ), &remap[0] );

    // vertex cache optimization should go first as it provides starting order for overdraw
    meshopt_optimizeVertexCache( &indices[0], &indices[0], indices.size(), opt_vertices.size() );

    // reorder indices for overdraw, balancing overdraw and vertex cache efficiency
    const float kThreshold = 1.01f; // allow up to 1% worse ACMR to get more reordering opportunities for overdraw
    meshopt_optimizeOverdraw( &indices[0], &indices[0], indices.size(), &opt_vertices[0].vertex.x,
                              opt_vertices.size(), sizeof( Vertex ), kThreshold );

    // vertex fetch optimization should go last as it depends on the final index order
    meshopt_optimizeVertexFetch( &opt_vertices[0].vertex.x, &indices[0], indices.size(),
                                 &opt_vertices[0].vertex.x, opt_vertices.size(), sizeof( Vertex ) );

    // vcs = meshopt_analyzeVertexCache(&indices[0], indices.size(), vertices.size(), kCacheSize, 0,
    // 0); os = meshopt_analyzeOverdraw(&indices[0], indices.size(), &vertices[0].vertex.x,
    // vertices.size(), sizeof(Vertex)); vfs = meshopt_analyzeVertexFetch(&indices[0],
    // indices.size(), vertices.size(), sizeof(Vertex)); LOG("After:"); LOG("ACMR: ", vcs.acmr, ",
    // ATVR: ", vcs.atvr, ", avg overdraw: ", os.overdraw, " avg # fetched: ", vfs.overfetch);
    vertices    = opt_vertices;
    numVertices = (uint32_t) vertices.size();
    numIndices  = (uint32_t) indices.size();

    int numLODs = 2;
    lods.resize( numLODs + 1 );
    std::vector< uint32_t > combinedIndices;
    combinedIndices.reserve( indices.size() );
    for ( int i = 0; i < 2; ++i )
    {
        float threshold = 0.1f + .4f*i;
        size_t target_index_count = size_t( index_count * threshold );

        lods[i].indices.resize( target_index_count );
        lods[i].indices.resize( meshopt_simplifySloppy( lods[i].indices.data(), indices.data(), index_count, &vertices[0].vertex.x,
                                                        vertex_count, sizeof( Vertex ), target_index_count) );
        // float target_error = 1e-2f;
        // lods[i].indices.resize( meshopt_simplify( lods[i].indices.data(), indices.data(), index_count, &vertices[0].vertex.x,
        //                                                 vertex_count, sizeof( Vertex ), target_index_count, target_error ) );
        lods[i].numIndices = lods[i].indices.size();
        lods[i].startIndex = combinedIndices.size();

        for ( const auto& index : lods[i].indices )
        {
            combinedIndices.push_back( index );
        }
    }

    startIndex = combinedIndices.size();
    for ( const auto& index : indices )
    {
        combinedIndices.push_back( index );
    }

    lods[numLODs].numIndices = numIndices;
    lods[numLODs].startIndex = startIndex;

    indices = combinedIndices;
}
