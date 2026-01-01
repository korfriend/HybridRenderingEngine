#include "PaintResourceManager.h"
#include "gpures_helper.h"
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

ActorPaintData* PaintResourceManager::createPaintResource(int actorId, int width, int height) {
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
	paintData.blendMode = PaintBlendMode::NORMAL;
	paintData.opacity = 1.0f;
	paintData.isDirty = false;

	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	grd_helper::GetPSOManager()->dx11DeviceImmContext->ClearRenderTargetView(paintData.paintTexture->getRenderTarget()->rtv.Get(), clr_float_zero_4);
	grd_helper::GetPSOManager()->dx11DeviceImmContext->ClearRenderTargetView(paintData.paintTexture->getOffRenderTarget()->rtv.Get(), clr_float_zero_4);

	auto result = actorPaintResources.emplace(actorId, std::move(paintData));

	vzlog("Created paint texture for actor (%d) [%d][%d]", actorId, width, height);

	return &result.first->second;
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