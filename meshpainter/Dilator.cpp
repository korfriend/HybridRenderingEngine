#include "Dilator.h"

Dilator::Dilator(__ID3D11Device* device, __ID3D11DeviceContext* context)
	: device(device)
	, context(context)
{
}

void Dilator::apply(FeedbackTexture* sourceFBT) {
	if (!sourceFBT || !dilationVS || !dilationPS)
		return;

	sourceFBT->renderOperation(sourceFBT->getOffRenderTarget(), [this, sourceFBT](__ID3D11DeviceContext* ctx) {
		// Bind dilation shaders
		if (dilationVS)
			ctx->VSSetShader(dilationVS.Get(), nullptr, 0);
		if (dilationPS)
			ctx->PSSetShader(dilationPS.Get(), nullptr, 0);

		// Bind input layout
		if (dilationInputLayout)
			ctx->IASetInputLayout(dilationInputLayout.Get());

		// Bind source texture to slot 0 (tex2d_base : register(t0))
		ID3D11ShaderResourceView* srv = sourceFBT->getRenderTarget()->srv.Get();
		ctx->PSSetShaderResources(0, 1, &srv);

		// Render fullscreen quad with dilation shader
		sourceFBT->renderFullscreenQuad();

		// Unbind
		ID3D11ShaderResourceView* nullSRV = nullptr;
		ctx->PSSetShaderResources(0, 1, &nullSRV);
	});

	sourceFBT->swap();
}
