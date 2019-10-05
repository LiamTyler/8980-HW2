#ifndef SCENE_H
#define SCENE_H

#include "glm/glm.hpp"
#include "Models.h"

#include <vector>
#include <string>

//inline std::ostream& operator<<( std::ostream& out, const glm::vec3& v )
//{
//    return out << v.x << " " << v.y << " " << v.z;
//}

struct Light{
	glm::vec3 color = glm::vec3(1,1,1);
	glm::vec3 direction = glm::vec3(-0.4 -0.1 -1.0);
	float distance = 10;
	float intensity = 3;
	bool castShadow = false; //TODO: Only 1 light can cast a shadow
	float frustLeft = -5.0f, frustRight = 5.0f;
	float frustBot = -5.0f, frustTop = 5.0f;
	float frustNear = 0.6f, frustFar = 20.0f;
	float shadowBias = 0.001;
	int pcfWidth = 1;
	std::string name = "**UNNAMED LIGHT***";
};
//TODO: Add point lights (we currently assume all lights are directional)

struct Camera{
	glm::vec3 camPos;
	glm::vec3 camDir;
	glm::vec3 camUp;
	glm::vec3 lookatPoint;
	float FOV = 50;
};

struct Model;

#define DEBUG_CAMERA 0 // TODO: represent cameras better later
#define MAIN_CAMERA 1

struct GameObject
{
    glm::mat4 transform;
    int materialID;
    AABB aabb;
    Model* model;
};

class BVH
{
public:
    AABB _boundingVolume;
    BVH* _left = nullptr;
    BVH* _right = nullptr;
    std::vector<GameObject*> _gameObjects;

    BVH(const std::vector<GameObject*>&);
    void partition(const std::vector<GameObject*>&);
};

struct Scene{
	std::string environmentMap = "";
	Light shadowLight;
	std::vector<Light> lights;
	Camera cameras[2];
	unsigned char currentCam = MAIN_CAMERA;
	Camera* activeCam;
	glm::mat4 rotSkybox; //skybox orientation
	bool singleSkyColor = true;
	glm::vec3 skyColor = glm::vec3(1,1,10);
	glm::vec3 ambientLight = glm::vec3(0.3, 0.3, 0.3);

	std::vector<Model*> toDraw;
    std::vector< GameObject > staticGameobjects;
    std::vector< Model* > dynamicModels;
};

void loadScene(std::string fileName);
void resetScene();

//TODO: Allow user to control lights via Lua script
//TODO: Allow the user to parent models to lights


//Global scene and list of lights/color
extern Scene curScene;
extern glm::vec3 lightDirections[20];
extern glm::vec3 lightColors[20];

#endif //SCENE_H