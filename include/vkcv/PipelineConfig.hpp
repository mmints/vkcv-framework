/**
 * @authors Mara Vogt, Mark Mints
 * @file src/vkcv/Pipeline.hpp
 * @brief Pipeline class to handle shader stages
 */

#ifndef VKCV_PIPELINECONFIG_HPP
#define VKCV_PIPELINECONFIG_HPP

#include <vector>
#include <cstdint>
#include "vkcv/Handles.hpp"
#include "ShaderProgram.hpp"

namespace vkcv {

    class PipelineConfig {

    public:
        /**
         *  Default constructer is deleted!
         */
        PipelineConfig() = delete;

        /**
         *  Constructor for the pipeline. Creates a pipeline using @p vertexCode, @p fragmentCode as well as the
         *  dimensions of the application window @p width and @p height. A handle for the Render Pass is also needed, @p passHandle.
         *
         * @param shaderProgram shaders of the pipeline
         * @param height height of the application window
         * @param width width of the application window
         * @param passHandle handle for Render Pass
         */
        PipelineConfig(const ShaderProgram& shaderProgram, uint32_t width, uint32_t height, PassHandle &passHandle);

		ShaderProgram m_ShaderProgram;
        uint32_t m_Height;
        uint32_t m_Width;
        PassHandle m_PassHandle;
    };

}
#endif //VKCV_PIPELINECONFIG_HPP
