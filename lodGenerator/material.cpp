#include "material.hpp"
#include "serialize.hpp"

bool Material::Serialize( std::ofstream& out ) const
{
    serialize::Write( out, name );
    serialize::Write( out, Ka );
    serialize::Write( out, Kd );
    serialize::Write( out, Ks );
    serialize::Write( out, Ke );
    serialize::Write( out, Ns );
    serialize::Write( out, map_Kd );

    return !out.fail();
}

bool Material::Deserialize( char*& buffer )
{
    serialize::Read( buffer, name );
    serialize::Read( buffer, Ka );
    serialize::Read( buffer, Kd );
    serialize::Read( buffer, Ks );
    serialize::Read( buffer, Ke );
    serialize::Read( buffer, Ns );
    serialize::Read( buffer, map_Kd );

    return true;
}

/*
bool Material::LoadMtlFile( std::vector< Material >& materials, const std::string& fname )
{
    std::ifstream file( fname );
    if ( !file )
    {
        LOG_ERR( "Could not open mtl file: ", fname );
        return false;
    }

    materials.clear();
    Material* mat = nullptr;

    std::string line;
    std::string first;
    while ( std::getline( file, line ) )
    {
        std::istringstream ss( line );
        ss >> first;
        if ( first == "#" )
        {
            continue;
        }
        else if ( first == "newmtl" )
        {
            std::string name;
            ss >> name;
            materials.emplace_back();
            mat = &materials[materials.size() - 1];
            mat->name = name;
        }
        else if ( first == "Ns" )
        {
            ss >> mat->Ns;
        }
        else if ( first == "Ka" )
        {
            ss >> mat->Ka;
        }
        else if ( first == "Kd" )
        {
            ss >> mat->Kd;
        }
        else if ( first == "Ks" )
        {
            ss >> mat->Ks;
        }
        else if ( first == "Ke" )
        {
            ss >> mat->Ke;
        }
        else if ( first == "map_Kd" )
        {
            std::string texName;
            ss >> texName;
            mat->map_Kd = ResourceManager::Get< Image >( texName );
            if ( !mat->map_Kd )
            {
                LOG_ERR("Failed to load map_Kd image '", texName, "' in mtl file '", fname, "'");
                return false;
            }
        }
    }

    return true;
}
*/
