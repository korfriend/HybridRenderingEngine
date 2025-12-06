#pragma once
#include "FeedbackTexture.h"
#include "TextureTransformer.h"

class Dilator {
public:
	Dilator(TextureTransformer* transformer, Renderer* renderer);
	~Dilator() = default;

	void apply(FeedbackTexture* sourceFBT);

private:
	Renderer* renderer;
	TextureTransformer* transformer;

	// In a real implementation, we would need:
	// - UVMask generation logic
	// - Dilation shader
};
