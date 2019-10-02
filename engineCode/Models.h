#ifndef MODELS_H
#define MODELS_H

#include "Materials.h"
#include "CollisionSystem.h"

#include <vector>
#include <string>

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

struct LODInfo
{
    LODInfo() : startIndex( 0 ), numIndices( 0 ) {}
    LODInfo( size_t start, uint32_t num, const std::vector< uint32_t>& ind ) :
        startIndex( start ),
        numIndices( num ),
        indices( ind )
    {
    }

    size_t   startIndex = 0;
    uint32_t numIndices = 0;
    std::vector< uint32_t > indices;
};

struct Model{
    std::string name = "**UNNAMED Model**";
    int ID = -1;
    AABB aabb;
    glm::mat4 transform;
    glm::mat4 modelOffset; //Just for placing geometry, not passed down the scene graph
    float* modelData = nullptr;
    std::vector< uint32_t > indices;
    size_t startIndex;
    uint32_t numIndices;
    int startVertex;
    int numVerts = 0;
    int numChildren = 0;
    int materialID = -1;
    Collider* collider = nullptr;
    glm::vec2 textureWrap = glm::vec2(1,1);
    glm::vec3 modelColor = glm::vec3(1,1,1);
    std::vector<Model*> childModel;
    std::vector< LODInfo > lods;
	bool isDynamic = false;
};

void resetModels();
void loadModel(string fileName);

void loadAllModelsTo1VBO( unsigned int vbo, unsigned int ibo );
int addModel(string modelName);
void addChild(string childName, int curModelID);

//Global Model List
extern Model models[100000];
extern int numModels;

#endif //MODELS_H
