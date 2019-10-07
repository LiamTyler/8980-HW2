#include "Scene.h"

#include <external/loguru.hpp>
#include <iostream>
#include "Frustum.h"

using std::string;
using glm::vec3;
using glm::mat3;
using glm::mat4;

Scene curScene;
glm::vec3 lightDirections[20];
glm::vec3 lightColors[20];

void resetScene(){
	curScene.lights.clear();
}

void loadScene(string fileName){
	LOG_SCOPE_FUNCTION(INFO);
	std::vector<Light> lightList;

	FILE *fp;
  long length;
  char rawline[1024]; //Assumes no line is longer than 1024 characters!

	LOG_F(INFO,"Opening Scene/Environment File: %s", fileName.c_str());
  fp = fopen(fileName.c_str(), "r"); // open the file containing the scene description

	// check for errors in opening the file
	CHECK_NOTNULL_F(fp,"Can't open Scene/Environment file '%s'", fileName.c_str());
	

	curScene.activeCam = &curScene.cameras[curScene.currentCam];

  fseek(fp, 0, SEEK_END); // move position indicator to the end of the file;
  length = ftell(fp);  // return the value of the current position
  LOG_F(1,"File '%s' is %ld bytes long.",fileName.c_str(),length);
  fseek(fp, 0, SEEK_SET);  // move position indicator to the start of the file
  
	string lightName = "";
  //Loop through reading each line
  while( fgets(rawline,1024,fp) ) { //Assumes no line is longer than 1024 characters!
	  string line = string(rawline);
    if (rawline[0] == '#'){
      LOG_F(2,"Skipping comment: %s", rawline);
      continue;
    }
    
    char command[100];
    int fieldsRead = sscanf(rawline,"%s ",command); //Read first word in the line (i.e., the command type)
    string commandStr = command;
    
    if (fieldsRead < 1){ //No command read = Blank line
     continue;
    }
    
    if (commandStr.substr(0,1) == "["){ // "[" indicates new model
		  int closeBrackets = line.find("]");
			CHECK_F(closeBrackets >= 0,"ERROR: Light name opened with [ but not closed with ]");
      lightName = line.substr(1,closeBrackets-1);
			Light curLight;
			curLight.name = lightName;
			LOG_F(1,"Light Name is: %s",lightName.c_str());
			lightList.push_back(curLight);
    }
		else if (commandStr == "lightDir"){ 
			float dx, dy, dz;
			sscanf(rawline,"lightDir = %f %f %f", &dx, &dy, &dz);
			lightList.back().direction = glm::vec3(dx,dy,dz);
      LOG_F(1,"Setting light direction to: %f %f %f",dx,dy,dz);
    }
		else if (commandStr == "lightDist"){ 
			float dist;
			sscanf(rawline,"lightDist = %f", &dist);
			lightList.back().distance = dist;
      LOG_F(1,"Setting light distance to: %f",dist);
    }
		else if (commandStr == "lightCol"){ 
			float cr, cg, cb;
			sscanf(rawline,"lightCol = %f %f %f", &cr, &cg, &cb);
			lightList.back().color = glm::vec3(cr,cg,cb);
      LOG_F(1,"Setting light color to: %f %f %f",cr,cg,cb);
    }
		else if (commandStr == "lightIntensity"){ 
			float intensity;
			sscanf(rawline,"lightIntensity = %f", &intensity);
			lightList.back().intensity = intensity;
      LOG_F(1,"Setting light intensity to: %f",intensity);
    }
		else if (commandStr == "shadowFrustum"){ 
			float l,r,t,b,n,f;
			sscanf(rawline,"shadowFrustum = %f %f %f %f %f %f", &l, &r, &t, &b, &n, &f);
			lightList.back().frustLeft = l; lightList.back().frustRight = r;
			lightList.back().frustBot = b; lightList.back().frustTop = t;
			lightList.back().frustNear = n; lightList.back().frustFar = f;
      LOG_F(1,"Setting shadow frustum to: %f %f %f %f %f %f",l,r,t,b,n,f);
    }
		else if (commandStr == "shadowBias"){ 
			float bias;
			sscanf(rawline,"shadowBias = %f", &bias);
			lightList.back().shadowBias = bias;
			LOG_F(1,"Shadow bias set to: %f",bias);
    }
		else if (commandStr == "pcfWidth"){ 
			int width;
			sscanf(rawline,"pcfWidth = %d", &width);
			lightList.back().pcfWidth = width;
			LOG_F(1,"PCF Sample Width set to: %d",width);
    }
		else if (commandStr == "lightCastShadow"){ 
			lightList.back().castShadow = true;
			LOG_F(1,"Casting Shadow with current light");
    }
		else if (commandStr == "ambientLight"){ 
			float cr, cg, cb;
			sscanf(rawline,"ambientLight = %f %f %f", &cr, &cg, &cb);
			curScene.ambientLight = glm::vec3(cr,cg,cb);
      LOG_F(1,"Setting ambient light to: %f %f %f",cr,cg,cb);
    }
		else if (commandStr == "skyColor"){ 
			curScene.singleSkyColor = true;
			float r,g,b;
			sscanf(rawline,"skyColor = %f %f %f", &r, &g, &b);
			curScene.skyColor = vec3(r,g,b);
			LOG_F(1,"Setting skybox to a fixed color of: %f %f %f", r, g, b);
    }
		else if (commandStr == "skybox"){ 
			curScene.singleSkyColor = false;
			char envMapName[1024];
			sscanf(rawline,"skybox = %s", envMapName);
			curScene.environmentMap = envMapName;
			LOG_F(1,"Setting environment map to: %s",envMapName);
    }
		else if (commandStr == "skyboxUp"){ 
			float x,y,z;
			sscanf(rawline,"skyboxUp = %f %f %f", &x, &y, &z);
			vec3 direction(x,y,z);
			if (direction.x == 0 && direction.z == 0){
				if (direction.y < 0){ // rotate 180 degrees
					curScene.rotSkybox = mat4(
						          mat3(vec3(-1.0f, 0.0f,  0.0f),
                      vec3( 0.0f, -1.0f, 0.0f),
                      vec3( 0.0f,  0.0f, 1.0f)));
				}
				//else: the rotation matrix should still be the identity
			}
			else{
				vec3 new_y = glm::normalize(direction);
				vec3 new_x = glm::normalize(glm::cross(new_y, vec3(0, 1, 0)));
				vec3 new_z = glm::normalize(glm::cross(new_y, new_x));
				curScene.rotSkybox = mat4(mat3(new_x, new_y, new_z));
			}
			curScene.rotSkybox = glm::inverse(curScene.rotSkybox);
			LOG_F(1,"Setting skybox up to: (%f, %f, %f)",x,y,z);
    }
		else if (commandStr == "CameraFOV"){ 
			float fov;
			sscanf(rawline,"CameraFOV = %f", &fov);
			curScene.activeCam->FOV = fov;
			LOG_F(1,"Camera FOV set to: %f",fov);
    }
	}

	curScene.lights = lightList;
	for (size_t i = 0; i < lightList.size(); i++){
		if (lightList[i].castShadow){
			curScene.shadowLight = lightList[i];
			break; //TODO: There can only be 1 shadow casting light!
		}
	}
}


void BVH::partition(const std::vector<GameObject*>& objs)
{
    // std::cout << "OBJS size: " << objs.size() << std::endl;

    _boundingVolume.max = glm::vec3(-FLT_MAX);
    _boundingVolume.min = glm::vec3(FLT_MAX);
    for ( auto o : objs )
    {
        _boundingVolume.max = glm::max( _boundingVolume.max, o->aabb.max );
        _boundingVolume.min = glm::min( _boundingVolume.min, o->aabb.min );
    }

    if ( objs.size() < 3 )
    {
        _gameObjects = objs;
        return;
    }

    glm::vec3 fullBVHDiff = _boundingVolume.max - _boundingVolume.min;

    // Calculate Split
    int bestAxis   = 0;
    float bestCost = FLT_MAX;
    float bestSplit = 0;
    for ( int axis = 0; axis < 3; ++axis )
    {
        const int NUM_STEPS = 1000;
        float start = _boundingVolume.min[axis];
        float end   = _boundingVolume.max[axis];
        float step  = ( end - start ) / NUM_STEPS;

        for ( float split = start + step; split < end - step; split += step )
        {
            AABB leftAABB  = { glm::vec3( FLT_MAX ), glm::vec3( -FLT_MAX ) };
            AABB rightAABB = { glm::vec3( FLT_MAX ), glm::vec3( -FLT_MAX ) };
            int numLeft    = 0;
            int numRight   = 0;

            for ( auto o : objs )
            {
                glm::vec3 center = ( o->aabb.max + o->aabb.min ) * .5f;
        
                if ( center[axis] > split )
                {
                    ++numRight;
                    rightAABB.max = glm::max(rightAABB.max, o->aabb.max);
                    rightAABB.min = glm::min(rightAABB.min, o->aabb.min);
                }
                else
                {
                    ++numLeft;
                    leftAABB.max = glm::max(leftAABB.max, o->aabb.max);
                    leftAABB.min = glm::min(leftAABB.min, o->aabb.min);
                }
            }

            if (numRight == 0 || numLeft == 0)
            {
                continue;
            }

            // Calculate the cost
            glm::vec3 leftDiff = leftAABB.max - leftAABB.min;
            float LCost = 2 * (leftDiff[0] *    leftDiff[1] +   leftDiff[1] * leftDiff[2] + leftDiff[2] + leftDiff[0]) * numLeft;
            
            glm::vec3 rightDiff = rightAABB.max - rightAABB.min;
            float RCost = 2 * (rightDiff[0] *   rightDiff[1] +  rightDiff[1] * rightDiff[2] + rightDiff[2] + rightDiff[0]) * numRight;

            float cost = LCost + RCost;
            if ( cost < bestCost)
            {
                bestCost = cost;
                bestSplit = split;
                bestAxis = axis;
            }
            
        }
    }

    //glm::vec3 boundingBoxCenter = (_boundingVolume.max + _boundingVolume.min) * .5f;

    // build lists to recurse on
    std::vector<GameObject*> leftList;
    std::vector<GameObject*> rightList;

    for (auto o: objs)
    {
        glm::vec3 center = (o->aabb.max + o->aabb.min) * .5f;
        
        if (center[bestAxis] > bestSplit)
        {
            rightList.push_back(o);
        }
        else
        {
            leftList.push_back(o);
        }
    }

    if (!rightList.empty() && !leftList.empty())
    {
        // std::cout << "BVH num nodes: " << _gameObjects.size() << "\t" << _boundingVolume.min << "\t" << _boundingVolume.max << std::endl;
        _right = new BVH(rightList);
        _left = new BVH(leftList);
    }
    else
    {
        if (!rightList.empty())
        {
            _gameObjects = rightList;
        }
        else
        {
            _gameObjects = leftList;
        }
        // std::cout << "BVH num nodes: " << _gameObjects.size() << "\t" << _boundingVolume.min << "\t" << _boundingVolume.max << std::endl;
    }

}

BVH::BVH(const std::vector<GameObject*>& gameObjs)
{
    _left = nullptr;
    _right = nullptr;
    partition(gameObjs);
}
