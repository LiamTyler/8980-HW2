#ifndef SHADOWS_H
#define SHADOWS_H

#include "Materials.h"
#include "Models.h"

class GameObject;

//Shadow mapping functions
void initShadowMapping();
void initShadowBuffers();
void computeShadowDepthMap(glm::mat4 lightView, glm::mat4 lightProjection, const std::vector<Model*>& toDraw);
void computeShadowDepthMapStatic(glm::mat4 lightView, glm::mat4 lightProjection, const std::vector<GameObject*>& staticObjs );
void drawGeometryShadow(int shaderProgram, const Model& model, Material material, glm::mat4 transform);

//Configuration values that can be set:
extern unsigned int shadowMapWidth, shadowMapHeight;

//Global values we write out:
extern unsigned int depthMapTex;

extern unsigned int g_shadowTris;

#endif //SHADOWS_H
