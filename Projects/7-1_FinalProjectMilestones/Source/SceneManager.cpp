///////////////////////////////////////////////////////////////////////////////
// shadermanager.cpp
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"
#include "SkyBox.h"
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();

	for (int i = 0; i < 16; i++) {
		m_textureIDs[i].tag = "/0";
		m_textureIDs[i].ID = -1;
	}

	m_loadedTextures = 0;
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}


/***********************************************************
 *  CreateCubemapTexture()
 *
 *  This method is used for loading cubemap textures from image
 * files, configuring mapping parameters, and loading the read
 * texture into the next available textures slot.
 ***********************************************************/

GLuint SceneManager::CreateCubemapTexture(std::vector<std::string> faces)
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(false);

	for (unsigned int i = 0; i < faces.size(); i++) {

		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);

		if (data) {
			//Match upload format to image's channel count (RGB vs RGBA)
			GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format,
				width, height, 0, format, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);
		}
		else {
			std::cout << "Failed to load cubemap face: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	return textureID;   // <-- return it directly, don't store in m_textureIDs
}



/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// add antistropic filtering if supported by hardware (sharpen texture viewed at angle)
		if (glewIsSupported("GL_EXT_texture_filter_anisotropic"))
		{
			GLfloat maxAniso = 0.0f;
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);

			glTexParameterf(
				GL_TEXTURE_2D,
				GL_TEXTURE_MAX_ANISOTROPY_EXT,
				maxAniso
			);
		}

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glDeleteTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationX * rotationY * rotationZ * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}


/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/

/***********************************************************
 *  LoadSceneTextures()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene
 *  rendering
 ***********************************************************/

void SceneManager::LoadSceneTextures()
{

	// Vector to pass to CreateCubemapTexture() that contains the
	// six necessary images.
	std::vector<std::string> skyboxFaces =
	{
		"./textures/skybox/right.png",
		"./textures/skybox/left.png",
		"./textures/skybox/top.png",
		"./textures/skybox/bottom.png",
		"./textures/skybox/front.png",
		"./textures/skybox/back.png"
	};

	// Create the cubemap and hand its texture handle to the skybox object
	GLuint skyboxTex = CreateCubemapTexture(skyboxFaces);
	m_skybox.SetTexture(skyboxTex);
	

	bool bReturn = false;

	bReturn = CreateGLTexture(
		"./textures/floor.png",
		"wood");

	bReturn = CreateGLTexture(
		"./textures/Wall.jpg",
		"wall");

	bReturn = CreateGLTexture(

		"./textures/WallWhite.jpg",
		"ceiling");

	bReturn = CreateGLTexture(

		"./textures/DS2.jpg",
		"DS2");

	bReturn = CreateGLTexture(

		"./textures/Dirt.png",
		"dirt");

	bReturn = CreateGLTexture(

		"./textures/concrete.jpg",
		"concrete");

	bReturn = CreateGLTexture(

		"./textures/Silver.jpg",
		"silver");

	bReturn = CreateGLTexture(

		"./textures/blackWood.jpg",
		"blackWood");

	bReturn = CreateGLTexture(

		"./textures/powdercoat1.png",
		"powderCoat");

	bReturn = CreateGLTexture(

		"./textures/desktop.png",
		"desktop");

	bReturn = CreateGLTexture(

		"./textures/blackGloss.png",
		"blackGloss");


	bReturn = CreateGLTexture(

		"./textures/speaker.png",
		"speaker");
	

	
	BindGLTextures();
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// Initialize and load cubemap textures for skybox
	m_skybox.Init();
	m_skybox.LoadCubeMap();

	

	// load texture files to be applied to 3D objects in scene
	LoadSceneTextures();

	// define materials for object lighting values
	DefineObjectMaterials();

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene
	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadRoundedBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadPrismMesh();
}


/***********************************************************
 *  DefineObjectMaterials()
 *
 *  This method is used for configuring the various material
 *  settings for all of the objects within the 3D scene.
 * 
 *  FIXME: USING ONE OBJECT MATERIAL AS PLACEHOLDER FOR ALL 
 *  OBJECTS UNTIL FUTURE ITERATION.
 *  
 * 
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	OBJECT_MATERIAL woodMaterial;
	woodMaterial.ambientColor = glm::vec3(0.15f);
	woodMaterial.ambientStrength = 0.3f;
	woodMaterial.diffuseColor = glm::vec3(0.9f);
	woodMaterial.specularColor = glm::vec3(0.4f);
	woodMaterial.shininess = 48.0;
	woodMaterial.tag = "wood";

	m_objectMaterials.push_back(woodMaterial);

	OBJECT_MATERIAL powderCoatMaterial;
	powderCoatMaterial.ambientColor = glm::vec3(0.5f);
	powderCoatMaterial.ambientStrength = 0.3f;
	powderCoatMaterial.diffuseColor = glm::vec3(1.6f);
	powderCoatMaterial.specularColor = glm::vec3(0.6f);
	powderCoatMaterial.shininess = 64.0f;
	powderCoatMaterial.tag = "powderCoat";

	m_objectMaterials.push_back(powderCoatMaterial);

	OBJECT_MATERIAL grassMaterial;
	grassMaterial.ambientColor = glm::vec3(0.1f);
	grassMaterial.ambientStrength = 0.1f;
	grassMaterial.diffuseColor = glm::vec3(1.0f);
	grassMaterial.specularColor = glm::vec3(0.1f);
	grassMaterial.shininess = 1.0f;
	grassMaterial.tag = "grass";

	m_objectMaterials.push_back(grassMaterial);

	OBJECT_MATERIAL screenMaterial;
	screenMaterial.ambientColor = glm::vec3(0.1f);
	screenMaterial.ambientStrength = 0.1f;
	screenMaterial.diffuseColor = glm::vec3(1.0f);
	screenMaterial.specularColor = glm::vec3(2.5f);
	screenMaterial.shininess = 256.0f;
	screenMaterial.tag = "screen";

	m_objectMaterials.push_back(screenMaterial);

	OBJECT_MATERIAL glossyPlastic;

	glossyPlastic.ambientColor = glm::vec3(0.15f);
	glossyPlastic.ambientStrength = 0.3f;
	glossyPlastic.diffuseColor = glm::vec3(0.9f);
	glossyPlastic.specularColor = glm::vec3(8.0f);
	glossyPlastic.shininess = 32.0f;
	glossyPlastic.tag = "glossyPlastic";

	m_objectMaterials.push_back(glossyPlastic);

	OBJECT_MATERIAL brushedMetal;
	brushedMetal.ambientColor = glm::vec3(0.15f);
	brushedMetal.ambientStrength = 0.15f;
	brushedMetal.diffuseColor = glm::vec3(1.5f);
	brushedMetal.specularColor = glm::vec3(1.5f);
	brushedMetal.shininess =32.0f;
	brushedMetal.tag = "brushedMetal";

	m_objectMaterials.push_back(brushedMetal);

	OBJECT_MATERIAL paintedWall;

	paintedWall.ambientColor = glm::vec3(0.2f);
	paintedWall.ambientStrength = 0.2f;
	paintedWall.diffuseColor = glm::vec3(1.0f);
	paintedWall.specularColor = glm::vec3(0.4f);
	paintedWall.shininess = 8.0f;
	paintedWall.tag = "paintedWall";

	m_objectMaterials.push_back(paintedWall);

	OBJECT_MATERIAL paintedCeiling;

	paintedCeiling.ambientColor = glm::vec3(0.2f);
	paintedCeiling.ambientStrength = 0.2f;
	paintedCeiling.diffuseColor = glm::vec3(1.0f);
	paintedCeiling.specularColor = glm::vec3(0.01f);
	paintedCeiling.shininess = 0.1f;
	paintedCeiling.tag = "paintedCeiling";

	m_objectMaterials.push_back(paintedCeiling);

}

/***********************************************************
 *  SetupSceneLights()
 *
 *  This method is called to add and configure the light
 *  sources for the 3D scene.  There are up to 4 light sources.
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	// this line of code is NEEDED for telling the shaders to render 
	// the 3D scene with custom lighting, if no light sources have
	// been added then the display window will be black - to use the 
	// default OpenGL lighting then comment out the following line
	//m_pShaderManager->setBoolValue(g_UseLightingName, true);

	/*** STUDENTS - add the code BELOW for setting up light sources ***/
	/*** Up to four light sources can be defined. Refer to the code ***/
	/*** in the OpenGL Sample for help                              ***/

	m_pShaderManager->setBoolValue(g_UseLightingName, true);

	m_pShaderManager->setVec3Value("sunLight.direction", -0.3f, -1.0f, -0.2f);
	m_pShaderManager->setVec3Value("sunLight.ambientColor", 0.08f, 0.08f, 0.08f);
	m_pShaderManager->setVec3Value("sunLight.diffuseColor", 0.50f, 0.53f, 0.48f);
	m_pShaderManager->setVec3Value("sunLight.specularColor", 0.25f, 0.25f, 0.25f);


	m_pShaderManager->setVec3Value("lightSources[0].position", -19.0f, 19.5f, -6.0f);
	m_pShaderManager->setVec3Value("lightSources[0].ambientColor", 0.05f, 0.05f, 0.05f);
	m_pShaderManager->setVec3Value("lightSources[0].diffuseColor", 0.45f, 0.44f, 0.40f);
	m_pShaderManager->setVec3Value("lightSources[0].specularColor", 1.0f, 1.0f, 1.0f);
	m_pShaderManager->setFloatValue("lightSources[0].focalStrength", 24.0f);
	m_pShaderManager->setFloatValue("lightSources[0].specularIntensity", 3.25f);


	m_pShaderManager->setVec3Value("lightSources[1].position", 19.0f, 19.5f, -6.0f);
	m_pShaderManager->setVec3Value("lightSources[1].ambientColor", 0.05f, 0.05f, 0.05f);
	m_pShaderManager->setVec3Value("lightSources[1].diffuseColor", 0.45f, 0.44, 0.40f);
	m_pShaderManager->setVec3Value("lightSources[1].specularColor", 1.0f, 1.0f, 1.0f);
	m_pShaderManager->setFloatValue("lightSources[1].focalStrength",24.0f);
	m_pShaderManager->setFloatValue("lightSources[1].specularIntensity", 3.25f);

	m_pShaderManager->setVec3Value("lightSources[2].position", 0.0f, 19.5f, 6.0f);
	m_pShaderManager->setVec3Value("lightSources[2].ambientColor", 0.05f, 0.05f, 0.05f);
	m_pShaderManager->setVec3Value("lightSources[2].diffuseColor", 0.45f, 0.44f, 0.40f);
	m_pShaderManager->setVec3Value("lightSources[2].specularColor", 1.0f, 1.0f, 1.0f);
	m_pShaderManager->setFloatValue("lightSources[2].focalStrength", 24.0f);
	m_pShaderManager->setFloatValue("lightSources[2].specularIntensity", 3.25f);


	for (int i = 3; i < 4; ++i)
	{
		std::string index = std::to_string(i);
		m_pShaderManager->setVec3Value(("lightSources[" + index + "].ambientColor").c_str(), glm::vec3(0.0f));
		m_pShaderManager->setVec3Value(("lightSources[" + index + "].diffuseColor").c_str(), glm::vec3(0.0f));
		m_pShaderManager->setVec3Value(("lightSources[" + index + "].specularColor").c_str(), glm::vec3(0.0f));
		m_pShaderManager->setFloatValue(("lightSources[" + index + "].focalStrength").c_str(), 1.0f);
		m_pShaderManager->setFloatValue(("lightSources[" + index + "].specularIntensity").c_str(), 0.0f);
	}

}


/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene(glm::mat4 view, glm::mat4 projection)
{
	m_skybox.Draw(view, projection);

	// Skybox uses its own shade so re-activate main shader before drawing rest
	// of the scene
	m_pShaderManager->use();

	// Initialize scene lights
	SetupSceneLights();


	RenderGround();
	RenderBuilding();
	RenderTV();
	RenderSpeakers();
	RenderDesk();


}


/*** RENDER FUNCTIONS **********************************************
/*** Set needed transformations before drawing the basic mesh.  ***/
/*** This same ordering of code should be used for transforming ***/
/*** and drawing all the basic 3D shapes.						***/
/******************************************************************/


void SceneManager::RenderGround()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************
	******* GROUND PLANE
	*******************************************************************
	*/

	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(500.0f, 1.0f, 500.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, -1.0f, 5.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("dirt");
	SetTextureUVScale(25.0f, 25.0f);
	SetShaderMaterial("grass");

	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/
}

void SceneManager::RenderBuilding()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************
	******* FOUNDATION BOX
	*******************************************************************
	*/

	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(39.8f, 1.0f, 29.8f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, -0.523f, 5.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("concrete");
	SetTextureUVScale(5.0f, 1.0f);
	SetShaderMaterial("paintedWall");


	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/

	/******************************************************************
	******* FLOOR PLANE
	*******************************************************************
	*/

	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 1.0f, 15.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.0f, 5.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("wood");
	SetTextureUVScale(2.5f, 5.0f);
	SetShaderMaterial("paintedWall");

	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/

	/******************************************************************
	******* CEILING PLANE
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 1.0f, 15.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 20.0f, 5.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	SetShaderTexture("ceiling");
	SetTextureUVScale(2.0f, 2.0f);
	SetShaderMaterial("paintedCeiling");

	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/

	/******************************************************************
	******* WALL PLANE (Far Wall)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 1.0f, 10.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 10.0f, -10.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("wall");
	SetTextureUVScale(5.0f, 5.0f);
	SetShaderMaterial("paintedWall");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/

	/******************************************************************
	******* WALL PLANE (Left Wall)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(15.0f, 1.0f, 10.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-20.0f, 10.0f, 5.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("wall");
	SetTextureUVScale(5.0f, 5.0f);
	SetShaderMaterial("paintedWall");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/

	/******************************************************************
	******* WALL PLANE (Right Wall)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(15.0f, 1.0f, 10.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(20.0f, 10.0f, 5.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("wall");
	SetTextureUVScale(5.0f, 5.0f);
	SetShaderMaterial("paintedWall");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/
}

void SceneManager::RenderTV()
{

	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************
	******* TV OuterEdge
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(16.0f, 9.0f, 0.25f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 11.0f, -9.85f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderColor(0, 0, 0, 1);
	SetShaderMaterial("glossyPlastic");

	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/

	/******************************************************************
	******* TV Screen
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(7.7f, 1.0f, 4.2f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 11.0f, -9.705f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	SetShaderTexture("DS2");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("screen");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/
}

void SceneManager::RenderSpeakers()
{

	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************
	******* SPEAKER STAND BASE (LEFT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(2.25f, 0.33f, 2.25f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.0f, 0.16f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("blackWood");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawRoundedBoxMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER STAND SHAFT(LEFT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(.66f, 5.5f, .45f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.0f, 0.16f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("silver");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("brushedMetal");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER STAND PLATFORM (LEFT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.50f, 0.16f, 1.50f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.0f, 5.6f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("blackWood");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER BODY RECTANGLE (LEFT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.75f, 3.0f, 1.75f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.0f, 7.15f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderColor(0, 0, 0, 1);
	SetShaderMaterial("glossyPlastic");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


	/******************************************************************
	******* SPEAKER BODY PRISM (LEFT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.75f, 1.0f, 3.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.87f, 7.15f, -6.625f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderColor(0.01, 0.01, 0.01, 1);
	SetShaderMaterial("glossyPlastic");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPrismMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER (LEFT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(.75f, 0.25f, .75f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -31.2f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.0f, 7.6f, -6.33f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("speaker");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER STAND BASE (RIGHT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(2.25f, 0.33f, 2.25f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = -180.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(11.0f, 0.16f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("blackWood");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawRoundedBoxMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER STAND SHAFT(RIGHT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(.66f, 5.5f, .45f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(11.0f, 0.16f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("silver");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("brushedMetal");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER STAND PLATFORM (RIGHT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.50f, 0.16f, 1.50f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(11.0f, 5.6f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("blackWood");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();

	/******************************************************************
	******* SPEAKER BODY RECTANGLE (RIGHT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.75f, 3.0f, 1.75f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(11.0f, 7.15f, -7.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderColor(0, 0, 0, 1);
	SetShaderMaterial("glossyPlastic");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	/****************************************************************/


	/******************************************************************
	******* SPEAKER BODY PRISM (RIGHT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.75f, 1.0f, 3.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 180.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(11.87f, 7.15f, -6.625f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderColor(0.01, 0.01, 0.01, 1);
	SetShaderMaterial("glossyPlastic");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPrismMesh();
	/****************************************************************/

	/******************************************************************
	******* SPEAKER (RIGHT)
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(.75f, 0.25f, .75f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 31.2f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(11.0f, 7.6f, -6.33f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("speaker");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();
	/****************************************************************/





}


void SceneManager::RenderDesk() 
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************
	******* DESK MAIN SURFACE
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(8.0f, 0.15f, 4.5f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 4.75f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("desktop");
	SetTextureUVScale(2.0f, 1.0f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();

	/******************************************************************
	******* DESK UPPER SHELF
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(8.0f, 0.15f, 1.25f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 5.4f, -2.25f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("desktop");
	SetTextureUVScale(1.0f, 0.25f);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();


	/******************************************************************
	******* DESK LEG LEFT LONG
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.25f, 7.75f, 0.125f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = -45.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-4.15f, 2.80f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 8.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();

	/******************************************************************
	******* DESK LEG RIGHT LONG
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.25f, 7.75f, 0.125f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = -45.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(4.15f, 2.80f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 8.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();


	/******************************************************************
	******* DESK LEG LEFT SHORT
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.25f, 6.55f, 0.125f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 45.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-3.8f, 2.35f, -0.50f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 6.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();

	/******************************************************************
	******* DESK LEG RIGHT SHORT
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.25f, 6.55f, 0.125f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 45.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(3.8f, 2.35f, -0.50f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 6.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();

	/******************************************************************
	******* DESK TOP SUPPORT CYLINDER FRONT
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.062f, 7.84f, 0.062f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(3.92f, 4.65f, 1.80f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 6.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();

	/******************************************************************
	******* DESK TOP SUPPORT CYLINDER BACK
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.062f, 8.15f, 0.062f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(4.075f, 4.62f, -1.820f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 6.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();

	/******************************************************************
	******* DESK LEG CYLINDER SUPPORT TOP
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.05f, 7.84f, 0.05f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(3.92f, 2.09f, -0.75f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 6.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();

	/******************************************************************
	******* DESK LEG CYLINDER SUPPORT BOTTOM
	*******************************************************************
	*/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.05f, 7.84f, 0.05f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(3.92f, 0.5f, -2.346f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);


	SetShaderTexture("powderCoat");
	SetTextureUVScale(1.0f, 6.0f);
	SetShaderMaterial("powderCoat");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();

}



