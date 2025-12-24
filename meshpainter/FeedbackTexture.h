#pragma once
#include "../gpu_common_res.h"

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Render target structure for DX11
struct RenderTarget {
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11RenderTargetView> rtv;
	ComPtr<ID3D11ShaderResourceView> srv;

	int width = 0;
	int height = 0;
	std::string id;

	RenderTarget() = default;
	~RenderTarget() = default;

	void create(__ID3D11Device* device, int w, int h);
	void bind(__ID3D11DeviceContext* context);
	static void unbind(__ID3D11DeviceContext* context);
};

// Texture info structure
struct TextureInfo {
	ComPtr<ID3D11ShaderResourceView> srv;
	int width = 0;
	int height = 0;
};

// FeedbackTexture class - manages ping-pong render targets
class FeedbackTexture {
public:
	FeedbackTexture(__ID3D11Device* device, __ID3D11DeviceContext* context, const TextureInfo& texture);
	~FeedbackTexture();

	// Create a new render target
	RenderTarget* makeTarget();

	// Swap the two render targets (ping-pong)
	void swap();

	// Perform rendering operation with custom render target
	using RenderOperationCallback = std::function<void(__ID3D11DeviceContext*)>;
	void renderOperation(RenderTarget* destination, RenderOperationCallback callback);

	// Accessors
	RenderTarget* getRenderTarget() { return renderTarget; }
	RenderTarget* getOffRenderTarget() { return offRenderTarget; }
	__ID3D11Device* getDevice() { return device; }
	__ID3D11DeviceContext* getContext() { return context; }

	// Fullscreen quad rendering
	void renderFullscreenQuad();

	// Static render target map for export tracking
	static std::map<std::string, RenderTarget*> renderTargetMap;

private:

	__ID3D11Device* device;
	__ID3D11DeviceContext* context;
	RenderTarget* renderTarget;
	RenderTarget* offRenderTarget;

	int width;
	int height;
};
