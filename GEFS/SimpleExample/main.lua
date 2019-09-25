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

CameraVelX = 0.0
CameraVelY = 0.0
CameraVelZ = 0.0

animatedModels = {}
velModel = {}
rotYVelModel = {}

function frameUpdate(dt)
  frameDT = dt
end

theta = 0
function keyHandler(keys)
  if keys.left then
    theta = theta + 1 *frameDT
  end
  if keys.right then
    theta = theta - 1 *frameDT
  end
  if keys.up then
    CameraPosX = CameraPosX + 1 * frameDT * CameraDirX
    CameraPosZ = CameraPosZ + 1 * frameDT * CameraDirZ
  end
  if keys.down then
    CameraPosX = CameraPosX - 1 * frameDT * CameraDirX
    CameraPosZ = CameraPosZ - 1 * frameDT * CameraDirZ
  end
  CameraDirX = math.cos( theta )
  CameraDirZ = -math.sin( theta )
end

--id = addModel("Teapot",0,0,0)
--setModelMaterial(id,"Shiny Red Plastic")
--setModelMaterial(id,"Steel")
--animatedModels[id] = true
--rotYVelModel[id] = 1

for r=0,10 do
    for c=0,10 do
        id = addModel("Teapot",-30 + 3*r, 0, -30 + 3*c)
    end
end

--id = addModel("Teapot2",0,0,0)
--setModelMaterial(id,"Gold")
--animatedModels[id] = true
--rotYVelModel[id] = 1

--id = addModel("Chalet",0,0,0)
--setModelMaterial(id,"Gold")
--animatedModels[id] = true
--rotYVelModel[id] = 1

id = addModel("FloorPart",0,0,0)
placeModel(id,0,-.02,0)
scaleModel(id,3,1,3)
setModelMaterial(id,"Gold")
piller = addModel("Dino",0,0,-.15)  --VeryFancyCube


--placeModel(piller,-1.5,1.5,0.5)
--scaleModel(piller,.5,0.5,1.5)
--animatedModels[piller] = true
--rotZVelModel[piller] = 1
