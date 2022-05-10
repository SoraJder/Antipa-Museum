#include <stdlib.h> 
#include <stdio.h>
#include <math.h> 

#include <GL/glew.h>

#define GLM_FORCE_CTOR_INIT 
#include <GLM.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "OBJ_Loader.h"
#pragma comment (lib, "glfw3dll.lib")
#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "OpenGL32.lib")

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

glm::vec3 lightPos(140.0f, 100.0f, -40.0f);

objl::Loader Loader;
enum ECameraMovementType
{
	UNKNOWN,
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

class Camera
{
private:
	// Default camera values
	const float zNEAR = 0.1f;
	const float zFAR = 1000.f;
	const float YAW = -90.0f;
	const float PITCH = 0.0f;
	const float FOV = 45.0f;
	glm::vec3 startPosition;

public:
	Camera(const int width, const int height, const glm::vec3& position)
	{
		startPosition = position;
		Set(width, height, position);
	}

	void Set(const int width, const int height, const glm::vec3& position)
	{
		this->isPerspective = true;
		this->yaw = YAW;
		this->pitch = PITCH;

		this->FoVy = FOV;
		this->width = width;
		this->height = height;
		this->zNear = zNEAR;
		this->zFar = zFAR;

		this->worldUp = glm::vec3(0, 1, 0);
		this->position = position;

		lastX = width / 2.0f;
		lastY = height / 2.0f;
		bFirstMouseMove = true;

		UpdateCameraVectors();
	}

	void Reset(const int width, const int height)
	{
		Set(width, height, startPosition);
	}

	void Reshape(int windowWidth, int windowHeight)
	{
		width = windowWidth;
		height = windowHeight;

		// define the viewport transformation
		glViewport(0, 0, windowWidth, windowHeight);
	}

	const glm::vec3 GetPosition() const
	{
		return position;
	}

	const glm::mat4 GetViewMatrix() const
	{
		// Returns the View Matrix
		return glm::lookAt(position, position + forward, up);
	}

	const glm::mat4 GetProjectionMatrix() const
	{
		glm::mat4 Proj = glm::mat4(1);
		if (isPerspective) {
			float aspectRatio = ((float)(width)) / height;
			Proj = glm::perspective(glm::radians(FoVy), aspectRatio, zNear, zFar);
		}
		else {
			float scaleFactor = 2000.f;
			Proj = glm::ortho<float>(
				-width / scaleFactor, width / scaleFactor,
				-height / scaleFactor, height / scaleFactor, -zFar, zFar);
		}
		return Proj;
	}

	void ProcessKeyboard(ECameraMovementType direction, float deltaTime)
	{
		float velocity = (float)(cameraSpeedFactor * deltaTime);
		switch (direction) {
		case ECameraMovementType::FORWARD:
			position += forward * velocity;
			break;
		case ECameraMovementType::BACKWARD:
			position -= forward * velocity;
			break;
		case ECameraMovementType::LEFT:
			position -= right * velocity;
			break;
		case ECameraMovementType::RIGHT:
			position += right * velocity;
			break;
		case ECameraMovementType::UP:
			position += up * velocity;
			break;
		case ECameraMovementType::DOWN:
			position -= up * velocity;
			break;
		}
	}

	void MouseControl(float xPos, float yPos)
	{
		if (bFirstMouseMove) {
			lastX = xPos;
			lastY = yPos;
			bFirstMouseMove = false;
		}

		float xChange = xPos - lastX;
		float yChange = lastY - yPos;
		lastX = xPos;
		lastY = yPos;

		if (fabs(xChange) <= 1e-6 && fabs(yChange) <= 1e-6) {
			return;
		}
		xChange *= mouseSensitivity;
		yChange *= mouseSensitivity;

		ProcessMouseMovement(xChange, yChange);
	}

	void ProcessMouseScroll(float yOffset)
	{
		if (FoVy >= 1.0f && FoVy <= 90.0f) {
			FoVy -= yOffset;
		}
		if (FoVy <= 1.0f)
			FoVy = 1.0f;
		if (FoVy >= 90.0f)
			FoVy = 90.0f;
	}

private:
	void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true)
	{
		yaw += xOffset;
		pitch += yOffset;

		//std::cout << "yaw = " << yaw << std::endl;
		//std::cout << "pitch = " << pitch << std::endl;

		// Avem grijã sã nu ne dãm peste cap
		if (constrainPitch) {
			if (pitch > 89.0f)
				pitch = 89.0f;
			if (pitch < -89.0f)
				pitch = -89.0f;
		}

		// Se modificã vectorii camerei pe baza unghiurilor Euler
		UpdateCameraVectors();
	}

	void UpdateCameraVectors()
	{
		// Calculate the new forward vector
		this->forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		this->forward.y = sin(glm::radians(pitch));
		this->forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		this->forward = glm::normalize(this->forward);
		// Also re-calculate the Right and Up vector
		right = glm::normalize(glm::cross(forward, worldUp));  // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		up = glm::normalize(glm::cross(right, forward));
	}

protected:
	const float cameraSpeedFactor = 90.5f;
	const float mouseSensitivity = 0.1f;

	// Perspective properties
	float zNear;
	float zFar;
	float FoVy;
	int width;
	int height;
	bool isPerspective;

	glm::vec3 position;
	glm::vec3 forward;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 worldUp;

	// Euler Angles
	float yaw;
	float pitch;

	bool bFirstMouseMove = true;
	float lastX = 0.f, lastY = 0.f;
};

class Shader
{
public:
	// constructor generates the shaderStencilTesting on the fly
	// ------------------------------------------------------------------------
	Shader(const char* vertexPath, const char* fragmentPath)
	{
		Init(vertexPath, fragmentPath);
	}

	~Shader()
	{
		glDeleteProgram(ID);
	}

	// activate the shaderStencilTesting
	// ------------------------------------------------------------------------
	void Use() const
	{
		glUseProgram(ID);
	}

	unsigned int GetID() const { return ID; }

	// MVP
	unsigned int loc_model_matrix;
	unsigned int loc_view_matrix;
	unsigned int loc_projection_matrix;

	// utility uniform functions
	void SetInt(const std::string& name, int value) const
	{
		glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
	}
	void SetFloat(const std::string& name, float value) const
	{
		glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	}
	void SetVec3(const std::string& name, const glm::vec3& value) const
	{
		glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
	}
	void SetVec3(const std::string& name, float x, float y, float z) const
	{
		glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
	}
	void SetMat4(const std::string& name, const glm::mat4& mat) const
	{
		glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}

private:
	void Init(const char* vertexPath, const char* fragmentPath)
	{
		// 1. retrieve the vertex/fragment source code from filePath
		std::string vertexCode;
		std::string fragmentCode;
		std::ifstream vShaderFile;
		std::ifstream fShaderFile;
		// ensure ifstream objects can throw exceptions:
		vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try {
			// open files
			vShaderFile.open(vertexPath);
			fShaderFile.open(fragmentPath);
			std::stringstream vShaderStream, fShaderStream;
			// read file's buffer contents into streams
			vShaderStream << vShaderFile.rdbuf();
			fShaderStream << fShaderFile.rdbuf();
			// close file handlers
			vShaderFile.close();
			fShaderFile.close();
			// convert stream into string
			vertexCode = vShaderStream.str();
			fragmentCode = fShaderStream.str();
		}
		catch (std::ifstream::failure e) {
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
		}
		const char* vShaderCode = vertexCode.c_str();
		const char* fShaderCode = fragmentCode.c_str();

		// 2. compile shaders
		unsigned int vertex, fragment;
		// vertex shaderStencilTesting
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vShaderCode, NULL);
		glCompileShader(vertex);
		CheckCompileErrors(vertex, "VERTEX");
		// fragment Shader
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShaderCode, NULL);
		glCompileShader(fragment);
		CheckCompileErrors(fragment, "FRAGMENT");
		// shaderStencilTesting Program
		ID = glCreateProgram();
		glAttachShader(ID, vertex);
		glAttachShader(ID, fragment);
		glLinkProgram(ID);
		CheckCompileErrors(ID, "PROGRAM");

		// 3. delete the shaders as they're linked into our program now and no longer necessery
		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}

	// utility function for checking shaderStencilTesting compilation/linking errors.
	// ------------------------------------------------------------------------
	void CheckCompileErrors(unsigned int shaderStencilTesting, std::string type)
	{
		GLint success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shaderStencilTesting, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shaderStencilTesting, 1024, NULL, infoLog);
				std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
		else {
			glGetProgramiv(shaderStencilTesting, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shaderStencilTesting, 1024, NULL, infoLog);
				std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
	}
private:
	unsigned int ID;
};

Camera* pCamera = nullptr;

unsigned int CreateTexture(const std::string& strTexturePath)
{
	unsigned int textureId = -1;

	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
	unsigned char* data = stbi_load(strTexturePath.c_str(), &width, &height, &nrChannels, 0);
	if (data) {
		GLenum format;
		if (nrChannels == 1)
			format = GL_RED;
		else if (nrChannels == 3)
			format = GL_RGB;
		else if (nrChannels == 4)
			format = GL_RGBA;

		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else {
		std::cout << "Failed to load texture: " << strTexturePath << std::endl;
	}
	stbi_image_free(data);

	return textureId;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

//textures
void renderScene(const Shader& shader);
void renderStegosaurus(const Shader& shader);
void renderVelociraptor(const Shader& shader);
void renderGrizzly(const Shader& shader);
void renderPtero(const Shader& shader,glm::vec3&light);
void renderCuteDino(const Shader& shader);
void renderRoom();


//objects
void renderStegosaurus();
void renderCuteDino();
void renderVelociraptorBody();
void renderVelociraptorEyes();
void renderVelociraptorLowerJaw();
void renderVelociraptorClaws();
void renderVelociraptorUpperJaw();
void renderGrizzly();
void renderGrizzlyFace();
void renderGrizzlyEyes();
void renderPtero();


// timing
double deltaTime = 0.0f;    // time between current frame and last frame
double lastFrame = 0.0f;


int main(int argc, char** argv)
{
	std::string strFullExeFileName = argv[0];
	std::string strExePath;
	const size_t last_slash_idx = strFullExeFileName.rfind('\\');
	if (std::string::npos != last_slash_idx) {
		strExePath = strFullExeFileName.substr(0, last_slash_idx);
	}

	// glfw: initialize and configure
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// glfw window creation
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Explorarea muzeului Antipa", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glewInit();



	// Create camera
	pCamera = new Camera(SCR_WIDTH, SCR_HEIGHT, glm::vec3(0.0, 40.0, 255.0));

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);

	// build and compile shaders
	// -------------------------
	Shader shadowMappingShader("ShadowMapping.vs", "ShadowMapping.fs");
	Shader shadowMappingDepthShader("ShadowMappingDepth.vs", "ShadowMappingDepth.fs");

	// load textures
	// -------------
	unsigned int roomTexture = CreateTexture(strExePath + "\\Bricks.jpg");
	unsigned int stegosaurusTexture = CreateTexture(strExePath + "\\stegosaurusSkin.jpg");
	unsigned int grizzlyTexture = CreateTexture(strExePath + "\\GrizzlyDiffuse.png");
	unsigned int pteroTexture = CreateTexture(strExePath + "\\pteroSkin.jpg");
	unsigned int veloTexture = CreateTexture(strExePath + "\\velociraptorSkin.jpg");
	unsigned int cuteDinoTexture = CreateTexture(strExePath + "\\cuteDino.jpg");

	// configure depth map FBO
	// -----------------------
	const unsigned int SHADOW_WIDTH = 4098, SHADOW_HEIGHT = 4098;
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create depth texture
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	// attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// shader configuration
	// --------------------
	shadowMappingShader.Use();
	shadowMappingShader.SetInt("diffuseTexture", 0);
	shadowMappingShader.SetInt("shadowMap", 1);

	glEnable(GL_CULL_FACE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		// render
		// ------
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		static float fRadius = 10.f;
		lightPos.y = fRadius * std::cos(currentFrame);

		// 1. render depth of scene to texture (from light's perspective)
		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;
		float near_plane = 1.0f, far_plane = 7.5f;
		lightProjection = glm::ortho(10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
		lightSpaceMatrix = lightProjection * lightView;

		// render scene from light's point of view
		shadowMappingDepthShader.Use();
		shadowMappingDepthShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);


		// reset viewport
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 2. render scene as normal using the generated depth/shadow map 
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shadowMappingShader.Use();
		glm::mat4 projection = pCamera->GetProjectionMatrix();
		glm::mat4 view = pCamera->GetViewMatrix();
		shadowMappingShader.SetMat4("projection", projection);
		shadowMappingShader.SetMat4("view", view);
		// set light uniforms
		shadowMappingShader.SetVec3("viewPos", pCamera->GetPosition());
		shadowMappingShader.SetVec3("lightPos", lightPos);
		shadowMappingShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);

		//Camera

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, roomTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderScene(shadowMappingShader);


		//Stegosaurus

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, stegosaurusTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderStegosaurus(shadowMappingShader);

		//Grizzly

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, grizzlyTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderGrizzly(shadowMappingShader);

		//Pterodactyle

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, pteroTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderPtero(shadowMappingShader,lightPos);

		//Velociraptor

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, veloTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderVelociraptor(shadowMappingShader);

		//cute dino

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, cuteDinoTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderCuteDino(shadowMappingShader);

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	delete pCamera;

	glfwTerminate();
	return 0;
}

// renders the 3D scene
// --------------------
void renderScene(const Shader& shader)
{
	glm::mat4 model;
	shader.SetMat4("model", model);
	renderRoom();
}



void renderVelociraptor(const Shader& shader)
{
	glm::mat4 object;
	object = glm::mat4();
	object = glm::translate(object, glm::vec3(100.0f, 6.f, 50.0f));
	object = glm::scale(object, glm::vec3(7.f));
	object = glm::rotate(object, glm::radians(270.0f), glm::vec3(0.f, 1.f, 0.f));
	shader.SetMat4("model", object);
	renderVelociraptorBody();
	renderVelociraptorEyes();
	renderVelociraptorLowerJaw();
	renderVelociraptorClaws();
	renderVelociraptorUpperJaw();
}
void renderGrizzly(const Shader& shader)
{
	glm::mat4 object;
	object = glm::mat4();
	object = glm::translate(object, glm::vec3(0.0f, 10.f, -200.0f));
	object = glm::scale(object, glm::vec3(35.f));

	shader.SetMat4("model", object);
	renderGrizzly();
	renderGrizzlyFace();
	renderGrizzlyEyes();
}

void renderPtero(const Shader& shader, glm::vec3&light)
{
	glm::mat4 object;
	object = glm::mat4();
	object = glm::translate(object, lightPos);
	object = glm::scale(object, glm::vec3(3500.f));
	object = glm::rotate(object, glm::radians(270.0f), glm::vec3(0.f, 1.f, 0.f));
	shader.SetMat4("model", object);
	renderPtero();
}



void renderStegosaurus(const Shader& shader)
{
	glm::mat4 object;
	object = glm::mat4();
	object = glm::translate(object, glm::vec3(100.0f, 8.5f, 150.0f));
	object = glm::scale(object, glm::vec3(10.f));
	object = glm::rotate(object, glm::radians(270.0f), glm::vec3(0.f, 1.f, 0.f));
	shader.SetMat4("model", object);
	renderStegosaurus();
}

void renderCuteDino(const Shader& shader)
{
	glm::mat4 object;
	object = glm::mat4();
	object = glm::translate(object, glm::vec3(0.f,25.f,200.0f));
	object = glm::scale(object, glm::vec3(1000.f));
	object = glm::rotate(object, glm::radians(180.0f), glm::vec3(0.f, 1.f, 0.f));
	shader.SetMat4("model", object);
	renderCuteDino();
}




float verticesRoom[82000];
unsigned int indicesRoom[72000];


GLuint floorVAO, floorVBO, floorEBO;

void renderRoom()
{
	if (floorVAO == 0)
	{

		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Room.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			verticesRoom[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicesRoom[j] = indices.at(j);
		}



		glGenVertexArrays(1, &floorVAO);
		glGenBuffers(1, &floorVBO);
		glGenBuffers(1, &floorEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(verticesRoom), verticesRoom, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicesRoom), &indicesRoom, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	// render Cube
	glBindVertexArray(floorVAO);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float stegosaurusVertices[820000];
unsigned int indicestegosaurus[72000];
GLuint stegosaurusVAO, stegosaurusVBO, stegosaurusEBO;

void renderStegosaurus()
{
	if (stegosaurusVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("stegosaurus.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			stegosaurusVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicestegosaurus[j] = indices.at(j);
		}

		glGenVertexArrays(1, &stegosaurusVAO);
		glGenBuffers(1, &stegosaurusVBO);
		glGenBuffers(1, &stegosaurusEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, stegosaurusVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(stegosaurusVertices), stegosaurusVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stegosaurusEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicestegosaurus), &indicestegosaurus, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(stegosaurusVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(stegosaurusVAO);
	glBindBuffer(GL_ARRAY_BUFFER, stegosaurusVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stegosaurusEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float cuteDinoVertices[82000];
unsigned int cuteDinoIndices[72000];
GLuint cuteDinoVAO, cuteDinoVBO, cuteDinoEBO;

void renderCuteDino()
{
	if (cuteDinoVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("cuteDino.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			cuteDinoVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			cuteDinoIndices[j] = indices.at(j);
		}

		glGenVertexArrays(1, &cuteDinoVAO);
		glGenBuffers(1, &cuteDinoVBO);
		glGenBuffers(1, &cuteDinoEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cuteDinoVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(cuteDinoVertices), cuteDinoVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cuteDinoEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cuteDinoIndices), &cuteDinoIndices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cuteDinoVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cuteDinoVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cuteDinoVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cuteDinoEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}



float velociraptorVertices[820000];
unsigned int indicevelociraptor[72000];
GLuint velociraptorVAO, velociraptorVBO, velociraptorEBO;
void renderVelociraptorBody()
{
	if (velociraptorVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Velociraptor.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			velociraptorVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicevelociraptor[j] = indices.at(j);
		}

		glGenVertexArrays(1, &velociraptorVAO);
		glGenBuffers(1, &velociraptorVBO);
		glGenBuffers(1, &velociraptorEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, velociraptorVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(velociraptorVertices), velociraptorVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicevelociraptor), &indicevelociraptor, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(velociraptorVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(velociraptorVAO);
	glBindBuffer(GL_ARRAY_BUFFER, velociraptorVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}


float velociraptorEyesVertices[820000];
unsigned int indicevelociraptorEyes[72000];
GLuint velociraptorEyesVAO, velociraptorEyesVBO, velociraptorEyesEBO;
void renderVelociraptorEyes()
{
	if (velociraptorEyesVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Velociraptor.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[1];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			velociraptorEyesVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicevelociraptorEyes[j] = indices.at(j);
		}

		glGenVertexArrays(1, &velociraptorEyesVAO);
		glGenBuffers(1, &velociraptorEyesVBO);
		glGenBuffers(1, &velociraptorEyesEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, velociraptorEyesVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(velociraptorEyesVertices), velociraptorEyesVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorEyesEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicevelociraptorEyes), &indicevelociraptorEyes, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(velociraptorEyesVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(velociraptorEyesVAO);
	glBindBuffer(GL_ARRAY_BUFFER, velociraptorEyesVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorEyesEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float velociraptorLowerJawVertices[820000];
unsigned int indicevelociraptorLowerJaw[72000];
GLuint velociraptorLowerJawVAO, velociraptorLowerJawVBO, velociraptorLowerJawEBO;
void renderVelociraptorLowerJaw()
{
	if (velociraptorLowerJawVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Velociraptor.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[2];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			velociraptorLowerJawVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicevelociraptorLowerJaw[j] = indices.at(j);
		}

		glGenVertexArrays(1, &velociraptorLowerJawVAO);
		glGenBuffers(1, &velociraptorLowerJawVBO);
		glGenBuffers(1, &velociraptorLowerJawEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, velociraptorLowerJawVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(velociraptorLowerJawVertices), velociraptorLowerJawVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorLowerJawEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicevelociraptorLowerJaw), &indicevelociraptorLowerJaw, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(velociraptorLowerJawVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(velociraptorLowerJawVAO);
	glBindBuffer(GL_ARRAY_BUFFER, velociraptorLowerJawVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorLowerJawEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float velociraptorClawsVertices[820000];
unsigned int indicevelociraptorClaws[72000];
GLuint velociraptorClawsVAO, velociraptorClawsVBO, velociraptorClawsEBO;
void renderVelociraptorClaws()
{
	if (velociraptorClawsVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Velociraptor.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[3];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			velociraptorClawsVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicevelociraptorClaws[j] = indices.at(j);
		}

		glGenVertexArrays(1, &velociraptorClawsVAO);
		glGenBuffers(1, &velociraptorClawsVBO);
		glGenBuffers(1, &velociraptorClawsEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, velociraptorClawsVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(velociraptorClawsVertices), velociraptorClawsVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorClawsEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicevelociraptorClaws), &indicevelociraptorClaws, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(velociraptorClawsVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(velociraptorClawsVAO);
	glBindBuffer(GL_ARRAY_BUFFER, velociraptorClawsVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorClawsEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float velociraptorUpperJawVertices[820000];
unsigned int indicevelociraptorUpperJaw[72000];
GLuint velociraptorUpperJawVAO, velociraptorUpperJawVBO, velociraptorUpperJawEBO;
void renderVelociraptorUpperJaw()
{
	if (velociraptorUpperJawVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Velociraptor.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[4];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			velociraptorUpperJawVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicevelociraptorUpperJaw[j] = indices.at(j);
		}

		glGenVertexArrays(1, &velociraptorUpperJawVAO);
		glGenBuffers(1, &velociraptorUpperJawVBO);
		glGenBuffers(1, &velociraptorUpperJawEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, velociraptorUpperJawVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(velociraptorUpperJawVertices), velociraptorUpperJawVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorUpperJawEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicevelociraptorUpperJaw), &indicevelociraptorUpperJaw, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(velociraptorUpperJawVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(velociraptorUpperJawVAO);
	glBindBuffer(GL_ARRAY_BUFFER, velociraptorUpperJawVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, velociraptorUpperJawEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float pteroVertices[820000];
unsigned int indiceptero[72000];
GLuint pteroVAO, pteroVBO, pteroEBO;
void renderPtero()
{
	if (pteroVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Ptero.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			pteroVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indiceptero[j] = indices.at(j);
		}

		glGenVertexArrays(1, &pteroVAO);
		glGenBuffers(1, &pteroVBO);
		glGenBuffers(1, &pteroEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, pteroVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(pteroVertices), pteroVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pteroEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indiceptero), &indiceptero, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(pteroVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(pteroVAO);
	glBindBuffer(GL_ARRAY_BUFFER, pteroVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pteroEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}


float grizzlyVertices[820000];
unsigned int indicesGrizzly[72000];
GLuint grizzlyVAO, grizzlyVBO, grizzlyEBO;
void renderGrizzly()
{
	if (grizzlyVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Grizzly.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			grizzlyVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicesGrizzly[j] = indices.at(j);
		}




		glGenVertexArrays(1, &grizzlyVAO);
		glGenBuffers(1, &grizzlyVBO);
		glGenBuffers(1, &grizzlyEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, grizzlyVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(grizzlyVertices), grizzlyVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grizzlyEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicesGrizzly), &indicesGrizzly, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(grizzlyVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);





	}
	// render Cube
	glBindVertexArray(grizzlyVAO);
	glBindBuffer(GL_ARRAY_BUFFER, grizzlyVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grizzlyEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float grizzlyFaceVertices[820000];
unsigned int indicesGrizzlyFace[72000];
GLuint grizzlyFaceVAO, grizzlyFaceVBO, grizzlyFaceEBO;
void renderGrizzlyFace()
{
	if (grizzlyFaceVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Grizzly.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[2];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			grizzlyFaceVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicesGrizzlyFace[j] = indices.at(j);
		}




		glGenVertexArrays(1, &grizzlyFaceVAO);
		glGenBuffers(1, &grizzlyFaceVBO);
		glGenBuffers(1, &grizzlyFaceEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, grizzlyFaceVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(grizzlyFaceVertices), grizzlyFaceVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grizzlyFaceEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicesGrizzlyFace), &indicesGrizzlyFace, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(grizzlyFaceVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(grizzlyFaceVAO);
	glBindBuffer(GL_ARRAY_BUFFER, grizzlyFaceVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grizzlyFaceEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

float grizzlyEyesVertices[820000];
unsigned int indicesGrizzlyEyes[72000];
GLuint grizzlyEyesVAO, grizzlyEyesVBO, grizzlyEyesEBO;
void renderGrizzlyEyes()
{
	if (grizzlyEyesVAO == 0)
	{
		std::vector<float> vertices;
		std::vector<float> indices;

		Loader.LoadFile("Grizzly.obj");
		objl::Mesh curMesh = Loader.LoadedMeshes[1];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			vertices.push_back((float)curMesh.Vertices[j].Position.X);
			vertices.push_back((float)curMesh.Vertices[j].Position.Y);
			vertices.push_back((float)curMesh.Vertices[j].Position.Z);
			vertices.push_back((float)curMesh.Vertices[j].Normal.X);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Y);
			vertices.push_back((float)curMesh.Vertices[j].Normal.Z);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			vertices.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < vertices.size(); j++)
		{
			grizzlyEyesVertices[j] = vertices.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indices.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indicesGrizzlyEyes[j] = indices.at(j);
		}




		glGenVertexArrays(1, &grizzlyEyesVAO);
		glGenBuffers(1, &grizzlyEyesVBO);
		glGenBuffers(1, &grizzlyEyesEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, grizzlyEyesVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(grizzlyEyesVertices), grizzlyEyesVertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grizzlyEyesEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicesGrizzlyEyes), &indicesGrizzlyEyes, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(grizzlyEyesVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(grizzlyEyesVAO);
	glBindBuffer(GL_ARRAY_BUFFER, grizzlyEyesVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grizzlyEyesEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}




// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		pCamera->ProcessKeyboard(FORWARD, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		pCamera->ProcessKeyboard(BACKWARD, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		pCamera->ProcessKeyboard(LEFT, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		pCamera->ProcessKeyboard(RIGHT, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
		pCamera->ProcessKeyboard(UP, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
		pCamera->ProcessKeyboard(DOWN, (float)deltaTime);

	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		pCamera->Reset(width, height);

	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	pCamera->Reshape(width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	pCamera->MouseControl((float)xpos, (float)ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yOffset)
{
	pCamera->ProcessMouseScroll((float)yOffset);
}
