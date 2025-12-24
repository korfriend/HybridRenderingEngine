#include "FeedbackTexture.h"
#include <iostream>
#include <sstream>
#include <vector>

// Static member initialization
std::map<std::string, RenderTarget*> FeedbackTexture::renderTargetMap;

// Generate unique ID for render targets
static std::string generateUUID() {
	static int counter = 0;
	std::ostringstream oss;
	oss << "rt_" << counter++;
	return oss.str();
}

// ============================================================================
// RenderTarget Implementation
// ============================================================================

void RenderTarget::create(__ID3D11Device* device, int w, int h) {
	width = w;
	height = h;
	id = generateUUID();

	// 1. Create Texture
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
	if (FAILED(hr)) {
		std::cerr << "Failed to create texture for RT " << id << std::endl;
		return;
	}

	// 2. Create RTV
	hr = device->CreateRenderTargetView(texture.Get(), nullptr, rtv.GetAddressOf());
	if (FAILED(hr)) {
		std::cerr << "Failed to create RTV for RT " << id << std::endl;
		return;
	}

	// 3. Create SRV
	hr = device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf());
	if (FAILED(hr)) {
		std::cerr << "Failed to create SRV for RT " << id << std::endl;
		return;
	}

	std::cout << "Created DX11 RenderTarget [" << id << "] " << width << "x" << height << std::endl;
}

void RenderTarget::bind(__ID3D11DeviceContext* context) {
	ID3D11RenderTargetView* rtvPtr = rtv.Get();
	context->OMSetRenderTargets(1, &rtvPtr, nullptr); // No depth stencil for now

	D3D11_VIEWPORT vp;
	vp.Width = (float)width;
	vp.Height = (float)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);
}

void RenderTarget::unbind(__ID3D11DeviceContext* context) {
	ID3D11RenderTargetView* nullRTV = nullptr;
	context->OMSetRenderTargets(1, &nullRTV, nullptr);
}

// ============================================================================
// FeedbackTexture Implementation
// ============================================================================

FeedbackTexture::FeedbackTexture(__ID3D11Device* device, __ID3D11DeviceContext* context, const TextureInfo& texture)
	: device(device)
	, context(context)
	, width(texture.width)
	, height(texture.height)
{
	// Create two render targets for ping-pong rendering
	renderTarget = makeTarget();
	offRenderTarget = makeTarget();

	// Initial copy if source texture exists
	if (texture.srv) {
		renderTarget->bind(context);

		// Bind source texture
		ID3D11ShaderResourceView* srv = texture.srv.Get();
		context->PSSetShaderResources(0, 1, &srv);

		// Render quad
		renderFullscreenQuad();

		// Unbind
		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->PSSetShaderResources(0, 1, &nullSRV);

		RenderTarget::unbind(context);
	}
}

FeedbackTexture::~FeedbackTexture() {
	if (renderTarget) {
		renderTargetMap.erase(renderTarget->id);
		delete renderTarget;
	}
	if (offRenderTarget) {
		renderTargetMap.erase(offRenderTarget->id);
		delete offRenderTarget;
	}
}

RenderTarget* FeedbackTexture::makeTarget() {
	RenderTarget* rt = new RenderTarget();
	rt->create(device, width, height);
	renderTargetMap[rt->id] = rt;
	return rt;
}

void FeedbackTexture::swap() {
	RenderTarget* temp = renderTarget;
	renderTarget = offRenderTarget;
	offRenderTarget = temp;
}

void FeedbackTexture::renderOperation(RenderTarget* destination, RenderOperationCallback callback) {
	destination->bind(context);

	callback(context);

	RenderTarget::unbind(context);
}

void FeedbackTexture::renderFullscreenQuad() {

	ID3D11Buffer* nullVB = nullptr;
	UINT stride = 0;
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);
	context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context->IASetInputLayout(nullptr);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	context->Draw(4, 0);
}
