//render dock
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
#define GLM_FORCE_RADIAN
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <memory>
#include "utils/imgui_impl_opengl3.h"
#include "utils/imgui_impl_glfw.h"
#include "main.h"


unsigned int planeVAO;
int shadow_width = 1024;
int shadow_height = 1024;

void renderScene(Program &shader, StaticMesh &mesh, float degree);
static void error_callback(int error, const char* description)
{
    std::cerr<<fmt::format("Error: {0}\n", description);
}

int main(void)
{
    GLFWwindow* window;
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(1024, 768, "Simple example", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    if(!gladLoadGL()) {
        exit(EXIT_FAILURE);
    }
    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Setup style
    ImGui::StyleColorsDark();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // auto text = Texture2D::LoadFromFile("../resource/image.png");
    auto mesh = StaticMesh::LoadMesh("../resource/sphere.obj");
    auto prog = Program::LoadFromFile(
        "../resource/vs.vert",
        "../resource/gs.geom",
        "../resource/fs.frag"
    );
    auto prog2 = Program::LoadFromFile(
        "../resource/vs.vert",
        "../resource/gs.geom",
        "../resource/fs_light.frag"
    );
    auto depthshader = Program::LoadFromFile(
        "../resource/depth.vert",
        "../resource/depth.frag"      
    );
    auto shadow = ShadowMap::LoadDepth(shadow_width, shadow_height);
    shadow.attach();


    float planeVertices[] = {
        25.0f, 25.0f, -1.5f, 25.0f, 0.0f, 0.0f, 0.0f, 1.0f,
       -25.0f, 25.0f, -1.5f,  0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
       -25.0f,-25.0f, -1.5f,  0.0f,25.0f, 0.0f, 0.0f, 1.0f,

        25.0f, 25.0f, -1.5f, 25.0f, 0.0f, 0.0f, 0.0f, 1.0f,
       -25.0f,-25.0f, -1.5f,  0.0f,25.0f, 0.0f, 0.0f, 1.0f,
        25.0f,-25.0f, -1.5f, 25.0f,25.0f, 0.0f, 0.0f, 1.0f
    };
    
    unsigned int planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glBindVertexArray(0);

    {
    // text and mesh, shader => garbage collector
    // auto g1 = Protect(text);
    auto g2 = Protect(mesh);
    auto g3 = Protect(prog);

    if(!mesh.hasNormal()||!mesh.hasUV()) {
        std::cerr<<"Mesh does not have normal or uv\n";
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    float degree = 0.0f;
    glm::vec3 object_color{1.0f};

    glm::vec3 light_pos = glm::vec3(2.0f, -2.0f, 10.0f);
    glm::vec3 light_center = glm::vec3(0.0f);
    float specular_s = 0.0f;
    float ambient_s = 0.0f;

    prog.use();
    // prog["text"] = 0;
    prog["shadowMap"] = 1;

    glEnable(GL_DEPTH_TEST);
    

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. render depth of scene to texture
        float near_plane = 0.1f, far_plane = 10000.0f;

        glm::mat4 lightprojection, lightview;
        glm::mat4 lightspacematrix;
        //lightprojection = glm::ortho(-5.0f, 5.0f, -3.0f, 3.0f, near_plane, far_plane);
        lightprojection = glm::perspective(90.0f/180.0f*3.1415926f, (GLfloat)1024 / (GLfloat)1024, near_plane, far_plane);
        lightview = glm::lookAt(light_pos, light_center, glm::vec3(0.0, 1.0, 0.0));
        lightspacematrix = lightprojection * lightview;
        depthshader.use();
        depthshader["lightspacematrix"] = lightspacematrix;

        shadow.generate();
        renderScene(depthshader, mesh, degree);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        prog.use();
        prog["lightspacematrix"] = lightspacematrix;
        prog["vp"] = glm::perspective(45/180.0f*3.1415926f, 1024.0f/768.0f, 0.1f, 10000.0f)*
            glm::lookAt(glm::vec3{0, 0, 10}, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});
        prog["object_color"] = object_color;

        prog["light_pos"] = light_pos;
        prog["eye_pos"] = glm::vec3{0, 0, 10};
        prog["specular_s"] = specular_s;
        prog["ambient_s"] = ambient_s;
        prog["cutoff"] = glm::cos(glm::radians(30.0f));
        prog["light_center"] = light_center;

        // text.bindToChannel(0);
        shadow.bindToChannel(1);
        renderScene(prog, mesh, degree);
        {
            prog2["vp"] = glm::perspective(45/180.0f*3.1415926f, 1024.0f/768.0f, 0.1f, 10000.0f)*
            glm::lookAt(glm::vec3{0, 0, 10}, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});
            prog2["model"] = glm::translate(glm::mat4(1.0f), light_pos)*glm::scale(glm::mat4(1.0f), glm::vec3{0.2f});

            prog2.use();
            //mesh.draw();
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::SliderFloat("degree", &degree, 0.0f, 360.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::SliderFloat("Specular", &specular_s, 0, 1);
            ImGui::SliderFloat("Ambient", &ambient_s, 0, 1);
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color
            ImGui::ColorEdit3("object color", glm::value_ptr(object_color)); // Edit 3 floats representing a color
            ImGui::SliderFloat2("Position", glm::value_ptr(light_center), -10, 10);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
            
    }

    }
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}

void renderScene(Program &shader, StaticMesh &mesh, float degree){
    //draw plane
    shader["model"] = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, -10.0f));
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    //draw ball
    shader["model"] = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, -2.0f, 2.0f))*glm::rotate(glm::mat4(1.0f), degree*3.1415926f/180.0f, glm::vec3(0, 1, 0));
    mesh.draw();

    shader["model"] = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 1.0f, -3.0f))*glm::rotate(glm::mat4(1.0f), degree*3.1415926f/180.0f, glm::vec3(0, 1, 0));
    mesh.draw();
}
