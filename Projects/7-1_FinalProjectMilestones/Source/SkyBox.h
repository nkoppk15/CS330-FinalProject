#include "shapemeshes.h"
#include "shadermanager.h"

#pragma once
class SkyBox
{

private:
	// stores the GL data relative to a given mesh
	struct GLMesh
	{
		GLuint vao;         // Handle for the vertex array object
		GLuint vbos[2];     // Handles for the vertex buffer objects
		GLuint nVertices;	// Number of vertices for the mesh
		GLuint nIndices;    // Number of indices for the mesh
		GLuint ebo;         // Element Array Buffer
	};

	GLuint m_CubeMapTexture;
	ShaderManager m_skyboxShader;
	GLMesh m_CubeMapMesh;


	public:

		void Init();

		void SetTexture(GLuint textureID);

		void LoadCubeMap();

		void Draw(glm::mat4 view, glm::mat4 projection);

};

