#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
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
"attribute vec2 vPos;\n"
"void main()\n"
"{\n"
"    gl_Position = vec4(vPos, 0.0, 1.0);\n"
"}\n";
 
static const char* fragment_shader_text =
"#version 110\n"
"uniform float iTime;\n" // fragment shader 中用到 iTime 参数
"uniform vec2 iResolution;\n" // fragment shader 中用到 iResolution 参数

"vec3 palette( float t ) {\n"
"    vec3 a = vec3(0.5, 0.5, 0.5);\n"
"    vec3 b = vec3(0.5, 0.5, 0.5);\n"
"    vec3 c = vec3(1.0, 1.0, 1.0);\n"
"    vec3 d = vec3(0.263,0.416,0.557);\n"
"\n"
"    return a + b*cos( 6.28318*(c*t+d) );\n"
"}\n"
"void mainImage( out vec4 fragColor, in vec2 fragCoord ) {\n"
"    vec2 uv = (fragCoord * 2.0 - iResolution.xy) / iResolution.y;\n"
"    vec2 uv0 = uv;\n"
"    vec3 finalColor = vec3(0.0);\n"
"    for (float i = 0.0; i < 4.0; i++) {\n"
"        uv = fract(uv * 1.5) - 0.5;\n"
"        float d = length(uv) * exp(-length(uv0));\n"
"        vec3 col = palette(length(uv0) + i*.4 + iTime*.4);\n"
"        d = sin(d*8. + iTime)/8.;\n"
"        d = abs(d);\n"
"        d = pow(0.01 / d, 1.2);\n"
"        finalColor += col * d;\n"
"    }\n"
"    fragColor = vec4(finalColor, 1.0);\n"
"}\n"

// 按 shadertoy 风格的 mainImage 来组织代码
"void main()\n"
"{\n"
"    mainImage( gl_FragColor, gl_FragCoord.xy );\n"
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
    // 定义 itime_location, iresolution_location 变量
    GLint vpos_location, itime_location, iresolution_location;
 
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

    vpos_location = glGetAttribLocation(program, "vPos");
    // 获取参数地址
    itime_location = glGetUniformLocation(program, "iTime");
    iresolution_location = glGetUniformLocation(program, "iResolution");
 
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
                          sizeof(vertices[0]), (void*) 0);
 
    while (!glfwWindowShouldClose(window))
    {
        float ratio;
        int width, height;
 
        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float) height;
 
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
 
        glUseProgram(program);
        glUniform2f(iresolution_location, (float) width, (float) height);
        glUniform1f(itime_location, (float) glfwGetTime()); // 赋值当前时间给 iTime
        glDrawArrays(GL_TRIANGLES, 0, 6);
 
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
 
    glfwDestroyWindow(window);
 
    glfwTerminate();
    exit(EXIT_SUCCESS);
}