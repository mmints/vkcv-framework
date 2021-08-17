#pragma once
#include "AppSetup.hpp"
#include "MotionBlurSetup.hpp"

// selection for motion blur input and visualisation
enum class eMotionVectorMode : int {
	FullResolution = 0,
	MaxTile = 1,
	MaxTileNeighbourhood = 2,
	OptionCount = 3
};

static const char* MotionVectorModeLabels[3] = {
	"Full resolution",
	"Max tile",
	"Tile neighbourhood max" };

class MotionBlur {
public:

	bool initialize(vkcv::Core* corePtr, const uint32_t targetWidth, const uint32_t targetHeight);
	void setResolution(const uint32_t targetWidth, const uint32_t targetHeight);

	vkcv::ImageHandle render(
		const vkcv::CommandStreamHandle cmdStream,
		const vkcv::ImageHandle         motionBufferFullRes,
		const vkcv::ImageHandle         colorBuffer,
		const vkcv::ImageHandle         depthBuffer,
		const eMotionVectorMode         motionVectorMode,
		const float                     cameraNear,
		const float                     cameraFar,
		const float                     deltaTimeSeconds,
		const float                     cameraShutterSpeedInverse,
		const float                     motionBlurMinVelocity);

	vkcv::ImageHandle MotionBlur::renderMotionVectorVisualisation(
		const vkcv::CommandStreamHandle cmdStream,
		const vkcv::ImageHandle         motionBuffer,
		const eMotionVectorMode         debugView,
		const float                     velocityRange);

private:
	// computes max per tile and neighbourhood tile max
	void computeMotionTiles(
		const vkcv::CommandStreamHandle cmdStream,
		const vkcv::ImageHandle         motionBufferFullRes);

	vkcv::Core* m_core;

	MotionBlurRenderTargets m_renderTargets;
	vkcv::SamplerHandle     m_nearestSampler;

	ComputePassHandles m_motionBlurPass;
	ComputePassHandles m_motionVectorMaxPass;
	ComputePassHandles m_motionVectorMaxNeighbourhoodPass;
	ComputePassHandles m_motionVectorVisualisationPass;
};