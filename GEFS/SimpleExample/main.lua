--Simple Example
print("Starting Lua for Simple Example")

--Todo:
-- Lua modules (for better organization, and maybe reloading?)

CameraPosX = -3.0
CameraPosY = 1.0
CameraPosZ = 0.0

CameraDirX = 1.0
CameraDirY = -0.0
CameraDirZ = -0.0

CameraUpX = 0.0
CameraUpY = 1.0
CameraUpZ = 0.0

animatedModels = {}
velModel = {}
rotYVelModel = {}

function frameUpdate(dt)
  for modelID,v in pairs(animatedModels) do
    --print("ID",modelID)
    local vel = velModel[modelID]
    if vel then 
      translateModel(modelID,dt*vel[1],dt*vel[2],dt*vel[3])
    end

    local rotYvel = rotYVelModel[modelID]
    if rotYvel then 
      rotateModel(modelID,rotYvel*dt, 0, 1, 0)
    end

  end
end

function keyHandler(keys)
  if keys.left then
    translateModel(piller,0,0,-0.1)
  end
  if keys.right then
    translateModel(piller,0,0,0.1)
  end
  if keys.up then
    translateModel(piller,0.1,0,0)
  end
  if keys.down then
    translateModel(piller,-0.1,0,0)
  end
end

id = addModel("Teapot",0,0,0)
setModelMaterial(id,"Shiny Red Plastic")
--setModelMaterial(id,"Steel")
animatedModels[id] = true
rotYVelModel[id] = 1

id = addModel("Teapot2",0,0,0)
setModelMaterial(id,"Gold")
animatedModels[id] = true
rotYVelModel[id] = 1

id = addModel("Chalet",0,0,0)
--setModelMaterial(id,"Gold")
animatedModels[id] = true
rotYVelModel[id] = 1

id = addModel("FloorPart",0,0,0)
placeModel(id,0,-.02,0)
scaleModel(id,3,1,3)
setModelMaterial(id,"Gold")
piller = addModel("Dino",0,0,-.15)  --VeryFancyCube


--placeModel(piller,-1.5,1.5,0.5)
--scaleModel(piller,.5,0.5,1.5)
--animatedModels[piller] = true
--rotZVelModel[piller] = 1
