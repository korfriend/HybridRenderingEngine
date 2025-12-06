#include "Dilator.h"

Dilator::Dilator(TextureTransformer* transformer, Renderer* renderer)
	: transformer(transformer)
	, renderer(renderer)
{
}

void Dilator::apply(FeedbackTexture* sourceFBT) {
	// 1. Bind Dilation Shader
	// 2. Set Uniforms (Texture Size, Source Texture)
	// 3. Render Quad

	sourceFBT->renderOperation(sourceFBT->getOffRenderTarget(), [this, sourceFBT](Renderer* r) {
		// Bind source texture to slot 0
		ID3D11ShaderResourceView* srv = sourceFBT->getRenderTarget()->srv.Get();
		r->getContext()->PSSetShaderResources(0, 1, &srv);

		// Render fullscreen quad with dilation shader
		r->renderFullscreenQuad();

		// Unbind
		ID3D11ShaderResourceView* nullSRV = nullptr;
		r->getContext()->PSSetShaderResources(0, 1, &nullSRV);
		});

	sourceFBT->swap();
}
