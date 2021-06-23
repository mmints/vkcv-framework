#include <vkcv/ImageConfig.hpp>
#include <vkcv/Logger.hpp>

namespace vkcv {
	vk::SampleCountFlagBits msaaToVkSampleCountFlag(Multisampling msaa) {
		switch (msaa) {
		case Multisampling::None:   return vk::SampleCountFlagBits::e1;
		case Multisampling::MSAA2X: return vk::SampleCountFlagBits::e2;
		case Multisampling::MSAA4X: return vk::SampleCountFlagBits::e4;
		case Multisampling::MSAA8X: return vk::SampleCountFlagBits::e8;
		default: vkcv_log(vkcv::LogLevel::ERROR, "Unknown Multisampling enum setting"); return vk::SampleCountFlagBits::e1;
		}
	}
}