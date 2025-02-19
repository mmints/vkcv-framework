
#include "vkcv/shader/SlimCompiler.hpp"

#include <vkcv/File.hpp>
#include <vkcv/Logger.hpp>

extern "C" {
	#include <shady/driver.h>
	#include <shady/ir.h>
}

namespace vkcv::shader {

	SlimCompiler::SlimCompiler(SlimCompileTarget target)
	: ShadyCompiler(), m_target(target) {}

	static bool shadyCompileModule(Module* module,
																 ShaderStage shaderStage,
																 const std::string& shaderSource,
																 const ShaderCompiledFunction &compiled,
																 const std::filesystem::path &includePath) {
		CompilerConfig compiler = shd_default_compiler_config();
		ShadyErrorCodes codes = shd_driver_load_source_file(
			&compiler,
			SrcSlim,
			shaderSource.length(),
			shaderSource.c_str(),
			"main",
			&module
		);

		switch (codes) {
			case NoError:
				break;
			case MissingInputArg:
			case MissingOutputArg:
			case InputFileDoesNotExist:
			case InputFileIOError:
			case MissingDumpCfgArg:
			case MissingDumpIrArg:
			case IncorrectLogLevel:
			case InvalidTarget:
			case ClangInvocationFailed:
			default:
				vkcv_log(LogLevel::ERROR, "Unknown error while loading shader");
				return false;
		}

		const std::filesystem::path tmp_path = generateTemporaryFilePath();

		DriverConfig config = shd_default_driver_config();

		config.target = TgtSPV;
		config.output_filename = tmp_path.string().c_str();

		codes = shd_driver_compile(&config, module);
		shd_destroy_driver_config(&config);

		switch (codes) {
			case NoError:
				break;
			case MissingInputArg:
			case MissingOutputArg:
			case InputFileDoesNotExist:
			case InputFileIOError:
			case MissingDumpCfgArg:
			case MissingDumpIrArg:
			case IncorrectLogLevel:
			case InvalidTarget:
			case ClangInvocationFailed:
			default:
				vkcv_log(LogLevel::ERROR, "Unknown error while compiling shader");
				return false;
		}

		if (compiled) {
			compiled(shaderStage, tmp_path);
		}

		std::filesystem::remove(tmp_path);
		return true;
	}

    static bool shadyCompileArena(IrArena* arena,
																	ShaderStage shaderStage,
																	const std::string& shaderSource,
																	const ShaderCompiledFunction &compiled,
																	const std::filesystem::path &includePath) {
		Module* module = shd_new_module(arena, "slim_module");

		if (nullptr == module) {
			vkcv_log(LogLevel::ERROR, "Module could not be created");
			return false;
		}

		return shadyCompileModule(module, shaderStage, shaderSource, compiled, includePath);
	}

	bool SlimCompiler::compileSource(ShaderStage shaderStage,
																	 const std::string& shaderSource,
																	 const ShaderCompiledFunction& compiled,
																	 const std::filesystem::path& includePath) {
		if (ShaderStage::COMPUTE != shaderStage) {
			vkcv_log(LogLevel::ERROR, "Shader stage not supported");
			return false;
		}

		TargetConfig target = shd_default_target_config();
		ArenaConfig config = shd_default_arena_config(&target);
		IrArena* arena = shd_new_ir_arena(&config);

		if (nullptr == arena) {
			vkcv_log(LogLevel::ERROR, "IR Arena could not be created");
			return false;
		}

		bool result = shadyCompileArena(arena, shaderStage, shaderSource, compiled, includePath);

		shd_destroy_ir_arena(arena);
		return result;
	}

}
