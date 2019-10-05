#include "GPU-Includes.h"
#include "RenderingSystem.h"
#include "RenderingCore.h"
#include "WindowManager.h"
#include "Bloom.h"
#include "Skybox.h"
#include "Shadows.h"
#include "CollisionSystem.h"
#include "Shader.h"
#include "Frustum.h"
#include <algorithm>
#include <external/loguru.hpp>
#include <external/stb_image.h>

Frustum g_frustum;
int g_numDrawn = 0;
int g_currentLOD = 2;

using std::vector;

GLuint tex[1000];

bool xxx; //Just an unspecified bool that gets passed to shader for debugging

int sphereVerts; //Number of verts in the colliders spheres

int totalTriangles = 0;

GLint uniColorID, uniEmissiveID, uniUseTextureID, modelColorID;
GLint metallicID, roughnessID, iorID, reflectivenessID;
GLint uniModelMatrix, colorTextureID, texScaleID, biasID, pcfID;
GLint xxxID;
GLint debugTransform;
GLint debugColor;

GLuint fboHDR; //floating point FBO for HDR rendering (two render outputs, base color and bloom)
GLuint baseTex, brightText; //Textures which are bound to the bloom FBOs
unsigned int pingpongFBO[2];
unsigned int pingpongColorbuffers[2];
Shader PBRShader;

GLint posAttrib, texAttrib, normAttrib;
GLuint modelsVAO, modelsVBO, modelsIBO;

GLuint colliderVAO; //Build a Vertex Array Object for the collider

GLuint unitCube;
GLuint unitCubeVAO;

Shader debugShader;
GLint uniView,uniInvView, uniProj;

// Draws unrotated AABB
void drawDebugGeometry(const Model& model, const glm::mat4 proj, const glm::mat4 view, glm::mat4 transform = glm::mat4(), glm::vec3 modelColor = glm::vec3(1, 0, 0))
{
	transform *= model.transform;

    for (int i = 0; i < model.numChildren; i++)
    {
		drawDebugGeometry(*model.childModel[i], proj, view, transform, modelColor);
	}
    if ( !model.modelData )
    {
        return;
    }

    auto min = model.aabb.min;
    auto max = model.aabb.max;
    auto e = max - min;
    std::vector< glm::vec3 > points =
    {
        min,
        min + glm::vec3( e.x, 0, 0 ),
        min + glm::vec3( 0, 0, e.z ),
        min + glm::vec3( e.x, 0, e.z ),
        min + glm::vec3( 0, e.y, 0 ),
        min + glm::vec3( e.x, e.y, 0 ),
        min + glm::vec3( 0, e.y, e.z ),
        max,
    };

    glm::vec3 newMin = glm::vec3( FLT_MAX );
    glm::vec3 newMax = -glm::vec3( FLT_MAX );
    for ( const auto& p : points )
    {
        auto newP = glm::vec3( transform * glm::vec4( p, 1 ) );
        newMin = glm::min( newMin, newP );
        newMax = glm::max( newMax, newP );
    }

    glm::vec3 extents    = ( newMax - newMin ) * .5f;
	glm::vec3 aabbOffset = ( newMax + newMin ) * .5f;

	//GLint TF = glGetUniformLocation(debugShader.ID, "transform");
	auto boxTransform = proj * view * glm::translate(glm::mat4(), aabbOffset) * glm::scale(glm::mat4(),extents);
	// auto boxTransform = proj * view * glm::translate(glm::mat4(), glm::vec3(transform[3]) + aabbOffset) * glm::scale(glm::mat4(),extents);
	glUniformMatrix4fv(debugTransform, 1, GL_FALSE, glm::value_ptr(boxTransform));
	glUniform4fv(debugColor, 1, glm::value_ptr(glm::vec4(modelColor,1)));
	//glLineWidth(5);
	glDrawArrays(GL_LINES, 0, 24);


	/*if (!g_frustum.AABBIntersect(model.aabb.min + pos, model.aabb.max + pos))
	{
		return;
	}*/
}

void drawAABBs( const std::vector<Model*>& dynamicModels, const std::vector< GameObject >& staticGameObjects, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& otherCamView, glm::mat4& otherCamModel )
{
	// Camera Frustum
	debugShader.bind();
	glBindVertexArray(unitCubeVAO);
	debugTransform = glGetUniformLocation(debugShader.ID, "transform");	// TODO: should set these up in a init function
	debugColor = glGetUniformLocation(debugShader.ID, "color");			// ''
	auto viewProj = proj * view * otherCamModel * glm::inverse(proj);
	glm::vec4 color(1, 1, 0, 1);
	glUniformMatrix4fv(debugTransform, 1, GL_FALSE, glm::value_ptr(viewProj));
	glUniform4fv(debugColor, 1, glm::value_ptr(color));
	glDrawArrays(GL_LINES,0,24);

	for (size_t i = 0; i < dynamicModels.size(); i++) {
		drawDebugGeometry(*dynamicModels[i], proj, view);
	}

    for ( const auto& obj : staticGameObjects )
    {
        glm::vec3 extents    = ( obj.aabb.max - obj.aabb.min ) * .5f;
        glm::vec3 aabbOffset = ( obj.aabb.max + obj.aabb.min ) * .5f;

        //GLint TF = glGetUniformLocation(debugShader.ID, "transform");
        auto boxTransform = proj * view * glm::translate(glm::mat4(), aabbOffset) * glm::scale(glm::mat4(),extents);
        // auto boxTransform = proj * view * glm::translate(glm::mat4(), glm::vec3(transform[3]) + aabbOffset) * glm::scale(glm::mat4(),extents);
        glUniformMatrix4fv(debugTransform, 1, GL_FALSE, glm::value_ptr(boxTransform));
        glUniform4fv(debugColor, 1, glm::value_ptr(glm::vec4( 1, 0, 0, 1)));
        //glLineWidth(5);
        glDrawArrays(GL_LINES, 0, 24);
    }
}

void drawGeometry(const Model& model, int matID, glm::mat4 transform = glm::mat4(), glm::vec2 textureWrap=glm::vec2(1,1), glm::vec3 modelColor=glm::vec3(1,1,1));

void drawGeometry(const Model& model, int materialID, glm::mat4 transform, glm::vec2 textureWrap, glm::vec3 modelColor){
    //printf("Model: %s, num Children %d\n",model.name.c_str(), model.numChildren);
    //printf("Material ID: %d (passed in id = %d)\n", model.materialID,materialID);
    //printf("xyx %f %f %f %f\n",model.transform[0][0],model.transform[0][1],model.transform[0][2],model.transform[0][3]);
    //if (model.materialID >= 0) material = materials[model.materialID];


    Material material;
    if (materialID < 0){
        materialID = model.materialID; 
    } 
    if (materialID >= 0){
        material = materials[materialID]; // Maybe should be a pointer
    }
    //printf("Using material %d - %s\n", model.materialID,material.name.c_str());

    transform *= model.transform;
    modelColor *= model.modelColor;
    //textureWrap *= model.textureWrap; //TODO: Where best to apply textureWrap transform?

    for (int i = 0; i < model.numChildren; i++){
        drawGeometry(*model.childModel[i], materialID, transform,textureWrap, modelColor);
    }
    if (!model.modelData) return;

    glm::vec3 pos = glm::vec3( transform[3] );
    if ( !g_frustum.AABBIntersect( model.aabb.min + pos, model.aabb.max + pos ) )
    {
        return;
    }
    ++g_numDrawn;

    // transform *= model.modelOffset;
    textureWrap *= model.textureWrap; //TODO: Should textureWrap stack like this?

    //std::cout << "Model: " << model.name << ", id = " << model.ID << std::endl;
    //std::cout << transform << std::endl;
    //assert( transform == model.finalTransform );
    //std::cout << model.finalTransform << std::endl;
    glUniformMatrix4fv(uniModelMatrix, 1, GL_FALSE, glm::value_ptr(transform));
    //glUniformMatrix4fv(uniModelMatrix, 1, GL_FALSE, glm::value_ptr(model.finalTransform));

    glUniform1i(uniUseTextureID, material.textureID >= 0); //textureID of -1 --> no texture

    if (material.textureID >= 0){
        glActiveTexture(GL_TEXTURE0);  //Set texture 0 as active texture
        glBindTexture(GL_TEXTURE_2D, tex[material.textureID]); //Load bound texture
        glUniform1i(colorTextureID, 0); //Use the texture we just loaded (texture 0) as material color
        glUniform2fv(texScaleID, 1, glm::value_ptr(textureWrap));
    }

    glUniform1i(xxxID, xxx);

    //printf("%f\n",model.modelColor[0]);
    glUniform3fv(modelColorID, 1, glm::value_ptr(modelColor*model.modelColor)); //multiply parent's color by your own

    glUniform1f(metallicID, material.metallic); 
    glUniform1f(roughnessID, material.roughness); 
    glUniform1f(iorID, material.ior);
    glUniform1f(reflectivenessID, material.reflectiveness);
    glUniform3fv(uniColorID, 1, glm::value_ptr(material.col));
    glUniform3fv(uniEmissiveID, 1, glm::value_ptr(material.emissive));

    //printf("start/end %d %d\n",model.startVertex, model.numVerts);
    totalTriangles += model.numVerts/3; //3 verts to a triangle
    // glDrawArrays(GL_TRIANGLES, model.startVertex, model.numVerts); //(Primitive Type, Start Vertex, End Vertex) //Draw only 1st object
	// glDrawElements(GL_TRIANGLES, model.numIndices, GL_UNSIGNED_INT, (void*)(model.startIndex * sizeof( uint32_t ) ) );
    int lod = std::min( (int)model.lods.size() - 1, std::max( 0, g_currentLOD ) );
    glDrawElements( GL_TRIANGLES, model.lods[lod].numIndices, GL_UNSIGNED_INT,
                    (void*)(model.lods[lod].startIndex * sizeof( uint32_t ) ) );
}

int drawStaticSceneGeometry( const std::vector< GameObject* >& staticGameObjects, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& otherCamView, glm::mat4& otherCamModel,
                             const glm::mat4& lightViewMatrix, const glm::mat4& lightProjectionMatrix, bool useShadowMap )
{
    setPBRShaderUniforms( view, proj, lightViewMatrix, lightProjectionMatrix, useShadowMap );
    updatePRBShaderSkybox(); //TODO: We only need to do this if the skybox changes

    glBindVertexArray(modelsVAO);

    glm::vec3 modelColor = glm::vec3( 1 );
    glm::vec2 textureWrap = glm::vec2( 1 );
    
    //std::cout << "\nDrawing\n" << std::endl;
    //glm::mat4 I;
    totalTriangles = 0;
    g_numDrawn = 0;
    /*for (size_t i = 0; i < culledModels.size(); ++i )
    {
        drawGeometry( *culledModels[i], -1, I );
    }*/
     for (const auto& obj : staticGameObjects){
        //drawGeometry(*toDraw[i], -1);

        //printf("Model: %s, num Children %d\n",model.name.c_str(), model.numChildren);
        //printf("Material ID: %d (passed in id = %d)\n", model.materialID,materialID);
        //printf("xyx %f %f %f %f\n",model.transform[0][0],model.transform[0][1],model.transform[0][2],model.transform[0][3]);
        //if (model.materialID >= 0) material = materials[model.materialID];

        Material material;
        if (obj->materialID >= 0){
            material = materials[obj->materialID]; // Maybe should be a pointer
        }
        //printf("Using material %d - %s\n", model.materialID,material.name.c_str());

        //obj.transform *= model.transform;
        // modelColor *= model.modelColor;
        //textureWrap *= model.textureWrap; //TODO: Where best to apply textureWrap transform?


        /*if ( !g_frustum.AABBIntersect( obj->aabb.min, obj->aabb.max ) )
        {
            continue;
        }*/
        ++g_numDrawn;

        // transform *= model.modelOffset;
        // !!!!!! textureWrap *= model.textureWrap; //TODO: Should textureWrap stack like this? ANSWER: NO

        //std::cout << "Model: " << model.name << ", id = " << model.ID << std::endl;
        //std::cout << transform << std::endl;
        //assert( transform == model.finalTransform );
        //std::cout << model.finalTransform << std::endl;
        glUniformMatrix4fv(uniModelMatrix, 1, GL_FALSE, glm::value_ptr( obj->transform ));
        //glUniformMatrix4fv(uniModelMatrix, 1, GL_FALSE, glm::value_ptr(model.finalTransform));

        glUniform1i(uniUseTextureID, material.textureID >= 0); //textureID of -1 --> no texture

        if (material.textureID >= 0){
            glActiveTexture(GL_TEXTURE0);  //Set texture 0 as active texture
            glBindTexture(GL_TEXTURE_2D, tex[material.textureID]); //Load bound texture
            glUniform1i(colorTextureID, 0); //Use the texture we just loaded (texture 0) as material color
            glUniform2fv(texScaleID, 1, glm::value_ptr(textureWrap));
        }

        glUniform1i(xxxID, xxx);

        glUniform3fv(modelColorID, 1, glm::value_ptr( modelColor )); //multiply parent's color by your own
        // !!!!! glUniform3fv(modelColorID, 1, glm::value_ptr(modelColor*model.modelColor)); //multiply parent's color by your own

        glUniform1f(metallicID, material.metallic); 
        glUniform1f(roughnessID, material.roughness); 
        glUniform1f(iorID, material.ior);
        glUniform1f(reflectivenessID, material.reflectiveness);
        glUniform3fv(uniColorID, 1, glm::value_ptr(material.col));
        glUniform3fv(uniEmissiveID, 1, glm::value_ptr(material.emissive));

        //printf("start/end %d %d\n",model.startVertex, model.numVerts);
        totalTriangles += obj->model->numVerts/3; //3 verts to a triangle
        // glDrawArrays(GL_TRIANGLES, model.startVertex, model.numVerts); //(Primitive Type, Start Vertex, End Vertex) //Draw only 1st object
	    // glDrawElements(GL_TRIANGLES, model.numIndices, GL_UNSIGNED_INT, (void*)(model.startIndex * sizeof( uint32_t ) ) );
        int lod = std::min( (int)obj->model->lods.size() - 1, std::max( 0, g_currentLOD ) );
        glDrawElements( GL_TRIANGLES, obj->model->lods[lod].numIndices, GL_UNSIGNED_INT,
                        (void*)(obj->model->lods[lod].startIndex * sizeof( uint32_t ) ) );
     }


    return g_numDrawn;
}

void drawColliderGeometry(){ //, Material material //TODO: Take in a material for the colliders
    //printf("Drawing %d Colliders\n",collisionModels.size());
    //printf("Material ID: %d\n", material);

    glBindVertexArray(colliderVAO);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glm::vec3 baseColor = glm::vec3(1,1,1);
    glm::vec3 modelColor = glm::vec3(1,0,0);

    glUniform1i(uniUseTextureID, -1); //textureID of -1 --> no texture

    glUniform1i(xxxID, xxx);

    glUniform3fv(modelColorID, 1, glm::value_ptr(baseColor)); //multiply parent's color by your own

    glUniform1f(metallicID, 0); 
    glUniform1f(roughnessID, 0); 
    glUniform1f(iorID, 1);
    glUniform3fv(uniColorID, 1, glm::value_ptr(modelColor));
    glUniform3fv(uniEmissiveID, 1, glm::value_ptr(modelColor));

    //TODO: Maybe loop through each layer and color by layer instead?
    for (size_t i = 0; i < collisionModels.size(); i++){
        Collider* c = models[collisionModels[i]].collider;
        if (c == nullptr) continue;
        //printf("Drawing at pos %f %f %f, radius = %f\n",c->globalPos[0],c->globalPos[1],c->globalPos[2],c->r);

        glm::mat4 colliderTans = glm::translate(glm::mat4(), c->globalPos);
        colliderTans = glm::scale(colliderTans, glm::vec3(c->r,c->r,c->r));

        glUniformMatrix4fv(uniModelMatrix, 1, GL_FALSE, glm::value_ptr(colliderTans));
        glDrawArrays(GL_TRIANGLES, 0, sphereVerts/3); //(Primitive Type, Start Vertex, End Vertex) //Draw only 1st object
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}


//TODO: When is the best time to call this? This loads all textures, should we call it per-texture instead?
void loadTexturesToGPU(){
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    for (int i = 0; i < numTextures; i++){
        LOG_F(1,"Loading Texture %s",textures[i].c_str());
        unsigned char *pixelData = stbi_load(textures[i].c_str(), &width, &height, &nrChannels, STBI_rgb);
        CHECK_NOTNULL_F(pixelData,"Fail to load model texture: %s",textures[i].c_str()); //TODO: Is there some way to get the error from STB image?

        //Load the texture into memory
        glGenTextures(1, &tex[i]);
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexStorage2D(GL_TEXTURE_2D, 2, GL_RGBA8, width, height); //Mipmap levels
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixelData);
        glGenerateMipmap(GL_TEXTURE_2D);

        //What to do outside 0-1 range
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //TODO: Does this look better? I'm not sure
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(pixelData);
    }
}


//------------ HDR ------------------

void BindHDRFramebuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fboHDR);
}

void initHDRBuffers(){
    glGenFramebuffers(1, &fboHDR);
    glBindFramebuffer(GL_FRAMEBUFFER, fboHDR);

    glGenTextures(1, &baseTex);
    glBindTexture(GL_TEXTURE_2D, baseTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, baseTex, 0);

    glGenTextures(1, &brightText);
    glBindTexture(GL_TEXTURE_2D, brightText);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, brightText, 0);

    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, screenWidth, screenHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    //Specify which color attachments we'll use (of this framebuffer) for rendering (both regular and bright pixels) 
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    CHECK_F(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer not complete!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // A pair of ping-pong framebuffers for extended blurring
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++){
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); //why clamp?
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER,0); //Return to normal rendering
}


//------------ PBR Shader ---------

void initPBRShading(){
    PBRShader = Shader("shaders/vertexTex.glsl", "shaders/fragmentTex.glsl");
    PBRShader.init();

    //Build a Vertex Array Object. This stores the VBO to shader attribute mappings
    glGenVertexArrays(1, &modelsVAO); //Create a VAO
    glBindVertexArray(modelsVAO); //Bind the above created VAO to the current context

    //We'll store all our models in one VBO //TODO: We should compare to 1 VBO/model?
    glGenBuffers(1, &modelsVBO); 
    glGenBuffers(1, &modelsIBO); 
    loadAllModelsTo1VBO(modelsVBO, modelsIBO );

    //Tell OpenGL how to set fragment shader input 
    posAttrib = glGetAttribLocation(PBRShader.ID, "position");
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), 0);
    //Attribute, vals/attrib., type, normalized?, stride, offset
    //Binds to VBO current GL_ARRAY_BUFFER 
    glEnableVertexAttribArray(posAttrib);

    //GLint colAttrib = glGetAttribLocation(phongShader, "inColor");
    //glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    //glEnableVertexAttribArray(colAttrib);

    texAttrib = glGetAttribLocation(PBRShader.ID, "inTexcoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));

    normAttrib = glGetAttribLocation(PBRShader.ID, "inNormal");
    glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(5*sizeof(float)));
    glEnableVertexAttribArray(normAttrib);


    uniView = glGetUniformLocation(PBRShader.ID, "view");
    uniInvView  = glGetUniformLocation(PBRShader.ID, "invView"); //inverse of view matrix
    uniProj = glGetUniformLocation(PBRShader.ID, "proj");


    uniColorID = glGetUniformLocation(PBRShader.ID, "materialColor");
    uniEmissiveID = glGetUniformLocation(PBRShader.ID, "emissive");
    uniUseTextureID = glGetUniformLocation(PBRShader.ID, "useTexture");
    modelColorID = glGetUniformLocation(PBRShader.ID, "modelColor");
    metallicID = glGetUniformLocation(PBRShader.ID, "metallic");
    roughnessID = glGetUniformLocation(PBRShader.ID, "roughness");
    biasID = glGetUniformLocation(PBRShader.ID, "shadowBias");
    pcfID = glGetUniformLocation(PBRShader.ID, "pcfSize");
    iorID = glGetUniformLocation(PBRShader.ID, "ior");
    reflectivenessID = glGetUniformLocation(PBRShader.ID, "reflectiveness");
    uniModelMatrix = glGetUniformLocation(PBRShader.ID, "model");
    colorTextureID = glGetUniformLocation(PBRShader.ID, "colorTexture");
    texScaleID = glGetUniformLocation(PBRShader.ID, "textureScaleing");
    xxxID = glGetUniformLocation(PBRShader.ID, "xxx");

    PBRShader.bind();
    glUniformMatrix4fv(glGetUniformLocation(PBRShader.ID, "rotSkybox"), 1, GL_FALSE, &curScene.rotSkybox[0][0]);


    glBindVertexArray(0); //Unbind the VAO in case we want to create a new one
}

void setPBRShaderUniforms(glm::mat4 view, glm::mat4 proj, glm::mat4 lightViewMatrix, glm::mat4 lightProjectionMatrix, bool useShadowMap){
    glBindFramebuffer(GL_FRAMEBUFFER, fboHDR);
    PBRShader.bind();
    glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));
    glm::mat4 invView = glm::inverse(view);
    glUniformMatrix4fv(uniInvView, 1, GL_FALSE, glm::value_ptr(invView));
    glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

    glUniform1i(glGetUniformLocation(PBRShader.ID, "numLights"), curScene.lights.size());

    glUniform3fv(glGetUniformLocation(PBRShader.ID, "inLightDir"), curScene.lights.size(), glm::value_ptr(lightDirections[0]));
    glUniform3fv(glGetUniformLocation(PBRShader.ID, "inLightCol"), curScene.lights.size(), glm::value_ptr(lightColors[0]));

    glUniform1f(biasID, curScene.shadowLight.shadowBias);
    glUniform1i(pcfID, curScene.shadowLight.pcfWidth);

    glUniform3fv(glGetUniformLocation(PBRShader.ID, "ambientLight"), 1, glm::value_ptr(curScene.ambientLight));

    glUniformMatrix4fv(glGetUniformLocation(PBRShader.ID, "shadowView"), 1, GL_FALSE, &lightViewMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(PBRShader.ID, "shadowProj"), 1, GL_FALSE, &lightProjectionMatrix[0][0]);

    glActiveTexture(GL_TEXTURE1); //Texture 1 in the PBR Shader is the shadow map
    glBindTexture(GL_TEXTURE_2D, depthMapTex); 
    glUniform1i(glGetUniformLocation(PBRShader.ID, "shadowMap"), 1);

    glUniform1i(glGetUniformLocation(PBRShader.ID, "useShadow"), useShadowMap && curScene.shadowLight.castShadow);
}

// ---------------- Collider Geometry -----------

int createColliderSphere(int sphereVbo) {
    int stacks = 4;
    int slices = 4;
    const float PI = 3.14f;

    std::vector<float> positions;
    std::vector<float> verts;

    for (int i = 0; i <= stacks; ++i){
        float V = (float)i / (float)stacks; 
        float phi = V * PI;

        for (int j = 0; j <= slices; ++j){
            float U = (float)j / (float)slices;
            float theta = U * (PI * 2);

            // use spherical coordinates to calculate the positions.
            float x = cos(theta) * sin(phi);
            float y = cos(phi);
            float z = sin(theta) * sin(phi);

            positions.push_back(x);
            positions.push_back(y);
            positions.push_back(z);
        }
    }

    // Calc The Index Positions
    for (int i = 0; i < slices * stacks + slices; ++i){
        int s = i;
        for (int j = 0; j < 3; j++) verts.push_back(positions[3*s+j]);
        s = i + slices + 1;
        for (int j = 0; j < 3; j++) verts.push_back(positions[3*s+j]);
        s = i + slices;
        for (int j = 0; j < 3; j++) verts.push_back(positions[3*s+j]);

        s = i + slices + 1;
        for (int j = 0; j < 3; j++) verts.push_back(positions[3*s+j]);
        s = i;
        for (int j = 0; j < 3; j++) verts.push_back(positions[3*s+j]);
        s = i + 1;
        for (int j = 0; j < 3; j++) verts.push_back(positions[3*s+j]);
    }

    // upload geometry to GPU.
    glBindBuffer(GL_ARRAY_BUFFER, sphereVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*verts.size(), verts.data(), GL_STATIC_DRAW);

    LOG_F(INFO,"Created Colider Circle with %d verts", (int)verts.size());
    return (int)verts.size();
}


void initColliderGeometry(){
    //PBRShader.bind();  //It's still bound from our call to initPBRShading()
    glGenVertexArrays(1, &colliderVAO); //Create a VAO
    glBindVertexArray(colliderVAO); //Bind the above created VAO to the current context

    GLuint colliderVBO;
    glGenBuffers(1, &colliderVBO);  //Create 1 buffer called vbo
    glBindBuffer(GL_ARRAY_BUFFER, colliderVBO); //(Only one buffer can be bound at a time) 

    //Tell OpenGL how to set fragment shader input 
    //posAttrib = glGetAttribLocation(PBRShader.ID, "position");
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), 0);
    glEnableVertexAttribArray(posAttrib);

    //glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    sphereVerts = createColliderSphere(colliderVBO);

    glBindVertexArray(0); //Unbind the VAO once we have set all the attributes
}


// --------  Draw Scene Geometry ----
void updatePRBShaderSkybox(){
    glActiveTexture(GL_TEXTURE5); //TODO: List what the first 5 textures are used for
    glUniform1i(glGetUniformLocation(PBRShader.ID, "skybox"),5);

    glUniform1i(glGetUniformLocation(PBRShader.ID, "useSkyColor"), curScene.singleSkyColor);
    if (curScene.singleSkyColor){
        glUniform3fv(glGetUniformLocation(PBRShader.ID, "skyColor"), 1, glm::value_ptr(curScene.skyColor));
    } else{
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    }
}

void CalculateFinalModelMatrix( Model& model, glm::mat4 transform = glm::mat4() )
{
    // std::cout << "Model: " << model.name << ", id = " << model.ID << std::endl;
    transform *= model.transform;
    // std::cout << transform << std::endl;
    // model.finalTransform = transform;
    for ( int i = 0; i < model.numChildren; i++ )
    {
        CalculateFinalModelMatrix( *model.childModel[i], transform );
    }
}

int drawSceneGeometry(const vector<Model*>& toDraw, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& otherCamView, glm::mat4& otherCamModel,
        const glm::mat4& lightViewMatrix, const glm::mat4& lightProjectionMatrix, bool useShadowMap )
{
    //g_frustum.UpdatePlanesExtract( proj * otherCamView );
    /*static int count = 0;
    for (size_t i = 0; i < toDraw.size(); i++ )
    {
        CalculateFinalModelMatrix( *toDraw[i] );
    }

    culledModels.reserve( toDraw.size() );
    culledModels.resize( 0 );

    for (size_t i = 0; i < toDraw.size(); i++ )
    {
        const Model& model = *toDraw[i];
        if ( g_frustum.AABBIntersect( model.aabb.min, model.aabb.max ) )
        {
            culledModels.push_back( toDraw[i] );
        }
    }*/
    //std::cout << "To draw: " << toDraw.size() << std::endl;
    //std::cout << "Survived culling: " << culledModels.size() << std::endl;
    //g_frustum.Print();
    setPBRShaderUniforms( view, proj, lightViewMatrix, lightProjectionMatrix, useShadowMap );
    updatePRBShaderSkybox(); //TODO: We only need to do this if the skybox changes

    glBindVertexArray(modelsVAO);
    
    //std::cout << "\nDrawing\n" << std::endl;
    //glm::mat4 I;
    //totalTriangles = 0;
    //g_numDrawn = 0;
    /*for (size_t i = 0; i < culledModels.size(); ++i )
    {
        drawGeometry( *culledModels[i], -1, I );
    }*/
     for (size_t i = 0; i < toDraw.size(); i++){
         drawGeometry(*toDraw[i], -1);
     }


    return g_numDrawn;
}

//-------------  Final Composite --------------

Shader compositeShader;

unsigned int quadVAO;


void initFinalCompositeShader(){
    compositeShader = Shader("shaders/quad-vert.glsl", "shaders/quad-frag.glsl");
    compositeShader.init();
}

void drawCompositeImage(bool useBloom){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(quadVAO);
    glClear(GL_DEPTH_BUFFER_BIT);

    float bloomLevel = 0;
    compositeShader.bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, brightText); 
    glUniform1i(glGetUniformLocation(compositeShader.ID, "texBright"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, baseTex);  //baseTex  depthMap
    glUniform1i(glGetUniformLocation(compositeShader.ID, "texDim"), 1);

    if (useBloom){
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]); //pass in blured texture
        bloomLevel = 1;
    }

    glUniform1f(glGetUniformLocation(compositeShader.ID, "bloomAmount"), bloomLevel);

    glDrawArrays(GL_TRIANGLES, 0, 6);  //Draw Quad for final render
}

// --------- Fullscreen quad
void createFullscreenQuad(){
    float quad[24] = {1,1,1,1, -1,1,0,1, -1,-1,0,0,  1,-1,1,0, 1,1,1,1, -1,-1,0,0,};

    //Build a Vertex Array Object for the full screen Quad. This stores the VBO to shader attribute mappings
    glGenVertexArrays(1, &quadVAO); //Create a VAO
    glBindVertexArray(quadVAO); //Bind the above created VAO to the current context

    GLuint quadVBO;
    glGenBuffers(1, &quadVBO);  //Create 1 buffer called vbo
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO); //(Only one buffer can be bound at a time) 

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    GLint quadPosAttrib = glGetAttribLocation(compositeShader.ID, "position");
    glVertexAttribPointer(quadPosAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
    //(above params: Attribute, vals/attrib., type, normalized?, stride, offset)
    glEnableVertexAttribArray(quadPosAttrib);

    GLint quadTexAttrib = glGetAttribLocation(compositeShader.ID, "texcoord");
    glVertexAttribPointer(quadTexAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(quadTexAttrib); 

    glBindVertexArray(0); //Unbind the VAO once we have set all the attributes
}

void cleanupBuffers(){
    glDeleteBuffers(1,&modelsVBO);
    glDeleteVertexArrays(1, &modelsVAO);
    glDeleteVertexArrays(1, &colliderVAO);
    //TODO: Clearn up the other VAOs and VBOs
}


void initUnitCube()
{
    debugShader = Shader("shaders/debugVert.glsl", "shaders/debugFrag.glsl");
    debugShader.init();

    glGenBuffers(1, &unitCube);

    glm::vec3 pts[24];
    glm::vec3 cubePts[8];

    cubePts[0] = glm::vec3(-1,-1,-1);   // bbl
    cubePts[1] = glm::vec3(1,-1,-1);    // bbr
    cubePts[2] = glm::vec3(-1,1,-1);    // btl
    cubePts[3] = glm::vec3(1,1,-1);     // btr
    cubePts[4] = glm::vec3(-1,-1,1);    // fbl
    cubePts[5] = glm::vec3(1,-1,1);     // fbr
    cubePts[6] = glm::vec3(-1,1,1);     // ftl
    cubePts[7] = glm::vec3(1,1,1);      // ftr


    pts[0] = cubePts[0]; // bbl
    pts[1] = cubePts[1]; // bbr

    pts[2] = cubePts[1]; // bbr
    pts[3] = cubePts[3]; // btr

    pts[4] = cubePts[3]; // btr
    pts[5] = cubePts[2]; // btl

    pts[6] = cubePts[2]; // btl
    pts[7] = cubePts[0]; // bbl

    pts[8] = cubePts[4]; // fbl
    pts[9] = cubePts[5]; // fbr

    pts[10] = cubePts[5]; // fbr
    pts[11] = cubePts[7]; // ftr

    pts[12] = cubePts[7]; // ftr
    pts[13] = cubePts[6]; // ftl

    pts[14] = cubePts[6]; // ftl
    pts[15] = cubePts[4]; // fbl

    pts[16] = cubePts[4]; // fbl
    pts[17] = cubePts[0]; // bbl

    pts[18] = cubePts[5]; // fbr
    pts[19] = cubePts[1]; // bbr

    pts[20] = cubePts[6]; // ftl
    pts[21] = cubePts[2]; // btl

    pts[22] = cubePts[7]; // ftr
    pts[23] = cubePts[3]; // btr

    glBindBuffer(GL_ARRAY_BUFFER, unitCube);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_STATIC_DRAW);

    glGenVertexArrays(1, &unitCubeVAO);

    glBindVertexArray(unitCubeVAO);

    GLint posAttrib = glGetAttribLocation(debugShader.ID, "position");
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
    //Attribute, vals/attrib., type, normalized?, stride, offset
    //Binds to VBO current GL_ARRAY_BUFFER 
    glEnableVertexAttribArray(posAttrib);
}
