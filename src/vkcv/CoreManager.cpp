/**
 * @authors Sebastian Gaida
 * @file src/vkcv/CoreManager.cpp
 * @brief Handling of global states regarding dependencies
 */

#include "CoreManager.hpp"

namespace vkcv {

    int glfwCounter = 0;

    void initGLFW() {

        if (glfwCounter == 0) {
            int glfwSuccess = glfwInit();

            if (glfwSuccess == GLFW_FALSE) {
                throw std::runtime_error("Could not initialize GLFW");
            }
        }
        glfwCounter++;
    }

    void terminateGLFW() {
        if (glfwCounter == 1) {
            glfwTerminate();
        }
        glfwCounter--;
    }
}