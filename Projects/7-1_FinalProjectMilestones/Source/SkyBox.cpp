#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SkyBox.h"

///////////////////////////////////////////////////
//	Init()
//
//	Initialize custom shaders made for the skybox.
// 
///////////////////////////////////////////////////
void SkyBox::Init() {
	m_skyboxShader.LoadShaders(
		"skyboxVert.glsl",
		"skyboxFrag.glsl"
	);
}

///////////////////////////////////////////////////
//	SetTexture()
//
//	Setter to map texture to the cubemap using textureID, similarly
//  to how normal textures bind.
// 
///////////////////////////////////////////////////
void SkyBox::SetTexture(GLuint textureID) {
	m_CubeMapTexture = textureID;
}

///////////////////////////////////////////////////
//	LoadCubeMap()
//
//	Create a box mesh by specifying the vertices and 
//  store it in a VAO/VBO.  The normals and texture
//  coordinates are also set.
//
//	Correct triangle drawing command:
//
//	glDrawElements(GL_TRIANGLES, meshes.gBoxMesh.nIndices, GL_UNSIGNED_INT, (void*)0);
///////////////////////////////////////////////////
void SkyBox::LoadCubeMap() {

	GLfloat verts[] = {
		-1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f
	};

	GLuint indices[] = {
		1, 2, 6,
		6, 5, 1,

		0, 4, 7,
		7, 3, 0,

		4, 5, 6,
		6, 7, 4,

		0, 3, 2,
		2, 1, 0,

		0, 1, 5,
		5, 4, 0,

		3, 7, 6,
		6, 2, 3
	};

	m_CubeMapMesh.nIndices = sizeof(indices) / sizeof(GLuint);
	
	glGenVertexArrays(1, &m_CubeMapMesh.vao);
	glGenBuffers(1, &m_CubeMapMesh.vbos[0]);
	glGenBuffers(1, &m_CubeMapMesh.ebo);

	glBindVertexArray(m_CubeMapMesh.vao);

	// Vertex Buffer
	glBindBuffer(GL_ARRAY_BUFFER, m_CubeMapMesh.vbos[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	// Element Buffer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_CubeMapMesh.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// Position attribute only
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);

}

///////////////////////////////////////////////////
//	Draw()
//
//	Use view and projection matrices passed from view manager to draw
//  the cubemap array and setup a no translation view so skybox always
//  seems the same distance away.
// 
///////////////////////////////////////////////////

void SkyBox::Draw(glm::mat4 view, glm::mat4 projection) {

	// Use LEQUAL so skybox (drawn at max depth) passes depth test
	glDepthFunc(GL_LEQUAL);

	m_skyboxShader.use();
	m_skyboxShader.setIntValue("skybox", 0);

	// Strip translation from view matrix so skybox follows the
	// camera's rotation but not its position
	glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
	m_skyboxShader.setMat4Value("view", viewNoTranslation);
	m_skyboxShader.setMat4Value("projection", projection);

	glBindVertexArray(m_CubeMapMesh.vao);

	// Bind cubemap to texture unit 0 for sampling
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_CubeMapTexture);

	glDrawElements(GL_TRIANGLES, m_CubeMapMesh.nIndices, GL_UNSIGNED_INT, 0);

	glBindVertexArray(0);
	// Restore default depth function for rest of the scene
	glDepthFunc(GL_LESS);
}

