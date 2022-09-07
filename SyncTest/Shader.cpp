#include "Shader.h"
#include <string>
#include <vector>

Shader::Shader()
{
    // compile VERTEX shader

    std::string vertexShaderStr=
        "#version 330 core\n"
        "layout(location=0)in vec2 verts;\n"
        "out vec2 texturePos;\n"
        "void main(){\n"
        "  gl_Position=vec4(verts.x,verts.y,0,1);\n"
        "  texturePos=(verts+vec2(1.0))/vec2(2.0);\n"
        "}";


    mVertexShader = glCreateShader(GL_VERTEX_SHADER);
    char* vertexShaderStrPtr=&vertexShaderStr[0];
    glShaderSource(mVertexShader, 1, &vertexShaderStrPtr, nullptr);
    glCompileShader(mVertexShader);

    GLint success;
    glGetShaderiv(mVertexShader,GL_COMPILE_STATUS,&success);
    if(!success){
        printf("compile vertex shader failed\n");
        exit(1);
    }

    printf("vertex shader compiled successfully\n");


    // compile FRAGMENT shader

    std::string fragmentShaderStr=
        R"(
            #version 330 core
            layout(location=0)out vec4 res;
            uniform sampler2D tex;
            in vec2 texturePos;
            void main() {
                res = texture(tex, texturePos);
            }
            )";

    mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    char* fragmentShaderStrPtr=&fragmentShaderStr[0];
    glShaderSource(mFragmentShader, 1, &fragmentShaderStrPtr, nullptr);
    glCompileShader(mFragmentShader);

    glGetShaderiv(mFragmentShader,GL_COMPILE_STATUS,&success);
    if(!success){
        printf("Shader compile failed\n");
        exit(1);
    }

    printf("fragment shader compiled successfully\n");

    mShaderProgram = glCreateProgram();

    glAttachShader(mShaderProgram, mVertexShader);
    glAttachShader(mShaderProgram, mFragmentShader);

    glLinkProgram(mShaderProgram);
    glGetProgramiv(mShaderProgram, GL_LINK_STATUS, &success);
    if(!success){
        printf("shader program link failed\n");
        exit(1);
    }

    printf("shader program linked successfully\n");

    mLocation = glGetUniformLocation(mShaderProgram, "tex");
    if (mLocation == -1){
        printf("tex location not found\n");
//        exit(1);
    }


    // Create VAO
    glGenVertexArrays(1, &mVAO);

    // bind VAO
    glBindVertexArray(mVAO);

    // create VBO
    glGenBuffers(1, &mVBO);

    // bind VBO
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);

    std::vector<float> verts = {-1, -1, 1, -1, -1, 1, 1, 1};
    mVertsCount=4;

    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float) * 2),
                 &verts[0], GL_STATIC_DRAW);

    GLuint location=0;

    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void *>(0));

    glBindVertexArray(0);
}

Shader::~Shader()
{
    glDeleteProgram(mShaderProgram);
    glDeleteShader(mVertexShader);
    glDeleteShader(mFragmentShader);
}

void Shader::render(GLuint textureId)
{
    glBindVertexArray(mVAO);
    glUseProgram(mShaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(mLocation, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, mVertsCount);

    glUseProgram(0);
    glBindVertexArray(0);
}
