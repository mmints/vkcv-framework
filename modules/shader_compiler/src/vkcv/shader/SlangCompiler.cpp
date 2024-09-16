
#include "vkcv/shader/SlangCompiler.hpp"

#include <cstdint>
#include <vkcv/File.hpp>
#include <vkcv/Logger.hpp>

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <vkcv/ShaderStage.hpp>

namespace vkcv::shader {
	
	static uint32_t s_CompilerCount = 0;
  static Slang::ComPtr<slang::IGlobalSession> s_GlobalSession;
	
	SlangCompiler::SlangCompiler(SlangCompileProfile profile)
	: Compiler(), m_profile(profile) {
		if (s_CompilerCount == 0) {
      slang::createGlobalSession(s_GlobalSession.writeRef());
		}
		
		s_CompilerCount++;
	}
	
	SlangCompiler::SlangCompiler(const SlangCompiler &other)
	: Compiler(other), m_profile(other.m_profile) {
		s_CompilerCount++;
	}
	
	SlangCompiler::~SlangCompiler() {
		s_CompilerCount--;
	}
	
	SlangCompiler &SlangCompiler::operator=(const SlangCompiler &other) {
		m_profile = other.m_profile;
		s_CompilerCount++;
		return *this;
	}

	constexpr SlangStage findShaderLanguage(ShaderStage shaderStage) {
		switch (shaderStage) {
			case ShaderStage::VERTEX:
				return SlangStage::SLANG_STAGE_VERTEX;
			case ShaderStage::TESS_CONTROL:
				return SlangStage::SLANG_STAGE_HULL;
			case ShaderStage::TESS_EVAL:
				return SlangStage::SLANG_STAGE_DOMAIN;
			case ShaderStage::GEOMETRY:
				return SlangStage::SLANG_STAGE_GEOMETRY;
			case ShaderStage::FRAGMENT:
				return SlangStage::SLANG_STAGE_FRAGMENT;
			case ShaderStage::COMPUTE:
				return SlangStage::SLANG_STAGE_COMPUTE;
			case ShaderStage::TASK:
				return SlangStage::SLANG_STAGE_AMPLIFICATION;
			case ShaderStage::MESH:
				return SlangStage::SLANG_STAGE_MESH;
			case ShaderStage::RAY_GEN:
			    return SlangStage::SLANG_STAGE_RAY_GENERATION;
			case ShaderStage::RAY_CLOSEST_HIT:
			    return SlangStage::SLANG_STAGE_CLOSEST_HIT;
			case ShaderStage::RAY_MISS:
			    return SlangStage::SLANG_STAGE_MISS;
			case ShaderStage::RAY_INTERSECTION:
				return SlangStage::SLANG_STAGE_INTERSECTION;
			case ShaderStage::RAY_ANY_HIT:
				return SlangStage::SLANG_STAGE_ANY_HIT;
			case ShaderStage::RAY_CALLABLE:
				return SlangStage::SLANG_STAGE_CALLABLE;
			default:
				return SlangStage::SLANG_STAGE_NONE;
		}
	}

	bool SlangCompiler::compileSource(ShaderStage shaderStage,
																		const std::string& shaderSource,
																		const ShaderCompiledFunction& compiled,
																		const std::filesystem::path& includePath) {
		slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};

		targetDesc.format = SLANG_SPIRV;

		switch (m_profile) {
			case SlangCompileProfile::GLSL:
				targetDesc.profile = s_GlobalSession->findProfile("glsl_460");
				sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
				break;
			case SlangCompileProfile::HLSL:
				targetDesc.profile = s_GlobalSession->findProfile("sm_5_0");
				sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_ROW_MAJOR;
				break;
			case SlangCompileProfile::SPIRV:
				targetDesc.profile = s_GlobalSession->findProfile("spirv_1_5");
				targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
				break;
			default:
				break;
		}

		sessionDesc.targets = &targetDesc;
		sessionDesc.targetCount = 1;

		const char *searchPath = includePath.c_str();
		sessionDesc.searchPaths = &searchPath;
		sessionDesc.searchPathCount = 1;

		Slang::ComPtr<slang::ISession> session;
		if (SLANG_FAILED(s_GlobalSession->createSession(sessionDesc, session.writeRef()))) {
			vkcv_log(LogLevel::ERROR, "Compiler session could not be created");
			return false;
		}

		Slang::ComPtr<slang::ICompileRequest> request;
		if (SLANG_FAILED(session->createCompileRequest(request.writeRef()))) {
			vkcv_log(LogLevel::ERROR, "Compilation request could not be created");
			return false;
		}

		const int entryPoint = request->addEntryPoint(
			0, "main", findShaderLanguage(shaderStage)
		);

		if (SLANG_FAILED(request->compile())) {
			vkcv_log(LogLevel::ERROR, "Compilation process failed");
			return false;
		}

		size_t size;
		const void *code = request->getEntryPointCode(entryPoint, &size);

		if ((size <= 0) || (!code)) {
			vkcv_log(LogLevel::ERROR, "Entry point could not be found");
			return false;
		}

		std::vector<uint32_t> spirv;
		spirv.resize(size / sizeof(uint32_t));
		memcpy(spirv.data(), code, spirv.size() * sizeof(uint32_t));

		const std::filesystem::path tmp_path = generateTemporaryFilePath();

		if (!writeBinaryToFile(tmp_path, spirv)) {
			vkcv_log(LogLevel::ERROR, "Spir-V could not be written to disk");
			return false;
		}
		
		if (compiled) {
			compiled(shaderStage, tmp_path);
		}
		
		std::filesystem::remove(tmp_path);
		return true;
	}
	
}
