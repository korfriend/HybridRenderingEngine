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

FeedbackTexture::FeedbackTexture(const TextureInfo& texture, Renderer* renderer)
	: renderer(renderer)
	, width(texture.width)
	, height(texture.height)
{
	// Create two render targets for ping-pong rendering
	renderTarget = makeTarget();
	offRenderTarget = makeTarget();

	// Create fullscreen quad resources
	createFullscreenQuad();

	// Initial copy if source texture exists
	if (texture.srv) {
		RenderTarget* saveTarget = renderer->getRenderTarget();
		renderer->setRenderTarget(renderTarget);

		// Bind source texture
		ID3D11ShaderResourceView* srv = texture.srv.Get();
		renderer->getContext()->PSSetShaderResources(0, 1, &srv);

		// Render quad
		renderer->renderFullscreenQuad();

		// Unbind
		ID3D11ShaderResourceView* nullSRV = nullptr;
		renderer->getContext()->PSSetShaderResources(0, 1, &nullSRV);

		renderer->setRenderTarget(saveTarget);
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
	rt->create(renderer->getDevice(), width, height);
	renderTargetMap[rt->id] = rt;
	return rt;
}

void FeedbackTexture::swap() {
	RenderTarget* temp = renderTarget;
	renderTarget = offRenderTarget;
	offRenderTarget = temp;
}

void FeedbackTexture::renderOperation(RenderTarget* destination, RenderOperationCallback callback) {
	RenderTarget* saveTarg = renderer->getRenderTarget();

	renderer->setRenderTarget(destination);

	callback(renderer);

	renderer->setRenderTarget(saveTarg);
}

void FeedbackTexture::createFullscreenQuad() {
	struct Vertex {
		float x, y, z;
		float u, v;
	};

	Vertex vertices[] = {
		{ -1.0f,  1.0f, 0.0f,  0.0f, 0.0f }, // Top-Left
		{  1.0f,  1.0f, 0.0f,  1.0f, 0.0f }, // Top-Right
		{ -1.0f, -1.0f, 0.0f,  0.0f, 1.0f }, // Bottom-Left
		{  1.0f, -1.0f, 0.0f,  1.0f, 1.0f }, // Bottom-Right
	};

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices;

	renderer->getDevice()->CreateBuffer(&bd, &initData, quadVB.GetAddressOf());

	unsigned int indices[] = { 0, 1, 2, 2, 1, 3 };

	bd.ByteWidth = sizeof(unsigned int) * 6;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	initData.pSysMem = indices;

	renderer->getDevice()->CreateBuffer(&bd, &initData, quadIB.GetAddressOf());
}
