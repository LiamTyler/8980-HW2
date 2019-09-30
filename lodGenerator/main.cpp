#include "model.hpp"
#include <fstream>

using namespace std;
using namespace PG;

int main( int argc, char** argv )
{
    if ( argc < 2 )
    {
        cout << "Usage: ./lodGenerator path_to_obj [material_directory]" << endl;
        return 0;
    }

    ModelCreateInfo info;
    info.filename = argv[1];
    if ( argc > 2 )
    {
        info.materialDir = argv[2];
    }

    Model model;
    if ( !model.LoadFromObj( &info ) )
    {
        cout << "Failed to load obj '" << info.filename << "'" << endl;
        return 1;
    }

    std::ofstream out( info.filename + ".ffi" );
    if ( !model.Serialize( out ) )
    {
        cout << "Failed to serialize obj '" << info.filename << "'" << endl;
        return 1;
    }

    return 0;
}
