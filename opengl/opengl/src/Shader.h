#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader
{
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath);

    void use();
    void setBool(const std::string& name, bool value);
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setMat4(const std::string& name, const glm::mat4& mat);

private:
    unsigned int vertex, fragment;

    std::ifstream vertexShaderFile;
    std::ifstream fragmentShaderFile;
    std::stringstream vertexShaderStream;
    std::stringstream fragmentShaderStream;
    std::string vertexCode;
    std::string fragmentCode;
    const char* vertexShaderData;
    const char* fragmentShaderData;
};