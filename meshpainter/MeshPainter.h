#pragma once
#include "../gpu_common_res.h"
#include "PaintResourceManager.h"
#include "Dilator.h"
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Brush parameters for painting
struct BrushParams {
	float position[3];     // World space position (from ray hit)
	float color[4];        // RGBA color
	float size;            // Brush radius in world units
	float strength;        // Brush opacity/strength [0-1]
	float hardness;        // Edge falloff [0-1], 1 = hard edge
	PaintBlendMode blendMode;

	BrushParams() : size(1.0f), strength(1.0f), hardness(0.5f), blendMode(PaintBlendMode::NORMAL) {
		position[0] = position[1] = position[2] = 0.0f;
		color[0] = color[1] = color[2] = color[3] = 1.0f;
	}
};

// Ray hit result for world-to-UV conversion
struct RayHitResult {
	bool hit;
	int actorId;
	int triangleIndex;
	float barycentricU;
	float barycentricV;
	float hitDistance;
	float worldPos[3];
	float uv[2];           // Computed UV coordinate at hit point

	RayHitResult() : hit(false), actorId(-1), triangleIndex(-1),
		barycentricU(0), barycentricV(0), hitDistance(FLT_MAX) {
		worldPos[0] = worldPos[1] = worldPos[2] = 0.0f;
		uv[0] = uv[1] = 0.0f;
	}
};

class MeshPainter {
public:
	MeshPainter(__ID3D11Device* device, __ID3D11DeviceContext* context);
	~MeshPainter();

	void initialize(const TextureInfo& baseTexture);

	PaintResourceManager* getPaintResourceManager() { return paintManager; }

	// Set brush shaders (loaded externally)
	void setBrushShaders(ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* layout) {
		brushVS = vs;
		brushPS = ps;
		brushInputLayout = layout;
	}

	// Set dilation shaders (loaded externally)
	void setDilationShaders(ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* layout) {
		if (dilator) {
			dilator->setDilationShaders(vs, ps, layout);
		}
	}

	// Paint on a specific actor at world position
	// Requires mesh data for UV lookup
	void paintOnActor(
		int actorId,
		const vmobjects::PrimitiveData* primData,
		const vmmat44f& matOS2WS,
		const BrushParams& brush,
		const RayHitResult& hitResult
	);

	// Compute UV from ray hit on triangle mesh
	// Returns true if valid UV was computed
	static bool computeUVFromHit(
		vmobjects::PrimitiveData* primData,
		int triangleIndex,
		float baryU, float baryV,
		float outUV[2]
	);

	// Ray-triangle intersection for picking
	// Returns hit result with barycentric coordinates
	static RayHitResult raycastMesh(
		vmobjects::PrimitiveData* primData,
		const vmmat44f& matOS2WS,
		const vmfloat3& rayOrigin,
		const vmfloat3& rayDir,
		const geometrics::BVH* bvh
	);

private:
	__ID3D11Device* device;
	__ID3D11DeviceContext* context;
	PaintResourceManager* paintManager;

	std::unique_ptr<Dilator> dilator;

	// Brush shader resources
	struct BrushConstants {
		float brushColor[4];     // 16 bytes
		float brushCenter[2];    // 8 bytes (UV space center)
		float brushRadius[2];    // 8 bytes (UV space radius x, y)
		float brushStrength;     // 4 bytes
		float brushHardness;     // 4 bytes
		int blendMode;           // 4 bytes
		float padding;           // 4 bytes
	};  // Total: 48 bytes (multiple of 16)

	ComPtr<ID3D11Buffer> brushConstantBuffer;
	ComPtr<ID3D11VertexShader> brushVS;
	ComPtr<ID3D11PixelShader> brushPS;
	ComPtr<ID3D11InputLayout> brushInputLayout;
	ComPtr<ID3D11BlendState> brushBlendState;

	void createBrushResources();
	void loadBrushShaders();
	void updateBrushConstants(const BrushParams& brush, const float uv[2], float uvRadius[2]);
	void renderBrushStroke(FeedbackTexture* feedbackTex, const float uv[2], float uvRadius[2]);
};
