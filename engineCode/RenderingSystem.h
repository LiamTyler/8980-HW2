#ifndef RENDERING_H
#define RENDERING_H

#include "RenderingCore.h"
#include "Scene.h"
#include "Materials.h"
#include "Models.h"
#include <iostream>

extern std::vector<Model*> toDraw;
extern bool xxx; //

//Main geometry drawing functions
void initPBRShading();
void setPBRShaderUniforms(glm::mat4 view, glm::mat4 proj, glm::mat4 lightViewMatrix, glm::mat4 lightProjectionMatrix, bool useShadowMap);

int drawSceneGeometry(const std::vector<Model*>& toDraw, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& otherCamView, glm::mat4& otherCamModel,
                      const glm::mat4& lightViewMatrix, const glm::mat4& lightProjectionMatrix, bool useShadowMap );

int drawStaticSceneGeometry( const std::vector< GameObject* >& staticGameObjects, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& otherCamView, glm::mat4& otherCamModel,
                      const glm::mat4& lightViewMatrix, const glm::mat4& lightProjectionMatrix, bool useShadowMap );


void drawAABBs( const std::vector<Model*>& dynamicModels, const std::vector< GameObject >& staticGameObjects, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& otherCamView, glm::mat4& otherCamModel );

void drawBVH (const BVH& bvh, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& otherCamView, glm::mat4& otherCamModel  );

//HDR render targets
void initHDRBuffers();
void BindHDRFramebuffer();

//Collider spheres drawing function
void initColliderGeometry();
void drawColliderGeometry();
int createSphere(int sphereVbo);

//Final compositing functions
void initFinalCompositeShader();
void drawCompositeImage(bool useBloom);

//Cleanup
void cleanupBuffers();

//Global values we write out:
extern int totalTriangles;
extern GLuint modelsVBO;

//Debug Cube
void initUnitCube();

#endif //RENDERING_H
