--Simple Example
print("Starting Lua for Simple Example")

--Todo:
-- Lua modules (for better organization, and maybe reloading?)

CameraPosX = 0.0
CameraPosY = 1.0
CameraPosZ = 5.0

CameraDirX = 0.0
CameraDirY = 0.0
CameraDirZ = -1.0

CameraUpX = 0.0
CameraUpY = 1.0
CameraUpZ = 0.0

animatedModels = {}
velModel = {}
rotYVelModel = {}

function frameUpdate(dt)
    frameDT = dt
end

theta = 90 * 3.1415925635 / 180
function keyHandler(keys)
    if keys.left then
        theta = theta + 1 *frameDT
    end
    if keys.right then
        theta = theta - 1 *frameDT
    end
    if keys.up then
        CameraPosX = CameraPosX + 5 * frameDT * CameraDirX
        CameraPosZ = CameraPosZ + 5 * frameDT * CameraDirZ
    end
    if keys.down then
        CameraPosX = CameraPosX - 5 * frameDT * CameraDirX
        CameraPosZ = CameraPosZ - 5 * frameDT * CameraDirZ
    end
    CameraDirX = math.cos( theta )
    CameraDirZ = -math.sin( theta )
end

id = addModel("Chalet",0,2,0)
--setModelMaterial(id,"Shiny Red Plastic")
--setModelMaterial(id,"Steel")
--animatedModels[id] = true
--rotYVelModel[id] = 1

numR = 50
numC = 20
for r=0,numR do
    for c=0,numC do
        id = addModel("Teapot", -numR / 2 + 1*r, 0, -numC/2 + 1*c )
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

--id = addModel("FloorPart",0,0,0)
--placeModel(id,0,-.02,0)
--scaleModel(id,3,1,3)
--setModelMaterial(id,"Gold")
--piller = addModel("Dino",0,0,-.15)  --VeryFancyCube


--placeModel(piller,-1.5,1.5,0.5)
--scaleModel(piller,.5,0.5,1.5)
--animatedModels[piller] = true
--rotZVelModel[piller] = 1
