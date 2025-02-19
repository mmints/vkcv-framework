
#include "vkcv/shader/Compiler.hpp"

#include <vkcv/Container.hpp>
#include <vkcv/File.hpp>
#include <vkcv/Logger.hpp>

namespace vkcv::shader {
	
	bool Compiler::compileSourceWithHeaders(ShaderStage shaderStage,
											const std::string &shaderSource,
											const Dictionary<std::filesystem::path, std::string> &shaderHeaders,
											const ShaderCompiledFunction &compiled) {
		const std::filesystem::path directory = generateTemporaryDirectoryPath();
		
		if (!std::filesystem::create_directory(directory)) {
			vkcv_log(LogLevel::ERROR, "The directory could not be created (%s)",
							 directory.string().c_str());
			return false;
		}
		
		for (const auto& header : shaderHeaders) {
			if (header.first.has_parent_path()) {
				std::filesystem::create_directories(directory / header.first.parent_path());
			}
			
			if (!writeTextToFile(directory / header.first, header.second)) {
				return false;
			}
		}
		
		return compileSource(
				shaderStage,
				shaderSource,
				[&directory, &compiled] (vkcv::ShaderStage shaderStage,
										 const std::filesystem::path& path) {
					if (compiled) {
						compiled(shaderStage, path);
					}
					
					std::filesystem::remove_all(directory);
				}, directory
		);
	}

	void Compiler::compile(ShaderStage shaderStage,
												 const std::filesystem::path &shaderPath,
												 const ShaderCompiledFunction &compiled,
												 const std::filesystem::path &includePath,
												 bool update) {
		std::string shaderCode;
		bool result = readTextFromFile(shaderPath, shaderCode);
		
		if (!result) {
			vkcv_log(LogLevel::ERROR, "Loading shader failed: (%s)",
							 shaderPath.string().c_str());
		}
		
		if (!includePath.empty()) {
			result = compileSource(shaderStage, shaderCode, compiled, includePath);
		} else {
			result = compileSource(shaderStage, shaderCode, compiled, shaderPath.parent_path());
		}
		
		if (!result) {
			vkcv_log(LogLevel::ERROR, "Shader compilation failed: (%s)",
							 shaderPath.string().c_str());
		}
		
		if (update) {
			// TODO: Shader hot compilation during runtime
		}
	}
	
	void Compiler::compileProgram(ShaderProgram& program,
								  const Dictionary<ShaderStage, const std::filesystem::path>& stages,
								  const ShaderProgramCompiledFunction& compiled,
								  const std::filesystem::path& includePath, bool update) {
		std::vector<std::pair<ShaderStage, const std::filesystem::path>> stageList;
		size_t i;
		
		stageList.reserve(stages.size());
		for (const auto& stage : stages) {
			stageList.push_back(stage);
		}
		
		/* Compile a shader programs stages in parallel to improve performance */
		#pragma omp parallel for shared(stageList, includePath, update) private(i)
		for (i = 0; i < stageList.size(); i++) {
			const auto& stage = stageList[i];
			
			compile(
				stage.first,
				stage.second,
				[&program](ShaderStage shaderStage, const std::filesystem::path& path) {
					#pragma omp critical
					program.addShader(shaderStage, path);
				},
				includePath,
				update
			);
		}
		
		if (compiled) {
			compiled(program);
		}
	}
	
	std::string Compiler::getDefine(const std::string &name) const {
		return m_defines.at(name);
	}
	
	void Compiler::setDefine(const std::string &name, const std::string &value) {
		m_defines[name] = value;
	}
	
}
