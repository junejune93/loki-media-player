#include "Utils.h"
#include <iostream>
#include <cstdio>

namespace Utils {
    std::string formatTime(double seconds) {
        int mins = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
        return std::string(buffer);
    }

    std::string selectVideoFile(const std::vector<std::string> &files) {
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
    }
}