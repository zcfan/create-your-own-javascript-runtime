#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
 
#include "linmath.h"
 
#include <stdlib.h>
#include <stdio.h>
 
static const struct
{
    float x, y;
    // float r, g, b; // 不需要指定顶点颜色
} vertices[6] =
{
    // 左下方三角形
    { -1.0f, 1.0f },
    { -1.0f, -1.0f },
    { 1.0f,  -1.0f },
    // 右上方三角形
    { 1.0f, -1.0f },
    { 1.0f,  1.0f },
    { -1.0f, 1.0f }
    // 共同组成一个正方形
};
 
static const char* vertex_shader_text =
"#version 110\n"
// "uniform mat4 MVP;\n" // 我们的顶点坐标直接用 clip space 单位（[-1.0, 1.0]），不需要变换
// "attribute vec3 vCol;\n" // 不使用传入的顶点颜色
"attribute vec2 vPos;\n"
// "varying vec3 color;\n" // 同样的，颜色完全由 fragment shader 决定，不用传递数据
"void main()\n"
"{\n"
// "    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n" // 不需要变化
"    gl_Position = vec4(vPos, 0.0, 1.0);\n"
// "    color = vCol;\n" // 不需要传递颜色给 fragment shader
"}\n";
 
static const char* fragment_shader_text =
"#version 110\n"
// "varying vec3 color;\n" // 不需要从 vertex shader 获得颜色
"void main()\n"
"{\n"
// "    gl_FragColor = vec4(color, 1.0);\n" // 当前阶段我们改为固定返回红色
"    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
"}\n";
 
static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
 
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}
 
int main(void)
{
    GLFWwindow* window;
    GLuint vertex_buffer, vertex_shader, fragment_shader, program;
    // GLint mvp_location, vpos_location, vcol_location; // 不需要的参数
    GLint vpos_location;
 
    glfwSetErrorCallback(error_callback);
 
    if (!glfwInit())
        exit(EXIT_FAILURE);
 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
 
    window = glfwCreateWindow(640, 640, "Simple example", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
 
    glfwSetKeyCallback(window, key_callback);
 
    glfwMakeContextCurrent(window);
    gladLoadGLES2(glfwGetProcAddress);
    glfwSwapInterval(1);
 
    // NOTE: OpenGL error checks have been omitted for brevity
 
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
 
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);
 
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);
 
    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
 
    // 不需要的参数
    // mvp_location = glGetUniformLocation(program, "MVP");
    // vcol_location = glGetAttribLocation(program, "vCol");
    vpos_location = glGetAttribLocation(program, "vPos");
 
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
                          sizeof(vertices[0]), (void*) 0);
    // glEnableVertexAttribArray(vcol_location);
    // glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
    //                       sizeof(vertices[0]), (void*) (sizeof(float) * 2));
 
    while (!glfwWindowShouldClose(window))
    {
        float ratio;
        int width, height;
        // 不需要的参数
        // mat4x4 m, p, mvp;
 
        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float) height;
 
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
 
        // 不需要的参数
        // mat4x4_identity(m);
        // mat4x4_rotate_Z(m, m, (float) glfwGetTime());
        // mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
        // mat4x4_mul(mvp, p, m);
 
        glUseProgram(program);
        // glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*) mvp); // 不需要的参数
        // glDrawArrays(GL_TRIANGLES, 0, 3); // 我们要画两个三角形，共 6 个顶点
        glDrawArrays(GL_TRIANGLES, 0, 6);
 
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
 
    glfwDestroyWindow(window);
 
    glfwTerminate();
    exit(EXIT_SUCCESS);
}