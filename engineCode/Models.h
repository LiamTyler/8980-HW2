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

struct Model{
    std::string name = "**UNNAMED Model**";
    int ID = -1;
    AABB aabb;
    glm::mat4 transform;
    glm::mat4 modelOffset; //Just for placing geometry, not passed down the scene graph
    glm::mat4 finalTransform;
    float* modelData = nullptr;
    int startVertex;
    int numVerts = 0;
    int numChildren = 0;
    int materialID = -1;
    Collider* collider = nullptr;
    glm::vec2 textureWrap = glm::vec2(1,1);
    glm::vec3 modelColor = glm::vec3(1,1,1); //TODO: Perhaps we can replace this simple approach with a more general material blending system?
    std::vector<Model*> childModel;
};

void resetModels();
void loadModel(string fileName);

void loadAllModelsTo1VBO(unsigned int vbo);
int addModel(string modelName);
void addChild(string childName, int curModelID);

//Global Model List
extern Model models[100000];
extern int numModels;

#endif //MODELS_H
