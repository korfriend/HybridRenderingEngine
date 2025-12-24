#pragma once
#include "FeedbackTexture.h"
#include <unordered_map>
#include <memory>

// Blend modes for paint layer compositing
enum class PaintBlendMode {
	NORMAL = 0,      // Alpha blend (lerp)
	MULTIPLY = 1,    // Multiply
	ADDITIVE = 2,    // Add
	OVERLAY = 3,     // Overlay
	SOFT_LIGHT = 4,  // Soft light
	SCREEN = 5,      // Screen
};

struct UVBufferInfo {
	std::vector<float> vbPainterUVs_TriVertex;  // Triangle-vertex indexed UVs (3 UVs per triangle)
	uint numVertrices;
	ComPtr<ID3D11Buffer> buffer;
	ComPtr<ID3D11ShaderResourceView> srv;

	~UVBufferInfo()
	{
		buffer.Reset();
		srv.Reset();
	}
};

// Per-actor paint data
struct ActorPaintData {
	ullong timeStamp = 0;
	std::unique_ptr<UVBufferInfo> vbUVs;
	std::unique_ptr<FeedbackTexture> hoverTexture;
	std::unique_ptr<FeedbackTexture> paintTexture;
	int width = 0;
	int height = 0;
	PaintBlendMode blendMode = PaintBlendMode::NORMAL;
	float opacity = 1.0f;
	bool isDirty = false;  // Flag to track if paint was modified

	ActorPaintData() = default;
	~ActorPaintData() = default;

	// Move only
	ActorPaintData(ActorPaintData&&) = default;
	ActorPaintData& operator=(ActorPaintData&&) = default;
	ActorPaintData(const ActorPaintData&) = delete;
	ActorPaintData& operator=(const ActorPaintData&) = delete;
};

// Manages paint textures for multiple actors
class PaintResourceManager {
public:
	PaintResourceManager(__ID3D11Device* device, __ID3D11DeviceContext* context);
	~PaintResourceManager();

	ActorPaintData* createPaintResource(int actorId, int width, int height,
		const std::vector<float>& vbPainterUVs_TriVertex);
	ActorPaintData* getPaintResource(int actorId);

	// Check if actor has paint texture
	bool hasPaintResource(int actorId) const;

	// Remove paint texture for an actor
	void removePaintResource(int actorId);

	// Clear all paint textures
	void clearAll();

	// Get/Set blend mode for an actor
	PaintBlendMode getBlendMode(int actorId) const;
	void setBlendMode(int actorId, PaintBlendMode mode);

	// Get/Set opacity for an actor
	float getOpacity(int actorId) const;
	void setOpacity(int actorId, float opacity);

	// Mark paint as dirty (modified)
	void markDirty(int actorId);
	bool isDirty(int actorId) const;
	void clearDirty(int actorId);

	// Get paint texture dimensions
	bool getPaintDimensions(int actorId, int& outWidth, int& outHeight) const;

private:
	__ID3D11Device* device;
	__ID3D11DeviceContext* context;
	std::unordered_map<int, ActorPaintData> actorPaintResources;
};
