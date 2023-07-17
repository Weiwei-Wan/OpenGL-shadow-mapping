// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <map>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

//glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp> 
#include <glm/gtx/quaternion.hpp>

// Project includes
#include "maths_funcs.h"
#define GLT_IMPLEMENTATION
#include "gltext.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define MESH_TEAPOT "./models/teapot.dae"
#define MESH_BUNNY "./models/stanford-bunny.dae"
#define MESH_SQUARE "./models/square.dae"
#define MESH_BOARD "./models/board.dae"

/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

struct ModelData
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
	std::vector<vec3> mTangents;
	std::vector<vec3> mBitangents;
};

ModelData mesh_teapot;
ModelData mesh_bunny;
ModelData mesh_square;
ModelData mesh_board;

using namespace std;
GLuint SkyBoxID, ShadowDepthID, ShadowMapID, BiasID, PCFID, PCSSID, VarianceID, VSSMID, MSMID, ShadowID;
GLuint brickWallMap;

unsigned int mesh_vao = 0;
int width = 1600;
int height = 1200;
int mode = 1;

glm::mat4 persp_proj = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 1000.0f);
glm::mat4 view;

// Camera pos
GLfloat camera_pos_x = -13.0f;
GLfloat camera_pos_z = 0.0f;
GLfloat camera_pos_y = 0.0f;
GLfloat camera_dir_x = -camera_pos_x;
GLfloat camera_dir_z = -camera_pos_z;
GLfloat camera_dir_y = -camera_pos_y;
GLfloat light_pos_x = 10.0f;
GLfloat light_pos_y = 5.0f;
GLfloat light_pos_z = 0.0f;

GLfloat pitch = 0.0f;
GLfloat roll = 0.0f;
GLfloat yaw = 0.0f;

GLuint loc1, loc2, loc3, loc4, loc5;
GLfloat rotate_x = 0.0f;
GLfloat Delta = 2.0f;

// shadow
GLuint depthMapFBO = 0;
GLuint depthMap;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	const aiScene* scene = aiImportFile(
		file_name,
		aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
	);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				const aiVector3D* vta = &(mesh->mTangents[v_i]);
				modelData.mTangents.push_back(vec3(vta->x, vta->y, vta->z));
				const aiVector3D* vbt = &(mesh->mBitangents[v_i]);
				modelData.mBitangents.push_back(vec3(vbt->x, vbt->y, vbt->z));
			}
		}
	}
	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders(const char* vshadername, const char* fshadername)
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	GLuint shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, vshadername, GL_VERTEX_SHADER);
	AddShader(shaderProgramID, fshadername, GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

unsigned int loadTexture(const char* texture) {
	unsigned int vto = 0;
	glGenTextures(1, &vto);
	int width, height, nrComponents;
	unsigned char* data = stbi_load(texture, &width, &height, &nrComponents, 0);
	GLenum format = GL_RGB;
	if (nrComponents == 1)
		format = GL_RED;
	else if (nrComponents == 3)
		format = GL_RGB;
	else if (nrComponents == 4)
		format = GL_RGBA;
	glBindTexture(GL_TEXTURE_2D, vto);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(data);
	return vto;
}

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS
void generateObjectBufferMesh(GLuint& ID, ModelData mesh_data) {
	loc1 = glGetAttribLocation(ID, "vertex_position");
	loc2 = glGetAttribLocation(ID, "vertex_normal");
	loc3 = glGetAttribLocation(ID, "vertex_texture");
	loc4 = glGetAttribLocation(ID, "aTangent");
	loc5 = glGetAttribLocation(ID, "aBitangent");

	unsigned int vp_vbo = 0;
	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);
	unsigned int vn_vbo = 0;
	glGenBuffers(1, &vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);
	unsigned int vt_vbo = 0;
	glGenBuffers(1, &vt_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vt_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec2), &mesh_data.mTextureCoords[0], GL_STATIC_DRAW);
	unsigned int va_vbo = 0;
	glGenBuffers(1, &va_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, va_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mTangents[0], GL_STATIC_DRAW);
	unsigned int vb_vbo = 0;
	glGenBuffers(1, &vb_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vb_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mBitangents[0], GL_STATIC_DRAW);

	unsigned int vao = 0;
	glBindVertexArray(vao);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc3);
	glBindBuffer(GL_ARRAY_BUFFER, vt_vbo);
	glVertexAttribPointer(loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc4);
	glBindBuffer(GL_ARRAY_BUFFER, va_vbo);
	glVertexAttribPointer(loc4, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc5);
	glBindBuffer(GL_ARRAY_BUFFER, vb_vbo);
	glVertexAttribPointer(loc5, 3, GL_FLOAT, GL_FALSE, 0, NULL);
}

void generateDepthMap() {
	glGenFramebuffers(1, &depthMapFBO);
	glGenTextures(1, &depthMap);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint varianceFBO[2];
GLuint varianceTexture[2];
GLuint depthMapFBO2 = 0;
GLuint depthMap2;
void generateVarianceMap() {
	glGenTextures(1, &depthMap2);
	glBindTexture(GL_TEXTURE_2D, depthMap2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, 1024, 1024, 0, GL_RG, GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthMap2, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenFramebuffers(2, varianceFBO);
	glGenTextures(2, varianceTexture);
	for (int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, varianceFBO[i]);
		glBindTexture(GL_TEXTURE_2D, varianceTexture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, 1024, 1024, 0, GL_RG, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, varianceTexture[i], 0);
	}
}

#pragma endregion VBO_FUNCTIONS

void drawText(const char* str, GLfloat size, glm::vec3 pos) {
	// Initialize glText
	gltInit();
	// Creating text
	GLTtext* text = gltCreateText();
	gltSetText(text, str);
	// Begin text drawing (this for instance calls glUseProgram)
	gltBeginDraw();
	// Draw any amount of text between begin and end
	gltColor(1.0f, 1.0f, 1.0f, 1.0f);
	gltDrawText2DAligned(text, 70 * (pos.x + 1), 450 - pos.y * 70, size, GLT_CENTER, GLT_CENTER);
	// Finish drawing text
	gltEndDraw();
	// Deleting text
	gltDeleteText(text);
	// Destroy glText
	gltTerminate();
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// position        // uv
			-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void displayNormalObject(GLuint& ID, glm::vec3 pos, ModelData mesh_data, GLuint type, float scale) {
	generateObjectBufferMesh(ID, mesh_data);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, pos);
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	if (type == 1) {
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	}
	else if (type == 2) {
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glUniformMatrix4fv(glGetUniformLocation(ID, "model"), 1, GL_FALSE, &model[0][0]);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);
}

void display() {
	//rotate_x += Delta;
	//camera_pos_x = 10.0f * cos(glm::radians(rotate_x));
	//camera_pos_z = 10.0f * sin(glm::radians(rotate_x));

	view = glm::lookAt(glm::vec3(camera_pos_x, camera_pos_y, camera_pos_z), // Camera is at (x,y,z), in World Space
		   glm::vec3(0, 0, 0), // and looks at the origin 
		   glm::vec3(0, 1, 0));  // Head is up (set to 0,-1,0 to look upside-down)
	glm::mat4 lightProjection = glm::ortho(-15.0f, 25.0f, -8.0f, 8.0f, -15.0f, 25.0f);
	glm::mat4 lightView = glm::lookAt(glm::vec3(light_pos_x, light_pos_y, light_pos_z), glm::vec3(0.0f), glm::vec3(1.0));
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	if (mode != 5) {
		// 1. get depth map
		glViewport(0, 0, 1024, 1024);
		glUseProgram(ShadowDepthID);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

		glUniformMatrix4fv(glGetUniformLocation(ShadowDepthID, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);
		displayNormalObject(ShadowDepthID, glm::vec3(0.0f, -0.5f, 2.5f), mesh_teapot, 1, 0.2f);
		displayNormalObject(ShadowDepthID, glm::vec3(0.0f, -1.0f, -3.0f), mesh_bunny, 2, 1.0f);
		displayNormalObject(ShadowDepthID, glm::vec3(-3.0f, -1.0f, 0.0f), mesh_teapot, 1, 0.1f);
		displayNormalObject(ShadowDepthID, glm::vec3(-5.0f, -2.0f, 0.0f), mesh_board, 2, 0.2f);
	}
	else {
		glViewport(0, 0, 1024, 1024);
		glUseProgram(ShadowDepthID);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO2);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

		glUniformMatrix4fv(glGetUniformLocation(ShadowDepthID, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);
		displayNormalObject(ShadowDepthID, glm::vec3(0.0f, -0.5f, 2.5f), mesh_teapot, 1, 0.2f);
		displayNormalObject(ShadowDepthID, glm::vec3(0.0f, -1.0f, -3.0f), mesh_bunny, 2, 1.0f);
		displayNormalObject(ShadowDepthID, glm::vec3(-3.0f, -1.0f, 0.0f), mesh_teapot, 1, 0.1f);
		//displayNormalObject(ShadowDepthID, glm::vec3(-5.0f, -1.0f, -3.0f), mesh_bunny, 2, 0.5f);
		displayNormalObject(ShadowDepthID, glm::vec3(-5.0f, -2.0f, 0.0f), mesh_board, 2, 0.2f);

		// calculate the average value
		glBindFramebuffer(GL_FRAMEBUFFER, varianceFBO[0]);
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depthMap2);
		glUseProgram(VarianceID);
		glUniform1f(glGetUniformLocation(VarianceID, "vertical"), 0.0f);
		renderQuad();

		glBindFramebuffer(GL_FRAMEBUFFER, varianceFBO[1]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, varianceTexture[0]);
		glUniform1f(glGetUniformLocation(VarianceID, "vertical"), 1.0f);
		renderQuad();
	}

	// 2. render scene
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, 1600, 1200);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(ShadowID);
	//lightSpaceMatrix = lightProjection * view;
	glUniformMatrix4fv(glGetUniformLocation(ShadowID, "proj"), 1, GL_FALSE, &persp_proj[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(ShadowID, "view"), 1, GL_FALSE, &view[0][0]);
	glUniform3f(glGetUniformLocation(ShadowID, "viewPos"), camera_pos_x, camera_pos_y, camera_pos_z);
	glUniform3f(glGetUniformLocation(ShadowID, "lightPos"), light_pos_x, light_pos_y, light_pos_z);
	glUniformMatrix4fv(glGetUniformLocation(ShadowID, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);

	glActiveTexture(GL_TEXTURE0);
	if (mode == 5) { glBindTexture(GL_TEXTURE_2D, varianceTexture[1]); }
	else { glBindTexture(GL_TEXTURE_2D, depthMap); }
	
	displayNormalObject(ShadowID, glm::vec3(0.0f, -0.5f, 2.5f), mesh_teapot, 1, 0.2f);
	displayNormalObject(ShadowID, glm::vec3(0.0f, -1.0f, -3.0f), mesh_bunny, 2, 1.0f);
	displayNormalObject(ShadowID, glm::vec3(-3.0f, -1.0f, 0.0f), mesh_teapot, 1, 0.1f);
	//displayNormalObject(ShadowID, glm::vec3(-5.0f, -1.0f, -3.0f), mesh_bunny, 2, 0.5f);
	displayNormalObject(ShadowID, glm::vec3(-5.0f, -2.0f, 0.0f), mesh_board, 2, 0.2f);

	if (mode == 1) { drawText("Basic Shadow", 5, glm::vec3(11.0f, 4.0f, 0.0f)); }
	else if (mode == 2) { drawText("Basic Shadow with bias", 5, glm::vec3(10.0f, 4.0f, 0.0f)); }
	else if (mode == 3) { drawText("PCF Shadow", 5, glm::vec3(11.0f, 4.0f, 0.0f)); }
	else if (mode == 4) { drawText("PCSS Shadow", 5, glm::vec3(11.0f, 4.0f, 0.0f)); }
	else if (mode == 5) { drawText("VSSM Shadow", 5, glm::vec3(11.0f, 4.0f, 0.0f)); }
	else if (mode == 6) { drawText("MSM Shadow", 5, glm::vec3(11.0f, 4.0f, 0.0f)); }
	glutPostRedisplay();
	glutSwapBuffers();
}

void init()
{
	mesh_teapot = load_mesh(MESH_TEAPOT);
	mesh_bunny = load_mesh(MESH_BUNNY);
	mesh_square = load_mesh(MESH_SQUARE);
	mesh_board = load_mesh(MESH_BOARD);
	brickWallMap = loadTexture("./textures/brickwall.jpg");
	SkyBoxID = CompileShaders("./shaders/skyboxVertexShader.txt", "./shaders/skyboxFragmentShader.txt");
	ShadowDepthID = CompileShaders("./shaders/shadowDepthVertexShader.txt", "./shaders/shadowDepthFragmentShader.txt");
	ShadowMapID = CompileShaders("./shaders/shadowVertexShader.txt", "./shaders/shadowFragmentShader.txt");
	BiasID = CompileShaders("./shaders/shadowVertexShader.txt", "./shaders/shadowBiasFragmentShader.txt");
	PCFID = CompileShaders("./shaders/shadowVertexShader.txt", "./shaders/shadowPCFFragmentShader.txt");
	PCSSID = CompileShaders("./shaders/shadowVertexShader.txt", "./shaders/shadowPCSSFragmentShader.txt");
	VarianceID = CompileShaders("./shaders/shadowDD2VertexShader.txt", "./shaders/shadowDD2FragmentShader.txt");
	VSSMID = CompileShaders("./shaders/shadowVertexShader.txt", "./shaders/shadowVSSMFragmentShader.txt");
	MSMID = CompileShaders("./shaders/shadowVertexShader.txt", "./shaders/shadowMSMFragmentShader.txt");
	ShadowID = ShadowMapID;
	generateDepthMap();
	generateVarianceMap();
}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	if (key == '1') {
		ShadowID = ShadowMapID;
		mode = 1;
	}
	else if (key == '2') {
		ShadowID = BiasID;
		mode = 2;
	}
	else if (key == '3') {
		ShadowID = PCFID;
		mode = 3;
	}
	else if (key == '4') {
		ShadowID = PCSSID;
		mode = 4;
	}
	else if (key == '5') {
		ShadowID = VSSMID;
		mode = 5;
	}
	else if (key == '6') {
		ShadowID = MSMID;
		mode = 6;
	}
}

void mousePress(int button, int state, int xpos, int ypos) {
	// Wheel reports as button 3(scroll up) and button 4(scroll down)
	if (button == 3) // It's a wheel event
	{
		// Each wheel event reports like a button click, GLUT_DOWN then GLUT_UP
		if (state == GLUT_UP) return; // Disregard redundant GLUT_UP events
		printf("Scroll %s At %d %d\n", (button == 3) ? "Up" : "Down", xpos, ypos);
		camera_pos_x += 1.0f;
	}
	else if (button == 4)
	{
		if (state == GLUT_UP) return; // Disregard redundant GLUT_UP events
		printf("Scroll %s At %d %d\n", (button == 3) ? "Up" : "Down", xpos, ypos);
		camera_pos_x -= 1.0f;
	}
	else if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {  // normal button event
		yaw += (xpos - float(width) / 2.0) / width;
		yaw = glm::mod(yaw + 180.0f, 360.0f) - 180.0f;
		pitch -= (ypos - float(height) / 2.0) / height;
		pitch = glm::clamp(pitch, -89.0f, 89.0f);
		//glutWarpPointer(width / 2.0, height / 2.0);	
		camera_dir_x = cos(pitch) * sin(yaw);
		camera_dir_y = sin(pitch);
		camera_dir_z = -cos(pitch) * cos(yaw);
	}
}

int main(int argc, char** argv) {
	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("shadow");

	//texure
	glEnable(GL_DEPTH_TEST);

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutKeyboardFunc(keypress);
	glutMouseFunc(mousePress);
	//glutMotionFunc(mouseMotion);

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}