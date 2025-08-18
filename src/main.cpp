#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include <optional>
#include <vector>

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

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        GLFWwindow* window = glfwCreateWindow(1280, 720, "Loki Media Player", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        VideoRenderer renderer(window, 1280, 720);
        AudioPlayer audioOut(48000, 2);
        SyncManager syncManager;

        decoder.start();
        auto& videoQueue = decoder.getVideoQueue();
        auto& audioQueue = decoder.getAudioQueue();

        std::atomic<bool> audioRun{true};

        // AUDIO
        auto audioLoop = [&]() {
            while (audioRun) {
                if (auto afOpt = waitPopOpt(audioQueue, 10)) {
                    auto& af = *afOpt;
                    audioOut.queueFrame(af);
                    syncManager.setAudioClock(audioOut.getCurrentPts());

                    if (!syncManager.isInitialized() && !videoQueue.empty()) {
                        if (auto vfOpt = waitPopOpt(videoQueue, 5)) {
                            if (vfOpt->pts >= 0 && af.pts >= 0) {
                                // PTS RESET
                                syncManager.initialize(vfOpt->pts, af.pts);
                            }
                            videoQueue.push_front(std::move(*vfOpt));
                        }
                    }
                }
            }
        };
        std::thread audioThread(audioLoop);

        // VIDEO
        auto renderLoop = [&]() {
            auto lastFrameTime = std::chrono::steady_clock::now();
            const auto targetFrameTime = std::chrono::milliseconds(16);

            while (!glfwWindowShouldClose(window)) {
                auto now = std::chrono::steady_clock::now();
                if (now - lastFrameTime < targetFrameTime) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    glfwPollEvents();
                    continue;
                }

                if (auto vfOpt = waitPopOpt(videoQueue, 5)) {
                    auto& vf = *vfOpt;
                    if (!syncManager.syncVideo(vf)) continue;

                    renderer.renderFrame(vf);
                    lastFrameTime = now;

                    if (!syncManager.isInitialized()) {
                        if (vf.pts >= 0 && audioOut.getCurrentPts() >= 0)
                            syncManager.initialize(vf.pts, audioOut.getCurrentPts());
                    }
                }

                glfwPollEvents();
            }
        };

        renderLoop();

        // CLEANUP
        audioRun = false;
        if (audioThread.joinable()) audioThread.join();
        decoder.stop();

        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
