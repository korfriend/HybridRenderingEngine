#include "PaintResourceManager.h"
#include <iostream>

PaintResourceManager::PaintResourceManager(__ID3D11Device* device, __ID3D11DeviceContext* context)
	: device(device)
	, context(context)
{
}

PaintResourceManager::~PaintResourceManager() {
	clearAll();
}

ActorPaintData* PaintResourceManager::getPaintResource(int actorId) {
	auto it = actorPaintResources.find(actorId);

	if (it == actorPaintResources.end()) {
		return nullptr;
	}

	return &it->second;
}

ActorPaintData* PaintResourceManager::createPaintResource(int actorId, int width, int height, const std::vector<float>& vbPainterUVs) {
	auto it = actorPaintResources.find(actorId);

	if (it != actorPaintResources.end()) {
		// Check if dimensions match, if not recreate
		if (it->second.width == width && it->second.height == height) {
			return &it->second;
		}
		// Dimensions changed, remove old texture
		actorPaintResources.erase(it);
	}

	// Create new paint texture
	TextureInfo emptyTexInfo;
	emptyTexInfo.width = width;
	emptyTexInfo.height = height;
	emptyTexInfo.srv = nullptr;  // Start with empty/transparent texture

	ActorPaintData paintData;
	paintData.width = width;
	paintData.height = height;
	paintData.feedbackTexture = std::make_unique<FeedbackTexture>(device, context, emptyTexInfo);
	paintData.vbUVs = std::make_unique<UVBufferInfo>();
	paintData.blendMode = PaintBlendMode::NORMAL;
	paintData.opacity = 1.0f;
	paintData.isDirty = false;

	paintData.vbUVs->vbPainterUVs = vbPainterUVs;

	// Create UV StructuredBuffer and SRV
	if (!vbPainterUVs.empty()) {
		paintData.vbUVs->numVertrices = static_cast<uint>(vbPainterUVs.size() / 2);  // float2 per vertex

		// Create StructuredBuffer (immutable for best GPU performance)
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.ByteWidth = static_cast<UINT>(vbPainterUVs.size() * sizeof(float));
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(float) * 2;  // float2

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = vbPainterUVs.data();

		HRESULT hr = device->CreateBuffer(&bufferDesc, &initData, paintData.vbUVs->buffer.GetAddressOf());

		if (SUCCEEDED(hr)) {
			// Create SRV
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = paintData.vbUVs->numVertrices;

			device->CreateShaderResourceView(paintData.vbUVs->buffer.Get(), &srvDesc, paintData.vbUVs->srv.GetAddressOf());
		}
	}

	auto result = actorPaintResources.emplace(actorId, std::move(paintData));

	std::cout << "Created paint texture for actor " << actorId
	          << " [" << width << "x" << height << "]" << std::endl;

	return &result.first->second;
}

ID3D11ShaderResourceView* PaintResourceManager::getPaintTextureSRV(int actorId) {
	auto it = actorPaintResources.find(actorId);
	if (it == actorPaintResources.end()) {
		return nullptr;
	}

	RenderTarget* rt = it->second.feedbackTexture->getRenderTarget();
	if (!rt) {
		return nullptr;
	}

	return rt->srv.Get();
}

ID3D11ShaderResourceView* PaintResourceManager::getPaintUVsSRV(int actorId)
{
	auto it = actorPaintResources.find(actorId);
	if (it == actorPaintResources.end()) {
		return nullptr;
	}

	return it->second.vbUVs->srv.Get();
}

bool PaintResourceManager::hasPaintResource(int actorId) const {
	return actorPaintResources.find(actorId) != actorPaintResources.end();
}

void PaintResourceManager::removePaintResource(int actorId) {
	auto it = actorPaintResources.find(actorId);
	if (it != actorPaintResources.end()) {
		std::cout << "Removed paint texture for actor " << actorId << std::endl;
		actorPaintResources.erase(it);
	}
}

void PaintResourceManager::clearAll() {
	actorPaintResources.clear();
	std::cout << "Cleared all paint textures" << std::endl;
}

PaintBlendMode PaintResourceManager::getBlendMode(int actorId) const {
	auto it = actorPaintResources.find(actorId);
	if (it == actorPaintResources.end()) {
		return PaintBlendMode::NORMAL;
	}
	return it->second.blendMode;
}

void PaintResourceManager::setBlendMode(int actorId, PaintBlendMode mode) {
	auto it = actorPaintResources.find(actorId);
	if (it != actorPaintResources.end()) {
		it->second.blendMode = mode;
	}
}

float PaintResourceManager::getOpacity(int actorId) const {
	auto it = actorPaintResources.find(actorId);
	if (it == actorPaintResources.end()) {
		return 1.0f;
	}
	return it->second.opacity;
}

void PaintResourceManager::setOpacity(int actorId, float opacity) {
	auto it = actorPaintResources.find(actorId);
	if (it != actorPaintResources.end()) {
		it->second.opacity = std::max(0.0f, std::min(1.0f, opacity));
	}
}

void PaintResourceManager::markDirty(int actorId) {
	auto it = actorPaintResources.find(actorId);
	if (it != actorPaintResources.end()) {
		it->second.isDirty = true;
	}
}

bool PaintResourceManager::isDirty(int actorId) const {
	auto it = actorPaintResources.find(actorId);
	if (it == actorPaintResources.end()) {
		return false;
	}
	return it->second.isDirty;
}

void PaintResourceManager::clearDirty(int actorId) {
	auto it = actorPaintResources.find(actorId);
	if (it != actorPaintResources.end()) {
		it->second.isDirty = false;
	}
}

bool PaintResourceManager::getPaintDimensions(int actorId, int& outWidth, int& outHeight) const {
	auto it = actorPaintResources.find(actorId);
	if (it == actorPaintResources.end()) {
		outWidth = 0;
		outHeight = 0;
		return false;
	}
	outWidth = it->second.width;
	outHeight = it->second.height;
	return true;
}

RenderTarget* PaintResourceManager::getOffRenderTarget(int actorId) {
	auto it = actorPaintResources.find(actorId);
	if (it == actorPaintResources.end()) {
		return nullptr;
	}
	return it->second.feedbackTexture->getOffRenderTarget();
}

void PaintResourceManager::swapBuffers(int actorId) {
	auto it = actorPaintResources.find(actorId);
	if (it != actorPaintResources.end()) {
		it->second.feedbackTexture->swap();
	}
}
