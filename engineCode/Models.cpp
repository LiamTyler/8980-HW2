#include "Models.h"

#include "GPU-Includes.h"

// #define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
#include <external/tiny_obj_loader.h>

#include <external/loguru.hpp>
#include <float.h>

std::string warn;
std::string err;

#include <fstream>
#include "serialize.hpp"
#include "model.hpp"
#include "material.hpp"
#include "MemoryMapped.h"
#include <unordered_map>

using std::string;
using std::ifstream;
using std::copy;

Model models[4000000];
int numModels = 0;

void resetModels(){
    //TODO: It is probably best practices to unload the old models from where they are on the GPU
    //TODO: It is best practices to delete the old models explicitly
    numModels = 0;
}

void loadAllModelsTo1VBO(GLuint vbo, GLuint ibo )
{
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    size_t vextexCount = 0;
    size_t indexCount  = 0;
    for(int i = 0; i < numModels; i++) {
        models[i].startVertex = vextexCount;
        vextexCount += models[i].numVerts;
        for ( size_t lod = 0; lod < models[i].lods.size(); ++lod )
        {
            indexCount += models[i].lods[lod].numIndices;
        }
    }
    int totalVertexCount = vextexCount;

    std::vector< uint32_t > indices;
    indices.reserve( indexCount );
    uint32_t currIndexOffset = 0;
    float* allModelData = new float[vextexCount*8];
    copy( models[0].modelData, models[0].modelData + models[0].numVerts*8, allModelData );
    for( int i = 0; i < numModels; i++ )
    {
        copy(models[i].modelData,models[i].modelData + models[i].numVerts*8,allModelData + models[i].startVertex*8);

        // std::cout << models[i].lods.size() << " " << models[i].numVerts << std::endl;
        for ( size_t lod = 0; lod < models[i].lods.size(); ++lod )
        {
            models[i].lods[lod].startIndex = indices.size();
            for ( size_t index = 0; index < models[i].lods[lod].numIndices; ++index )
            {
                indices.push_back( currIndexOffset + models[i].lods[lod].indices[index] );
            }
            // currIndexOffset += (uint32_t) models[i].lods[lod].numIndices;
        }
        currIndexOffset += models[i].numVerts;
    }
    glBufferData(GL_ARRAY_BUFFER,totalVertexCount*8*sizeof(float), allModelData,GL_STATIC_DRAW);
    delete allModelData;

    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW );
}

int addModel(string modelName){
    int curModelID = numModels;
    numModels++;
    models[curModelID] = Model(); //Create new model with default values
    models[curModelID].ID = curModelID;
    models[curModelID].name = modelName;
    return curModelID;
}

void addChild(string childName,int curModelID){
    static std::unordered_map<string, int> childMap;
    int childModel = -1;
    auto it = childMap.find( childName );
    if ( it != childMap.end() )
    {
        childModel = it->second;
    }
    else
    {
        for(int i = 0; i < numModels; i++){
            if(models[i].name == childName){
                childModel = i;
                continue;
            }
        }
        childMap[childName] = childModel;
    }
    CHECK_F(childModel >= 0,"No model of name '%s' found to be added as a child model!",childName.c_str());

    LOG_F(1,"Adding child %s",childName.c_str());

    models[curModelID].childModel.push_back(&models[childModel]);
    models[curModelID].numChildren++;
}

void loadModel(string fileName){
    LOG_SCOPE_FUNCTION(INFO);

    FILE *fp;
    long length;
    char rawline[1024]; //Assumes no line is longer than 1024 characters!

    // open the file containing the scene description
    fp = fopen(fileName.c_str(),"r");

    LOG_F(INFO,"Loading Model File: %s",fileName.c_str());
    // check for errors in opening the file
    CHECK_NOTNULL_F(fp,"Can't open model file '%s'",fileName.c_str());

    fseek(fp,0,SEEK_END); // move position indicator to the end of the file;
    length = ftell(fp);  // return the value of the current position
    LOG_F(INFO,"File '%s' is %ld bytes long.",fileName.c_str(),length);
    fseek(fp,0,SEEK_SET);  // move position indicator to the start of the file

    string modelName = "";
    string modelDir = "./";
    int curModelID = 0;
    //Loop through reading each line
    while(fgets(rawline,1024,fp)) { //Assumes no line is longer than 1024 characters!
        string line = string(rawline);
        if(rawline[0] == '#'){
            LOG_F(2,"Skipping comment: %s",rawline);
            continue;
        }

        char command[100];
        int fieldsRead = sscanf(rawline,"%s ",command); //Read first word in the line (i.e., the command type)
        string commandStr = command;

        if(fieldsRead < 1){ //No command read = Blank line
            continue;
        }

        if(commandStr.substr(0,1) == "["){ // "[" indicates new model
            int closeBrackets = line.find("]");
            CHECK_F(closeBrackets >= 0,"ERROR: Model name opened with [ but not closed with ]");
            modelName = line.substr(1,closeBrackets-1);
            curModelID = addModel(modelName);
            LOG_F(INFO,"Creating PreFab Model with name: %s",modelName.c_str());
        } else if(commandStr == "identity"){
            models[curModelID].transform = glm::mat4();
            LOG_F(1,"Reseting to Indentity Transform");
        } else if(commandStr == "scale"){
            float scaleFactor;
            sscanf(rawline,"scale %f",&scaleFactor);
            models[curModelID].transform = glm::scale(models[curModelID].transform,scaleFactor*glm::vec3(1,1,1));
            LOG_F(1,"Scaling by %f",scaleFactor);
        } else if(commandStr == "scalexyz"){
            //compute new glm matrix
            float sx,sy,sz;
            sscanf(rawline,"scalexyz %f %f %f",&sx,&sy,&sz);
            models[curModelID].transform = glm::scale(models[curModelID].transform,glm::vec3(sx,sy,sz));
            LOG_F(1,"Scaling by %f %f %f",sx,sy,sz);
        } else if(commandStr == "rotate"){
            //compute new glm matrix`
            float angle,rx,ry,rz;
            sscanf(rawline,"rotate %f %f %f %f",&angle,&rx,&ry,&rz);
            models[curModelID].transform = glm::rotate(models[curModelID].transform,(float)(angle*(M_PI/180.f)),glm::vec3(rx,ry,rz));
            LOG_F(1,"Rotating by %f around axis %f %f %f",angle,rx,ry,rz);
        } else if(commandStr == "translate"){
            //compute new glm matrix
            float tx,ty,tz;
            sscanf(rawline,"translate %f %f %f",&tx,&ty,&tz);
            models[curModelID].transform = glm::translate(models[curModelID].transform,glm::vec3(tx,ty,tz));
            LOG_F(1,"Transling by (%f, %f, %f)",tx,ty,tz);
        } else if(commandStr == "textureWrap"){
            float uScale,vScale;
            sscanf(rawline,"textureWrap %f %f",&uScale,&vScale);
            models[curModelID].textureWrap = glm::vec2(uScale,vScale);
            LOG_F(1,"Wrapping texture by %fX in u and %fX in v",uScale,vScale);
        } else if(commandStr == "modelDir"){
            char dirName[1024];
            sscanf(rawline,"modelDir = %s",dirName);
            modelDir = dirName;
            LOG_F(1,"Setting model directory to: %s",modelDir.c_str());
        } else if(commandStr == "flatModel"){
            char flatDataFile[1024];
            sscanf(rawline,"flatModel = %s",flatDataFile);

            ifstream modelFile;
            LOG_F(1,"Loading flat model file %s as ID: %d",(modelDir + flatDataFile).c_str(),curModelID);
            modelFile.open(modelDir + flatDataFile);
            CHECK_F(!modelFile.fail(),"Failed to open flatmodel: %s",(modelDir + flatDataFile).c_str());
            int numLines = 0;
            modelFile >> numLines;
            models[curModelID].modelData = new float[numLines];
            for(int i = 0; i < numLines; i++){
                modelFile >> models[curModelID].modelData[i];
                //if (i%8 == 3 || i%8 == 4) models[curModelID].modelData[i] *= 2; //texture wrap factor
            }
            LOG_F(1,"Loaded %d lines",numLines);
            models[curModelID].numVerts = numLines/8;
			
			// Generate AABB
            AABB aabb = { glm::vec3( FLT_MAX ), glm::vec3( -FLT_MAX ) };
            models[curModelID].lods.resize( 1 );
            models[curModelID].lods[0].indices.resize( numLines/8 );
            for(int i = 0; i < models[curModelID].numVerts; ++i)
            {
                float vx = models[curModelID].modelData[8*i + 0];
                float vy = models[curModelID].modelData[8*i + 1];
                float vz = models[curModelID].modelData[8*i + 2];
                aabb.min = glm::min(aabb.min,glm::vec3(vx,vy,vz));
                aabb.max = glm::max(aabb.max,glm::vec3(vx,vy,vz));
                models[curModelID].lods[0].indices[i] = i;
            }
            // models[curModelID].numIndices = models[curModelID].lods[0].indices.size();
            models[curModelID].aabb = aabb;
            models[curModelID].lods[0].startIndex = 0;
            models[curModelID].lods[0].numIndices = models[curModelID].lods[0].indices.size();

            modelFile.close();
        }
        else if ( commandStr == "ffiModel" )
        {
            char rawname[1024];
            sscanf( rawline, "ffiModel = %s", rawname );
            std::string fname = modelDir + rawname;

            MemoryMapped memMappedFile;
            if ( !memMappedFile.open( fname, MemoryMapped::WholeFile, MemoryMapped::SequentialScan ) )
            {
                LOG_F( INFO, "Could not open fastfile: '%s'", fname.c_str() );
                return;
            }

            char* buffer = (char*) memMappedFile.getData();
            PG::Model model;
            model.Deserialize( buffer );
            memMappedFile.close();


            LOG_F( INFO,"num materials: %d", (int) model.materials.size() );
            for( size_t i = 0; i < model.materials.size(); i++ ){
                auto pgMat = model.materials[i];
                Material m;
                m.name      = pgMat->name;
                m.col       = pgMat->Kd;
                m.emissive  = pgMat->Ke;
                m.ior       = 1.5;
                m.roughness = 1/(1+0.01*pgMat->Ns );
                m.metallic  = 0;
                //m.metallic = objMaterials[i].metallic;
                float avgSpec = ( pgMat->Ks.x + pgMat->Ks.y + pgMat->Ks.z ) / 3.0;
                if(avgSpec > 0.8) m.reflectiveness = (avgSpec-0.8)/0.2;
                // std::cout << "Model: " << fname << std::endl;
                // std::cout << "Matname: " << m.name << std::endl;
                // std::cout << "IOR: " << m.ior << std::endl;
                // std::cout << "Roughness: " << m.roughness << std::endl;
                // std::cout << "Metallic: " << m.metallic << std::endl;
                // std::cout << "reflectiveness: " << m.reflectiveness << std::endl;
                // std::cout << "color: " << m.col.x << " " << m.col.y << " " << m.col.z << std::endl;
                // std::cout << "emissive: " << m.emissive.x << " " << m.emissive.y << " " << m.emissive.z << std::endl;
                // std::cout << std::endl;

                if( pgMat->map_Kd != "" )
                {
                    string textureName = modelDir + pgMat->map_Kd;
                    int foundTexture = -1;
                    for(int i = 0; i < numTextures; i++){
                        if(textures[i] == textureName){
                            foundTexture = i;
                            break;
                        }
                    }
                    if(foundTexture >= 0){
                        LOG_F(1,"Reusing existing texture: %s",textures[foundTexture].c_str());
                        m.textureID = foundTexture;
                    } else{
                        textures[numTextures] = textureName;
                        m.textureID = numTextures;
                        numTextures++;
                        LOG_F(1,"New texture named: %s",textureName.c_str());
                    }
                }

                materials[numMaterials] = m;
                LOG_F( INFO, "Creating Material ID %d with name '%s'",numMaterials,materials[numMaterials].name.c_str());
                numMaterials++;
            }

            int objChild = 0;
            string childName = fname + string("-Child-") + std::to_string( objChild );
            int childModelID = addModel(childName); //add Childs

            {
                string materialName = model.materials[0]->name;
                int materialID = -1;
                if(models[curModelID].materialID == -1) //Only use obj materials if the parent doesn't have it's own material
                    materialID = findMaterial(materialName.c_str());
                LOG_F(1,"Binding material: '%s' (Material ID %d) to Model %d",materialName.c_str(),materialID,childModelID);
                models[childModelID].materialID = materialID;
            }

            for ( size_t i = 0; i < model.meshes.size(); i++ )
            {
                const auto& mesh = model.meshes[i];

                models[childModelID].aabb = { mesh.aabb.min, mesh.aabb.max };
                //models[childModelID].indices = mesh.indices;
                //models[childModelID].numIndices = mesh.indices.size();
                models[childModelID].modelData = new float[mesh.numVertices * 8];
                memcpy( models[childModelID].modelData, mesh.vertices.data(), mesh.numVertices * sizeof( PG::Vertex ) );
                models[childModelID].numVerts = mesh.numVertices;
                LOG_F(1,"Loaded %d vertices",models[childModelID].numVerts);
                models[childModelID].lods.resize( mesh.lods.size() );
                for ( size_t lod = 0; lod < mesh.lods.size(); ++ lod )
                {
                    models[childModelID].lods[lod].indices    = mesh.lods[lod].indices;
                    models[childModelID].lods[lod].startIndex = mesh.lods[lod].startIndex;
                    models[childModelID].lods[lod].numIndices = mesh.lods[lod].numIndices;
                }
                
                addChild(childName,curModelID);

                // Start a new model for the next set of textures
                objChild++;
                childName = fname + string("-Child-") + std::to_string( objChild );
                LOG_F( INFO,"Loading obj child model %s as IDs: %d",(childName).c_str(),childModelID);
                childModelID = addModel(childName);

                //Bind the new material
                string materialName = model.materials[i]->name;
                int materialID = -1;
                if(models[curModelID].materialID == -1) //Only use obj materials if the parent doesn't have it's own material
                    materialID = findMaterial(materialName.c_str());
                LOG_F( INFO, "Binding material: '%s' (Material ID %d) to Model %d",materialName.c_str(),materialID,curModelID);
                models[childModelID].materialID = materialID;

            }
        }
        else if (commandStr == "objModel")
        {
            char objFile[1024];
            sscanf(rawline,"objModel = %s",objFile);

            LOG_F(1,"Loading obj model %s starting at ID: %d",(modelDir + objFile).c_str(),curModelID);
            tinyobj::attrib_t objAttrib;
            std::vector<tinyobj::shape_t> objShapes;
            std::vector<tinyobj::material_t> objMaterials;
            bool ret = tinyobj::LoadObj(&objAttrib,&objShapes,&objMaterials,&warn,&err,(modelDir + objFile).c_str(),modelDir.c_str());

            LOG_IF_F(WARNING,!warn.empty(),"Obj Loading Warning: %s",warn.c_str());
            LOG_IF_F(WARNING,!err.empty(),"Obj Loading ERROR: %s",err.c_str());
            CHECK_F(ret,"CRITICAL MODEL LOADING ERROR!");

            for(size_t i = 0; i < objMaterials.size(); i++){
                Material m;
                m.name = objFile+string("-")+objMaterials[i].name;
                memcpy((void*)& m.col,(void*)& objMaterials[i].diffuse[0],sizeof(float)*3); //copy all 3 colors (RGB)
                m.ior = objMaterials[i].ior;
                m.roughness = 1/(1+0.01*objMaterials[i].shininess);
                m.metallic = objMaterials[i].metallic;
                float avgSpec = (objMaterials[i].specular[0]+objMaterials[i].specular[1]+objMaterials[i].specular[2])/3.0;
                if(avgSpec > 0.8) m.reflectiveness = (avgSpec-0.8)/0.2;
                memcpy((void*)& m.emissive,(void*)& objMaterials[i].emission[0],sizeof(float)*3); //copy all 3 emissive colors (RGB)

                // std::cout << "Model: " << objFile << std::endl;
                // std::cout << "Matname: " << m.name << std::endl;
                // std::cout << "IOR: " << m.ior << std::endl;
                // std::cout << "Roughness: " << m.roughness << std::endl;
                // std::cout << "Metallic: " << m.metallic << std::endl;
                // std::cout << "reflectiveness: " << m.reflectiveness << std::endl;
                // std::cout << "color: " << m.col.x << " " << m.col.y << " " << m.col.z << std::endl;
                // std::cout << "emissive: " << m.emissive.x << " " << m.emissive.y << " " << m.emissive.z << std::endl;
                // std::cout << std::endl;

                if(objMaterials[i].diffuse_texname != ""){
                    string textureName = modelDir + objMaterials[i].diffuse_texname;
                    int foundTexture = -1;
                    for(int i = 0; i < numTextures; i++){
                        if(textures[i] == textureName){
                            foundTexture = i;
                            break;
                        }
                    }
                    if(foundTexture >= 0){
                        LOG_F(1,"Reusing existing texture: %s",textures[foundTexture].c_str());
                        m.textureID = foundTexture;
                    } else{
                        textures[numTextures] = textureName;
                        m.textureID = numTextures;
                        numTextures++;
                        LOG_F(1,"New texture named: %s",textureName.c_str());
                    }
                }

                materials[numMaterials] = m;
                LOG_F(1,"Creating Material ID %d with name '%s'",numMaterials,materials[numMaterials].name.c_str());
                numMaterials++;
            }

            int objChild = 0;
            string childName = objFile+string("-Child-")+std::to_string(objChild);
            int childModelID = addModel(childName); //add Childs
            LOG_F(1,"Loading obj child model %s as IDs: %d",(childName).c_str(),childModelID);


            if (childName == "Trex.obj-Child-0")

            {
                printf("debug\n");
            }

            std::vector< float > vertexData;
            std::vector< uint32_t > indices;
            AABB currentAABB = { glm::vec3( FLT_MAX ), glm::vec3( -FLT_MAX ) };
            // Loop over shapes
            for(size_t s = 0; s < objShapes.size(); s++) {
                int curMaterialID = -1;
                int lastMaterialID = -1;
                if(objMaterials.size() > 0 && objShapes[s].mesh.material_ids.size() > 0){
                    curMaterialID = lastMaterialID = objShapes[s].mesh.material_ids[0];
                    //if (curMaterialID == -1) curMaterialID = 0;

                    string materialName = objFile+string("-")+objMaterials[curMaterialID].name;
                    int materialID = -1;
                    if(models[curModelID].materialID == -1) //Only use obj materials if the parent doesn't have it's own material
                        materialID = findMaterial(materialName.c_str());
                    LOG_F(1,"Binding material: '%s' (Material ID %d) to Model %d",materialName.c_str(),materialID,childModelID);
                    models[childModelID].materialID = materialID;
                }
                // Loop over faces(polygon)
                size_t index_offset = 0;
                for(size_t f = 0; f < objShapes[s].mesh.num_face_vertices.size(); f++) {
                    size_t fv = objShapes[s].mesh.num_face_vertices[f];
                    assert(fv == 3); //tinyobj loader triangulates all faces by default

                    lastMaterialID = curMaterialID;
                    curMaterialID = objShapes[s].mesh.material_ids[f];

                    if( !vertexData.empty() && f == 0 || curMaterialID != lastMaterialID){
                        LOG_F(3,"CurMat: %d, LastMat: %d",curMaterialID,lastMaterialID);

                        //Copy vertex data read so far into the model
                        int numAttribs = vertexData.size();
                        models[childModelID].aabb = currentAABB;
                        models[childModelID].lods.resize( 1 );
                        models[childModelID].lods[0].indices    = indices;
                        models[childModelID].lods[0].numIndices = indices.size();
                        models[childModelID].lods[0].startIndex = 0;
                        models[childModelID].modelData = new float[numAttribs];
                        std::copy(vertexData.begin(),vertexData.end(),models[childModelID].modelData);
                        models[childModelID].numVerts = numAttribs/8;
                        LOG_F(1,"Loaded %d vertices",models[childModelID].numVerts);
                        addChild(childName,curModelID);

                        ///Start a new model for the next set of textures
                        objChild++;
                        childName = objFile+string("-Child-")+std::to_string(objChild);
                        LOG_F(1,"Loading obj child model %s as IDs: %d",(childName).c_str(),childModelID);
                        childModelID = addModel(childName);
                        vertexData.clear();
                        indices.clear();

                        //Bind the new material
                        string materialName = objFile+string("-")+objMaterials[curMaterialID].name;
                        int materialID = -1;
                        if(models[curModelID].materialID == -1) //Only use obj materials if the parent doesn't have it's own material
                            materialID = findMaterial(materialName.c_str());
                        LOG_F(1,"Binding material: '%s' (Material ID %d) to Model %d",materialName.c_str(),materialID,curModelID);
                        models[childModelID].materialID = materialID;
                    }

                    // Loop over vertices in the face.
                    for(size_t v = 0; v < fv; v++) {
                        // access to vertex
                        tinyobj::index_t idx = objShapes[s].mesh.indices[index_offset + v];
                        tinyobj::real_t vx = objAttrib.vertices[3*idx.vertex_index+0];
                        tinyobj::real_t vy = objAttrib.vertices[3*idx.vertex_index+1];
                        tinyobj::real_t vz = objAttrib.vertices[3*idx.vertex_index+2];

                        currentAABB.min = glm::min(currentAABB.min,glm::vec3(vx,vy,vz));
                        currentAABB.max = glm::max(currentAABB.max,glm::vec3(vx,vy,vz));

                        CHECK_F(objAttrib.normals.size() > 0 && idx.normal_index >=0,"All objects need normals to load");
                        //TODO: We should really compute normals if none are give to us (@HW)
                        tinyobj::real_t nx = objAttrib.normals[3*idx.normal_index+0];
                        tinyobj::real_t ny = objAttrib.normals[3*idx.normal_index+1];
                        tinyobj::real_t nz = objAttrib.normals[3*idx.normal_index+2];

                        tinyobj::real_t tx=-1,ty=-1; //TODO: Ohh, what do if there is no texture coordinates?
                        if(objAttrib.texcoords.size() > 0 && idx.texcoord_index > 0){
                            tx = objAttrib.texcoords[2*idx.texcoord_index+0];
                            ty = objAttrib.texcoords[2*idx.texcoord_index+1];
                        }
                        float vertex[8] ={vx,vy,vz,tx,ty,nx,ny,nz};
                        vertexData.insert(vertexData.end(),vertex,vertex+8); //maybe should be vertexData.insert(vertex.end(),std::begin(vertex),std::end(vertex+)) ie make vertex a vector
                        indices.push_back( indices.size() );
                        // Optional: vertex colors
                        // tinyobj::real_t red = objAttrib.colors[3*idx.vertex_index+0];
                        // tinyobj::real_t green = objAttrib.colors[3*idx.vertex_index+1];
                        // tinyobj::real_t blue = objAttrib.colors[3*idx.vertex_index+2];
                    }
                    index_offset += fv;

                    // per-face material
                    objShapes[s].mesh.material_ids[f];
                }
            }
            //Copy vertex data read so far into the model
            int numAttribs = vertexData.size();
            models[childModelID].modelData = new float[numAttribs];
            //models[childModelID].indices = indices;
            //models[childModelID].numIndices = indices.size();
            models[childModelID].lods.resize( 1 );
            models[childModelID].lods[0].indices    = indices;
            models[childModelID].lods[0].numIndices = indices.size();
            models[childModelID].lods[0].startIndex = 0;
            models[childModelID].aabb = currentAABB;
            std::copy(vertexData.begin(),vertexData.end(),models[childModelID].modelData);
            models[childModelID].numVerts = numAttribs/8;
            LOG_F(1,"Loaded %d vertices",models[childModelID].numVerts);
            addChild(childName,curModelID);
        } else if(commandStr == "child"){
            int openBracket = line.find("[")+1;
            int modelNameLength = line.find("]")-openBracket;
            string childName = line.substr(openBracket,modelNameLength);
            LOG_F(1,"Adding Child with name %s to model %d",childName.c_str(),curModelID);
            addChild(childName,curModelID);
        } else if(commandStr == "material"){
            int openBracket = line.find("[")+1;
            int modelNameLength = line.find("]")-openBracket;
            string materialName = line.substr(openBracket,modelNameLength);
            int materialID = findMaterial(materialName);
            LOG_F(1,"Binding material: '%s' (Material ID %d) to Model %d",materialName.c_str(),materialID,curModelID);
            models[curModelID].materialID = materialID;
        } else {
            LOG_F(WARNING,"WARNING. Unknown model command: %s",commandStr.c_str());
        }
    }
}
