#include "GPU-Includes.h"

//imgui includes
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"

//stb_image include
#define STB_IMAGE_IMPLEMENTATION //put this line in only one .cpp file
#include <external/stb_image.h>

#include <external/loguru.hpp>
#include <algorithm>

bool useBloom = false;
bool useShadowMap = true;
bool drawColliders = false;

int targetFrameRate = 30;
float secondsPerFrame = 1.0f / (float)targetFrameRate;
float nearPlane = .2;
float farPlane = 20;

#include "luaSupport.h"

#include "RenderingSystem.h"
#include "Frustum.h"
#include "Skybox.h"
#include "Shadows.h"
#include "CollisionSystem.h"
#include "Materials.h"
#include "Models.h"
#include "Models.h"
#include "Shader.h"
#include "Bloom.h"
#include "Sound.h"
#include "Scene.h"
#include "keyboard.h"
#include "controller.h"
#include "WindowManager.h"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>

using namespace std;
using std::swap; //fast delete
using glm::vec3;
using glm::mat4;

const string materialsFile = "materials.txt";
const string modelFile = "Prefabs.txt";
const string sceneFile = "Scene.txt";

int targetScreenWidth = 1120;
int targetScreenHeight = 700;

bool saveOutput = false;

bool fullscreen = false;
void Win2PPM(int width,int height);

AudioManager audioManager = AudioManager();


void configEngine(string configFile,string configName);

unsigned char lastActiveCamera = MAIN_CAMERA;

int g_depthSlice = 0;

void CalculateFinalPosition( Model* model, glm::mat4 transform = glm::mat4(1), int materialID = -1, glm::vec2 textureWrap=glm::vec2(1,1), glm::vec3 modelColor=glm::vec3(1,1,1))
{
    transform *= model->transform;

    if (materialID < 0){
        materialID = model->materialID; 
    }

    for (int i = 0; i < model->numChildren; i++)
    {
        CalculateFinalPosition( model->childModel[i], transform, materialID);
    }

    if ( model->modelData )
    {
        GameObject obj;
        obj.model = model;
        obj.transform = transform;
        auto min = model->aabb.min;
        auto max = model->aabb.max;
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

        obj.aabb.min = glm::vec3( FLT_MAX );
        obj.aabb.max = -glm::vec3( FLT_MAX );
        for ( const auto& p : points )
        {
            auto newP = glm::vec3( transform * glm::vec4( p, 1 ) );
            obj.aabb.min = glm::min( obj.aabb.min, newP );
            obj.aabb.max = glm::max( obj.aabb.max, newP );
        }
        obj.materialID = materialID;
        curScene.staticGameobjects.push_back( obj );
    }
}

std::vector< GameObject* > g_visibleStaticGameObjects;

extern Frustum g_frustum;

void Cull( BVH* bvh )
{
    if ( !g_frustum.AABBIntersect( bvh->_boundingVolume.min, bvh->_boundingVolume.max ) )
    {
        return;
    }

    if ( !bvh->_left )
    {
        for ( const auto& o : bvh->_gameObjects )
        {
            g_visibleStaticGameObjects.push_back( o );
        }
        return;
    }

    Cull( bvh->_left );
    Cull( bvh->_right );
}

int main(int argc,char *argv[]){
    std::cout << "HELLO!" << std::endl;
    loguru::g_stderr_verbosity = 0; // Only show most relevant messages on stderr
    loguru::init(argc,argv); //Detect verbosity level on command line as -v.

    //Log messages to file:
    //loguru::add_file("fullLog.log", loguru::Append, loguru::Verbosity_MAX); //Will append to log from last run
    loguru::add_file("latest_fullLog.log",loguru::Truncate,loguru::Verbosity_MAX);
    loguru::add_file("latest_readable.log",loguru::Truncate,loguru::Verbosity_INFO);

    string gameFolder = "";
    string configType = "";
    if(argc == 1){
        LOG_F(ERROR,"No Gamefolder specified");
        printf("\nUSAGE: ./engine Gamefolder/ [EngineConfig]\n");
        exit(1);
    }
    if(argc >= 2) //First argument is which game folder to load
        gameFolder = string(argv[1]);
    if(argc >= 3) //Second argument is what engine quality settings to use
        configType = string(argv[2]);

    LOG_F(INFO,"Loading game folder %s",gameFolder.c_str());

    configEngine("settings.cfg",configType);

    string luaFile = gameFolder + "main.lua";

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);  //Initialize Graphics (for OpenGL) and Audio (for sound) and Game Controller

    audioManager.init();

    initControllers();
    //TODO: Let users plug in mid game, must look for events: SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED and SDL_CONTROLLERDEVICEREMAPPED

    lua_State * L = luaL_newstate(); //create a new lua state
    luaSetup(L); //Load custom engine functions for Lua scripts to use

    loadMaterials(gameFolder+materialsFile);
    LOG_F(INFO,"Read %d materials\n",numMaterials);

    loadModel(gameFolder+modelFile);
    LOG_F(INFO,"Read %d models\n",numModels);

    loadScene(gameFolder+sceneFile);
    LOG_F(INFO,"Loaded scene file\n");

    createOpenGLWindow(targetScreenWidth,targetScreenHeight,fullscreen);

    //Initalize various buffers on the GPU
    initIMGui();
    loadTexturesToGPU();
    initHDRBuffers();
    initSkyboxShader();
    initFinalCompositeShader();
    initShadowMapping();
    initBloom();
    initPBRShading();
    initColliderGeometry(); //Depends on PBRShading being initalized
    initSkyboxBuffers();
    initShadowBuffers();
    initUnitCube();

    //Create a quad to be used for fullscreen rendering
    createFullscreenQuad();

    glEnable(GL_DEPTH_TEST);  //Have closer objects hide further away ones

    //Load Lua Gamescript (this will start the game running)
    LOG_F(INFO,"Loading Gamescript: '%s'",luaFile.c_str());
    int luaErr;
    luaErr = luaL_loadfile(L,luaFile.c_str());
    if(!luaErr)
        luaErr = lua_pcall(L,0,LUA_MULTRET,0);

    CHECK_F(luaErr==0,"Error loading Lua: %s ",lua_tostring(L,-1));

    LOG_F(INFO,"Script Loaded without Error.");

    int timeSpeed = 1;   //Modifies timestep rate given to Lua

	for (int i = 0; i < curScene.toDraw.size(); ++i)
	{
	    printf("Model %s %s dynamic\n", curScene.toDraw.at(i)->name.c_str(), (curScene.toDraw.at(i)->isDynamic ? "is" : "is not"));
        if ( !curScene.toDraw.at(i)->isDynamic )
        {
            CalculateFinalPosition( curScene.toDraw[i] );
        }
        else
        {
            curScene.dynamicModels.push_back( curScene.toDraw[i] );
        }
	}

    printf("\n");
    for ( size_t i = 0; i < curScene.staticGameobjects.size(); ++i )
    {
        auto p = curScene.staticGameobjects[i].transform[3];
        // printf( "Model %s is static at position: %f, %f, %f\n", curScene.staticGameobjects[i].model->name.c_str(), p.x, p.y, p.z );
        std::cout << "Model '" << curScene.staticGameobjects[i].model->name << "':\t" << curScene.staticGameobjects[i].aabb.min << "\t" << curScene.staticGameobjects[i].aabb.max << std::endl;

        g_visibleStaticGameObjects.push_back( &curScene.staticGameobjects[i] );
    }

    BVH bvh( g_visibleStaticGameObjects );

    //Event Loop (Loop while alive, processing each event as fast as possible)
    SDL_Event windowEvent;
    bool quit = false;
    while(!quit){
        //LOG_F(3,"New Frame.");
        while(SDL_PollEvent(&windowEvent)){  //inspect all events in the queue
            if(windowEvent.type == SDL_QUIT) quit = true;

            if(windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_ESCAPE)
                quit = true; //Exit event loop
            if(windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_f){ //If "f" is pressed
                fullscreen = !fullscreen;
                setWindowSize(targetScreenWidth,targetScreenHeight,fullscreen);
            }

            if(windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_s){ //If "s" is released
                saveOutput = false;
            }
            if(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_s){ //If "s" is released
                saveOutput = true;
            }
            if(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_x){
                xxx = !xxx;
            }

            if(windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_SPACE){
                if(timeSpeed != 1){
                    timeSpeed = 1;
                    audioManager.unpause();
                    printf("Resuming normal speed.\n");
                } else{
                    timeSpeed = 0;
                    audioManager.pause();
                    printf("Paused!\n");
                }
            }

            if( windowEvent.type == SDL_MOUSEWHEEL)
            {
                if( windowEvent.wheel.y > 0 ) // scroll up
                {
                    g_depthSlice += 1;
                     // Put code for handling "scroll up" here!
                }
                else if( windowEvent.wheel.y < 0) // scroll down
                {
                    g_depthSlice = std::max( -1, g_depthSlice - 1 );
                     // Put code for handling "scroll down" here!
                }
            }

			if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_d) {
					//printf("previous Current Camera: %d, lAC: %d\n", curScene.currentCam, lastActiveCamera);
					if (curScene.currentCam == DEBUG_CAMERA)
					{
						printf("Last Active Camera!\n");
						curScene.currentCam = lastActiveCamera;
						lastActiveCamera = DEBUG_CAMERA;
					}
					else
					{
						printf("Debug Camera!\n");
						lastActiveCamera = curScene.currentCam;
						curScene.currentCam = DEBUG_CAMERA;
					}
					//printf("post Current Camera: %d, lAC: %d\n", curScene.currentCam, lastActiveCamera);
			}

            if(windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_r){
                printf("Re-loading Materials file!\n");
                resetMaterials();
                loadMaterials(gameFolder+materialsFile);
                loadTexturesToGPU();

                printf("Re-loading Models file!\n");
                resetModels();
                loadModel(gameFolder+modelFile);
                // loadAllModelsTo1VBO(modelsVBO);

                printf("Re-loading Scene file!\n");
                resetScene();
                loadScene(gameFolder+sceneFile);
                loadSkyboxToGPU();
            }
        }


        //Read keyboard and send the state of the keys to the lau scrip
        updateKeyboardState();
        keyboardUpdateLau(L);

        //Read gamepad/controller and send the state to the lau script
        updateControllerState();
        gamepadUpdateLua(L);

        lua_getglobal(L,"frameUpdate");
        static long long lastTime_dt = 0; //@cleanup -- pull together various timing code
        long long curTime_dt = SDL_GetTicks(); //TODO: is this really long long?
        if(saveOutput) curTime_dt = lastTime_dt + .07 * 1000.0;
        float lua_dt = timeSpeed*(curTime_dt-lastTime_dt)/1000.0;
        lastTime_dt = curTime_dt;
        lua_pushnumber(L,lua_dt); //1/60.f
        //lua_call(L, 1, 0); //1 arguments, no returns
        luaErr = lua_pcall(L,1,0,0);
        CHECK_F(luaErr==0,"Error after call to lua function 'frameUpdate': %s \n",lua_tostring(L,-1));

        //Update Colliders
        updateColliderPositions();

        static long long lastTime;
        long long curTime_frame = (long long)SDL_GetTicks(); //TODO: Does this actually return a long long?
        float frameTime = float(curTime_frame-lastTime)/1000.f;
        int delayFrames = int((secondsPerFrame-frameTime)*1000.0 + 0.5);

        if(delayFrames > 0) SDL_Delay(delayFrames);
        lastTime = (long long)SDL_GetTicks();

		// Get camera from Lua
		//unsigned char curCam = MAIN_CAMERA;
		//curScene.currentCam = curCam;
		//printf("%d", getCamera(L));

		// Set the Active Camera
		curScene.activeCam = &curScene.cameras[curScene.currentCam];

        //Get the camera state from the Lua Script
		curScene.activeCam->camPos = getCameraPosFromLau(L);
		curScene.activeCam->camDir = getCameraDirFromLau(L);
		curScene.activeCam->camUp = getCameraUpFromLau(L);
		curScene.activeCam->lookatPoint = curScene.activeCam->camPos + curScene.activeCam->camDir;

        //LOG_F(3,"Read Camera from Lua");

        //TODO: Allow Lua script to set Lights dynamically (it's currently static)
        for(int i = 0; i < (int)curScene.lights.size(); i++){
            lightDirections[i] = glm::normalize(curScene.lights[i].direction);
            lightColors[i] = curScene.lights[i].color * curScene.lights[i].intensity;
        }

        //TODO: Allow Lua script to set FOV dynamically (it's currently static)
        float FOV = curScene.activeCam->FOV;

        //Rendering the frame goes over 4 passes:
        // 1. Computing depth map
        // 2. Rendering Geometry (and skybox)
        // 3. Bluring Bloom
        // 4. Composite and tone map

        //------ PASS 1 - Shadow map ----------------------
        static mat4 lightProjectionMatrix,lightViewMatrix;

        useShadowMap = false;
        if(useShadowMap && curScene.shadowLight.castShadow){
            //TODO: We can re-use the lightViewMatrix and lightProjectionMatrix if the light doesn't move @performance
            lightProjectionMatrix = glm::ortho(curScene.shadowLight.frustLeft,curScene.shadowLight.frustRight,
                    curScene.shadowLight.frustTop,curScene.shadowLight.frustBot,
                    curScene.shadowLight.frustNear,curScene.shadowLight.frustFar);

            static vec3 lightDir,lightPos,lightUp;
            static float lightDist;
            lightDir = curScene.shadowLight.direction;  //TODO: Should the directional light always follow the user's lookat point?
            lightDist = curScene.shadowLight.distance;
            lightPos = curScene.activeCam->lookatPoint - lightDir*lightDist;
            lightUp = glm::cross(vec3(lightDir.y,lightDir.x,lightDir.z), curScene.activeCam->lookatPoint-lightPos);
            lightViewMatrix = glm::lookAt(lightPos, curScene.activeCam->lookatPoint, lightUp);

            computeShadowDepthMap(lightViewMatrix,lightProjectionMatrix,curScene.toDraw);
        }

        glViewport(0,0,screenWidth,screenHeight); //TODO: Make this more robust when the user switches to fullscreen

        //------ PASS 2 - Main (PBR) Shading Pass --------------------



        mat4 view = glm::lookAt( curScene.activeCam->camPos, //Camera Position
								 curScene.activeCam->lookatPoint, //Point to look at (camPos + camDir)
								 curScene.activeCam->camUp );     //Camera Up direction
        mat4 proj = glm::perspective(FOV * 3.14f/180,screenWidth / (float)screenHeight,nearPlane,farPlane); //FOV, aspect, near, far


		mat4 otherCamView = glm::lookAt(curScene.cameras[MAIN_CAMERA].camPos, //Camera Position
										curScene.cameras[MAIN_CAMERA].lookatPoint, //Point to look at (camPos + camDir)
										curScene.cameras[MAIN_CAMERA].camUp);     //Camera Up direction
		
		//glm::mat4 temp = glm::lookAt(curScene.cameras[MAIN_CAMERA].camPos, //Camera Position
		//	curScene.cameras[MAIN_CAMERA].lookatPoint, //Point to look at (camPos + camDir)
		//	curScene.cameras[MAIN_CAMERA].camUp);
		
		mat4 temp = otherCamView;

		// Zero lookat junk
		temp[3][0] = 0;
		temp[3][1] = 0;
		temp[3][2] = 0;
		// Get rotation of camera wrt world
		mat4 otherCamModel = glm::transpose(temp);
		// setup cam location
		otherCamModel[3][0] = curScene.cameras[MAIN_CAMERA].camPos.x;
		otherCamModel[3][1] = curScene.cameras[MAIN_CAMERA].camPos.y;
		otherCamModel[3][2] = curScene.cameras[MAIN_CAMERA].camPos.z;

		//otherProj = glm::perspective(curScene.cameras[lastActiveCamera].FOV * 3.14f / 180, screenWidth / (float)screenHeight, nearPlane, farPlane); //FOV, aspect, near, far

        //view = lightViewMatrix; proj = lightProjectionMatrix;  //This was useful to visualize the shadowmap

        BindHDRFramebuffer();
        // setPBRShaderUniforms(view,proj,lightViewMatrix,lightProjectionMatrix,useShadowMap);
        // updatePRBShaderSkybox(); //TODO: We only need to do this if the skybox changes

        // Clear the screen to default color
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        g_frustum.UpdatePlanesExtract( proj * otherCamView );

        g_visibleStaticGameObjects.clear();
        Cull( &bvh );
        std::cout << g_visibleStaticGameObjects.size() << std::endl;

        int numDrawn = 0;
        // drawSceneGeometry(curScene.toDraw, view,proj); //Pass 2A: Draw Scene Geometry
        // int numDrawn = drawSceneGeometry( curScene.toDraw, view, proj, otherCamView, otherCamModel, lightViewMatrix, lightProjectionMatrix, useShadowMap );
        if (curScene.currentCam == DEBUG_CAMERA)
        {
            // drawBVH( bvh, view, proj, otherCamView, otherCamModel );
            drawAABBs( curScene.dynamicModels, curScene.staticGameobjects, view, proj, otherCamView, otherCamModel );
        }
        // numDrawn += drawStaticSceneGeometry( curScene.staticGameobjects, view, proj, otherCamView, otherCamModel, lightViewMatrix, lightProjectionMatrix, useShadowMap );
        numDrawn += drawStaticSceneGeometry( g_visibleStaticGameObjects, view, proj, otherCamView, otherCamModel, lightViewMatrix, lightProjectionMatrix, useShadowMap );
        numDrawn += drawSceneGeometry( curScene.dynamicModels, view, proj, otherCamView, otherCamModel, lightViewMatrix, lightProjectionMatrix, useShadowMap );

        //TODO: Add a pass which draws some items without depth culling (e.g. keys, items)
        if(drawColliders) drawColliderGeometry(); //Pass 2B: Draw Colliders
        drawSkybox(view,proj); //Pass 2C: Draw Skybox / Sky color

        //------ PASS 3 - Compute Bloom Blur --------------------
        if(useBloom)
            computeBloomBlur();

        //------ PASS 4 - HDR Tonemap & Composite -------------
        drawCompositeImage(useBloom);

        // == Check for OpenGL Errors ===
        int err = glGetError();
        if(err){
            LOG_F(ERROR,"GL ERROR: %d\n",err);
            exit(1);
        }
        // ==============================

        long long curTime_draw = SDL_GetTicks(); //Time just 3D graphics
        glFlush();
        float drawTime = (curTime_draw-lastTime);

        if(saveOutput) Win2PPM(screenWidth,screenHeight);

        IMGuiNewFrame();

        //ImGui can do some pretty cool things, try some of these:
        //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        //ImGui::ColorEdit3("clear color", (float*)&clear_color);
        //static float f = 0.0f; 
        //ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        //static int counter = 0;
        //if (ImGui::Button("Button")) counter++;

        ImGui::Begin("Frame Info");
        ImGui::Text("Time for Rendering %.0f ms",drawTime);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",1000.0f / ImGui::GetIO().Framerate,ImGui::GetIO().Framerate);
        ImGui::Text("%d Objects in Scene Graph, %d being drawn",numModels, numDrawn );
        ImGui::Text("Total Triangles: %d",totalTriangles);
        ImGui::Text("Camera Pos %f %f %f", curScene.activeCam->camPos.x, curScene.activeCam->camPos.y, curScene.activeCam->camPos.z);
        ImGui::End();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        //LOG_F(3,"Done Drawing");
        swapDisplayBuffers();
    }

    //Clean Up
    cleanupBuffers();

    //Remove controllers
    gamepadCleanup();

    //Cleanup SDL windows
    windowCleanup();

    lua_close(L);
    return 0;
}

void configEngine(string configFile,string configName){
    LOG_SCOPE_FUNCTION(INFO); //Group logging info from this function

    FILE *fp;
    char rawline[1024]; //Assumes no line is longer than 1024 characters!

    // open the file containing the scene description
    fp = fopen(configFile.c_str(),"r");

    LOG_F(INFO,"Loading Config File: %s",configFile.c_str());
    CHECK_NOTNULL_F(fp,"Can't open configuration file '%s'",configFile.c_str());

    bool useThisConfig = true; //a helper variable to make sure we only use elements related to the config chosen by the user
    //Loop through reading each line
    while(fgets(rawline,1024,fp)) { //Assumes no line is longer than 1024 characters!
        string line = string(rawline);
        if(rawline[0] == '#' || rawline[0] == ';'){
            LOG_F(2,"Skipping comment: %s",rawline);
            continue;
        }

        char command[100]; //Assumes no command is longer than 100 chars
        int fieldsRead = sscanf(rawline,"%s ",command); //Read first word in the line (i.e., the command type)
        string commandStr = command;

        if(fieldsRead < 1) { continue; } //No command read = Blank line


        if(commandStr == "["+configName+"]"){ //start using a configuration whenver it matches the passed-in configName
            useThisConfig = true;
        } else if(commandStr.substr(0,1) == "["){ //stop using configuration values whenver you see a no configuration set start (unless it equals the passed-in name)
            useThisConfig = false;
        }

        if(!useThisConfig) { continue; } //Not in the passed-in configuration

        if(commandStr == "useShadowMap"){
            int val;
            sscanf(rawline,"useShadowMap = %d",&val);
            useShadowMap = val;
            LOG_F(1,"Using Shadow map: %s",useShadowMap ? "TRUE" : "FALSE");
        } else if(commandStr == "shadowMapW"){
            float val;
            sscanf(rawline,"shadowMapW = %f",&val);
            shadowMapWidth = val;
            LOG_F(1,"Setting Shadow Map Width to %d",shadowMapWidth);
        } else if(commandStr == "shadowMapH"){
            float val;
            sscanf(rawline,"shadowMapH = %f",&val);
            shadowMapHeight = val;
            LOG_F(1,"Setting Shadow Map Height to %d",shadowMapHeight);
        } else if(commandStr == "screenW"){
            float val;
            sscanf(rawline,"screenW = %f",&val);
            targetScreenWidth = val;
            LOG_F(1,"Setting Screen Width to %d",targetScreenWidth);
        } else if(commandStr == "screenH"){
            float val;
            sscanf(rawline,"screenH = %f",&val);
            targetScreenHeight = val;
            LOG_F(1,"Setting Screen Height to %d",targetScreenHeight);
        } else if(commandStr == "drawColliders"){
            int val;
            sscanf(rawline,"drawColliders = %d",&val);
            drawColliders = val;
            LOG_F(1,"Drawing colliders: %s",drawColliders ? "TRUE" : "FALSE");
        } else if(commandStr == "targetFrameRate"){
            float val;
            sscanf(rawline,"targetFrameRate = %f",&val);
            targetFrameRate = val;
            secondsPerFrame = 1.0f / val;
            LOG_F(1,"Setting Target Framerate to %d",(int)targetFrameRate);
        } else if(commandStr == "nearPlane"){
            float val;
            sscanf(rawline,"nearPlane = %f",&val);
            nearPlane = val;
            LOG_F(1,"Setting Target nearPlane to %f", nearPlane);
        } else if(commandStr == "farPlane"){
            float val;
            sscanf(rawline,"farPlane = %f",&val);
            farPlane = val;
            LOG_F(1,"Setting farPlane to %f", farPlane);
        } else if(commandStr == "startFullscreen"){
            int val;
            sscanf(rawline,"startFullscreen = %d",&val);
            fullscreen = val;
            LOG_F(1,"Starting Fullscreen: %s",fullscreen ? "TRUE" : "FALSE");
        } else if(commandStr == "boarderlessWindow"){
            int val;
            sscanf(rawline,"boarderlessWindow = %d",&val);
            fullscreenMode = val ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
            LOG_F(1,"Boarderless Window for Fullscreen: %s",fullscreenMode ? "TRUE" : "FALSE");
        }
        if(commandStr == "useBloom"){
            int val;
            sscanf(rawline,"useBloom = %d",&val);
            useBloom = val;
            LOG_F(1,"Using Bloom: %s",useBloom ? "TRUE" : "FALSE");
        }
        if(commandStr == "bloomPasses"){
            int val;
            sscanf(rawline,"bloomPasses = %d",&val);
            numBloomPasses = val;
            LOG_F(1,"Number of bloom passes: %d",numBloomPasses);
        }
    }
}



void Win2PPM(int width,int height){
    char outdir[20] = "Screenshots/"; //Must be exist!
    int i,j;
    FILE* fptr;
    static int counter = 0;
    char fname[64];
    unsigned char *image;

    // Allocate our buffer for the image 
    image = (unsigned char *)malloc(3*width*height*sizeof(char));
    if(image == NULL) {
        fprintf(stderr,"ERROR: Failed to allocate memory for image\n");
    }

    // Open the file
    sprintf(fname,"%simage_%04d.ppm",outdir,counter);
    if((fptr = fopen(fname,"w")) == NULL) {
        fprintf(stderr,"ERROR: Failed to open file for window capture\n");
    }

    // Copy the image into our buffer
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,image);

    // Write the PPM file
    fprintf(fptr,"P6\n%d %d\n255\n",width,height);
    for(j=height-1;j>=0;j--) {
        for(i=0;i<width;i++) {
            fputc(image[3*j*width+3*i+0],fptr);
            fputc(image[3*j*width+3*i+1],fptr);
            fputc(image[3*j*width+3*i+2],fptr);
        }
    }

    free(image);
    fclose(fptr);
    counter++;
}
