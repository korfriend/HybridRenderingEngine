#include "PaintResourceManager.h"
#include "vzm2/Backlog.h"
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

ActorPaintData* PaintResourceManager::createPaintResource(int actorId, int width, int height,
	const std::vector<float>& vbPainterUVs_TriVertex) {
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
	paintData.hoverTexture = std::make_unique<FeedbackTexture>(device, context, emptyTexInfo);
	paintData.paintTexture = std::make_unique<FeedbackTexture>(device, context, emptyTexInfo);
	paintData.vbUVs = std::make_unique<UVBufferInfo>();
	paintData.blendMode = PaintBlendMode::NORMAL;
	paintData.opacity = 1.0f;
	paintData.isDirty = false;

	paintData.vbUVs->vbPainterUVs_TriVertex = vbPainterUVs_TriVertex;  // Triangle-vertex indexed UVs (3 UVs per triangle)

	// Create UV StructuredBuffer and SRV
	if (!vbPainterUVs_TriVertex.empty()) {
		paintData.vbUVs->numVertrices = static_cast<uint>(vbPainterUVs_TriVertex.size() / 2);  // float2 per UV

		// Create StructuredBuffer (immutable for best GPU performance)
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.ByteWidth = static_cast<UINT>(vbPainterUVs_TriVertex.size() * sizeof(float));
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(float) * 2;  // float2

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = vbPainterUVs_TriVertex.data();

		HRESULT hr = device->CreateBuffer(&bufferDesc, &initData, paintData.vbUVs->buffer.GetAddressOf());

		if (SUCCEEDED(hr)) {
			// Create SRV for StructuredBuffer
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;  // Required for structured buffers
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;  // Must use BUFFEREX for StructuredBuffer
			srvDesc.BufferEx.FirstElement = 0;
			srvDesc.BufferEx.NumElements = paintData.vbUVs->numVertrices;
			srvDesc.BufferEx.Flags = 0;  // 0 for StructuredBuffer (D3D11_BUFFEREX_SRV_FLAG_RAW for ByteAddressBuffer)

			device->CreateShaderResourceView(paintData.vbUVs->buffer.Get(), &srvDesc, paintData.vbUVs->srv.GetAddressOf());
		}
	}

	auto result = actorPaintResources.emplace(actorId, std::move(paintData));

	vzlog("Created paint texture for actor (%d) [%d][%d]", actorId, width, height);

	return &result.first->second;
}

//ID3D11ShaderResourceView* PaintResourceManager::getHoverTextureSRV(int actorId) {
//	auto it = actorPaintResources.find(actorId);
//	if (it == actorPaintResources.end()) {
//		return nullptr;
//	}
//
//	RenderTarget* rt = it->second.hoverTexture->getRenderTarget();
//	if (!rt) {
//		return nullptr;
//	}
//
//	return rt->srv.Get();
//}
//
//ID3D11ShaderResourceView* PaintResourceManager::getPainterUVsSRV(int actorId)
//{
//	auto it = actorPaintResources.find(actorId);
//	if (it == actorPaintResources.end()) {
//		return nullptr;
//	}
//
//	return it->second.vbUVs->srv.Get();
//}

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