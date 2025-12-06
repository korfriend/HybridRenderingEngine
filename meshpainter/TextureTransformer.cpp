#include "TextureTransformer.h"

TextureTransformer::TextureTransformer(const TextureInfo& texture, Renderer* renderer)
	: renderer(renderer)
{
	feedbackTexture = std::make_unique<FeedbackTexture>(texture, renderer);
}

TextureTransformer::~TextureTransformer() = default;

void TextureTransformer::renderUVMeshToTarget(RenderTarget* destination) {
	feedbackTexture->renderOperation(destination, [this](Renderer* r) {
		// Here we would render the UV scene
		// For the purpose of this port, we assume the renderer has the context
		// to render the "UV Mesh" which flattens the 3D model onto the UV plane.

		// r->renderUVMesh(); // Placeholder for actual rendering call
		});

	feedbackTexture->swap();
}
