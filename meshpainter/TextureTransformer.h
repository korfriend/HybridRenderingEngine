#pragma once
#include "FeedbackTexture.h"
#include <memory>

class TextureTransformer {
public:
	TextureTransformer(const TextureInfo& texture, Renderer* renderer);
	~TextureTransformer();

	void renderUVMeshToTarget(RenderTarget* destination);

	FeedbackTexture* getFeedbackTexture() { return feedbackTexture.get(); }

private:
	Renderer* renderer;
	std::unique_ptr<FeedbackTexture> feedbackTexture;

	// In a real engine, this would be a Scene object
	// For this port, we'll assume the Renderer handles the "UV Scene" 
	// or we pass the mesh to renderUVMeshToTarget
};
