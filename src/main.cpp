#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include <optional>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Decoder.h"
#include "VideoRenderer.h"
#include "AudioPlayer.h"
#include "ThreadSafeQueue.h"
#include "SyncManager.h"

template <typename T>
std::optional<T> waitPopOpt(ThreadSafeQueue<T>& queue, int timeoutMs = 10) {
    T item;
    if (queue.waitPop(item, timeoutMs)) return item;
    return std::nullopt;
}

struct VideoFBO {
    GLuint fbo = 0;
    GLuint texture = 0;
    int width = 0;
    int height = 0;

    void create(int w, int h) {
        width = w;
        height = h;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "FBO setup failed!\n";

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void bind() { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }
    void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

    void updateTexture(const uint8_t* frameData) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        GL_RGB, GL_UNSIGNED_BYTE, frameData);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
};

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << "\n";
    }
    return shader;
}

// 시간을 MM:SS 포맷으로 변환하는 함수
std::string formatTime(double seconds) {
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

int main() {
    try {
        // PLAY LIST
        std::vector<std::string> files = {
                "../assets/big_buck_bunny_1080p_h264.mov",
                "../assets/tears_of_steel_1080p_h264.mov",
                "../assets/STARCRAFT_1080p_h264.mov"
        };

        // CHOICE A PLAY FILE
        auto selectFile = [&]() -> std::string {
            std::cout << "Select a video to play:\n";
            for (size_t i = 0; i < files.size(); ++i)
                std::cout << "  " << (i + 1) << ": " << files[i] << "\n";

            int choice = 0;
            while (true) {
                std::cout << "Enter number (1-" << files.size() << "): ";
                std::cin >> choice;
                if (choice >= 1 && choice <= static_cast<int>(files.size()))
                    break;
                std::cout << "Invalid choice, try again.\n";
            }
            return files[choice - 1];
        };

        std::string filename = selectFile();
        std::cout << "Playing: " << filename << "\n";

        // INIT
        Decoder decoder(filename);
        if (!glfwInit()) return -1;

        const int videoWidth = 1280;
        const int videoHeight = 720;
        const int controlsHeight = 80;

        int winWidth = videoWidth;
        int winHeight = videoHeight + controlsHeight;

        GLFWwindow* window = glfwCreateWindow(winWidth, winHeight, "Loki Media Player", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW\n";
            return -1;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();

        // Style
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.ItemSpacing = ImVec2(8, 8);
        style.WindowPadding = ImVec2(12, 12);
        style.FramePadding = ImVec2(8, 4);
        style.GrabRounding = 4.0f;

        // Theme
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        VideoRenderer renderer(window, videoWidth, videoHeight);
        AudioPlayer audioOut(48000, 2);
        SyncManager syncManager;

        auto& videoQueue = decoder.getVideoQueue();
        auto& audioQueue = decoder.getAudioQueue();

        std::atomic<bool> audioRun{true};
        std::atomic<bool> playing{false};
        std::atomic<bool> isFullscreen{false};

        double totalDuration = decoder.getDuration();
        double currentTime = 0.0;
        bool seekRequested = false;
        double seekTarget = 0.0;

        std::thread audioThread([&]() {
            while (audioRun) {
                if (playing) {
                    // Seek
                    if (seekRequested) {
                        decoder.seek(seekTarget);
                        syncManager.reset();
                        videoQueue.clear();
                        audioQueue.clear();
                        seekRequested = false;
                        continue;
                    }

                    if (auto afOpt = waitPopOpt(audioQueue, 10)) {
                        auto& af = *afOpt;
                        audioOut.queueFrame(af);
                        syncManager.setAudioClock(audioOut.getCurrentPts());
                        currentTime = audioOut.getCurrentPts();
                        if (!syncManager.isInitialized() && !videoQueue.empty()) {
                            if (auto vfOpt = waitPopOpt(videoQueue, 5)) {
                                if (vfOpt->pts >= 0 && af.pts >= 0)
                                    syncManager.initialize(vfOpt->pts, af.pts);
                                videoQueue.push_front(std::move(*vfOpt));
                            }
                        }
                    }
                } else { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            }
        });

        // --- Double FBO setup ---
        VideoFBO fboFront, fboBack;
        fboFront.create(videoWidth, videoHeight);
        fboBack.create(videoWidth, videoHeight);

        GLuint vertexShader, fragmentShader, shaderProgram;
        const char* vertexShaderSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTex;
        out vec2 TexCoord;
        void main() {
            TexCoord = aTex;
            gl_Position = vec4(aPos.xy, 0.0, 1.0);
        })";

        const char* fragmentShaderSrc = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D screenTexture;
        void main() { FragColor = texture(screenTexture, TexCoord); })";

        vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLuint VAO, VBO, EBO;
        float vertices[] = {
                -1.f, -1.f, 0.f, 0.f,
                1.f, -1.f, 1.f, 0.f,
                1.f,  1.f, 1.f, 1.f,
                -1.f,  1.f, 0.f, 1.f
        };
        unsigned int indices[] = {0,1,2, 2,3,0};
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        auto lastFrameTime = std::chrono::steady_clock::now();
        const auto targetFrameTime = std::chrono::milliseconds(16);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // --- Render new video frame to back FBO ---
            if (playing) {
                auto now = std::chrono::steady_clock::now();
                if (now - lastFrameTime >= targetFrameTime) {
                    if (auto vfOpt = waitPopOpt(videoQueue, 5)) {
                        auto& vf = *vfOpt;
                        if (!syncManager.syncVideo(vf)) continue;

                        fboBack.bind();
                        glViewport(0,0,videoWidth,videoHeight);
                        glClearColor(0,0,0,1);
                        glClear(GL_COLOR_BUFFER_BIT);

                        renderer.renderFrame(vf);
                        fboBack.unbind();

                        std::swap(fboFront, fboBack);
                        lastFrameTime = now;
                    }
                }
            }

            // --- Draw front FBO to screen (비디오를 상단에 표시) ---
            glViewport(0, controlsHeight, videoWidth, videoHeight);
            glUseProgram(shaderProgram);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, fboFront.texture);
            glUniform1i(glGetUniformLocation(shaderProgram, "screenTexture"), 0);
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);

            glViewport(0, 0, winWidth, winHeight);

            // --- Control Panel ---
            ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(videoHeight)), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(videoWidth), static_cast<float>(controlsHeight)), ImGuiCond_Always);

            ImGui::Begin("MediaControls", nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);

            // --- Progress bar ---
            ImGui::SetCursorPosY(8);
            float progress = (totalDuration > 0) ? static_cast<float>(currentTime / totalDuration) : 0.0f;
            float progressValue = progress;

            ImGui::PushItemWidth(static_cast<float>(videoWidth) - 240); // 시간 표시를 위한 여백
            ImGui::SetCursorPosX(12);

            if (ImGui::SliderFloat("##progress", &progressValue, 0.0f, 1.0f, "")) {
                if (ImGui::IsItemActive()) {
                    seekTarget = progressValue * totalDuration;
                    seekRequested = true;
                }
            }
            ImGui::PopItemWidth();

            // --- Time (right)
            ImGui::SameLine();
            std::string timeText = formatTime(currentTime) + " / " + formatTime(totalDuration);
            ImGui::TextUnformatted(timeText.c_str());

            // --- Button (bottom)
            ImGui::SetCursorPosY(35);

            const float buttonWidth = 60.0f;
            const float buttonHeight = 35.0f;
            const float spacing = 15.0f;
            const int numButtons = 3;
            float totalWidth = buttonWidth * numButtons + spacing * (numButtons - 1);
            float startX = (static_cast<float>(videoWidth) - totalWidth) * 0.5f;

            ImGui::SetCursorPosX(startX);

            // Play/Pause Toggle
            const char* playPauseLabel = playing ? "Pause" : "Play";
            if (ImGui::Button(playPauseLabel, ImVec2(buttonWidth, buttonHeight))) {
                if (playing) {
                    playing = false;
                    syncManager.pause();
                } else {
                    if (!playing) {
                        decoder.stop();
                        videoQueue.clear();
                        audioQueue.clear();
                        syncManager.reset();
                        decoder.start();
                        playing = true;
                        syncManager.resume();
                    }
                }
            }

            ImGui::SameLine(0, spacing);
            if (ImGui::Button("Stop", ImVec2(buttonWidth, buttonHeight))) {
                playing = false;
                decoder.stop();
                videoQueue.clear();
                audioQueue.clear();
                syncManager.reset();
                currentTime = 0.0;
            }

            ImGui::SameLine(0, spacing);
            if (ImGui::Button("<<10s", ImVec2(buttonWidth, buttonHeight))) {
                // << 10sec
                seekTarget = std::max(0.0, currentTime - 10.0);
                seekRequested = true;
            }

            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }

        audioRun = false;
        if (audioThread.joinable()) audioThread.join();
        decoder.stop();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}