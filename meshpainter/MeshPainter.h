#pragma once
#include "../gpu_common_res.h"
#include "PaintResourceManager.h"
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

struct MeshParams {
	ID3D11Buffer* vbMesh;
	ID3D11ShaderResourceView* uvBufferSRV;  // UV buffer for mesh painter (t61)
	std::string inputLayerDesc;	// "P", "PN", "PT", "PNT", "PNTC"
	uint stride;
	uint offset;
	vmobjects::PrimitiveData* primData;
};

class MeshPainter {
public:
	MeshPainter(__ID3D11Device* device, __ID3D11DeviceContext* context);
	~MeshPainter();

	PaintResourceManager* getPaintResourceManager() { return paintManager.get(); }

	void loadShaders();

	// Paint on a specific actor at world position
	// Requires mesh data for UV lookup
	void paintOnActor(
		int actorId, 
		const MeshParams& meshParams,
		const vmmat44f& matOS2WS,
		const BrushParams& brush,
		const RayHitResult& hitResult, const bool paint
	);

	// Compute UV from ray hit on triangle mesh
	// Returns true if valid UV was computed
	// If painterUVs is provided, uses painter UVs; otherwise falls back to TEXCOORD0
	static bool computeUVFromHit(
		vmobjects::PrimitiveData* primData,
		int triangleIndex,
		float baryU, float baryV,
		float outUV[2],
		const std::vector<float>* painterUVs = nullptr
	);

	// Ray-triangle intersection for picking
	// Returns hit result with barycentric coordinates
	// If actorId >= 0, uses painter UVs for UV calculation
	RayHitResult raycastMesh(
		vmobjects::PrimitiveData* primData,
		const vmmat44f& matOS2WS,
		const vmfloat3& rayOrigin,
		const vmfloat3& rayDir,
		const geometrics::BVH* bvh,
		int actorId = -1
	);

private:
	__ID3D11Device* device;
	__ID3D11DeviceContext* context;

	std::unique_ptr<PaintResourceManager> paintManager;

	// Brush shader resources
	struct BrushConstants {
		float brushColor[4];     // 16 bytes

		float brushCenter[3];    // 12 bytes (world space center)
		float brushStrength;     // 4 bytes

		float brushRadius[3];    // 12 bytes (world space radius)
		float brushHardness;     // 4 bytes

		float matOS2WS[16];      // 64 bytes (4x4 matrix)

		int blendMode;           // 4 bytes
		uint painterFlags;          // 4 bytes
		float padding0;          // 4 bytes
		float padding1;          // 4 bytes
	};  // Total: 128 bytes (matches HLSL CB_Brush)

	ComPtr<ID3D11Buffer> brushConstantBuffer;

	ComPtr<ID3D11VertexShader> brushVS;
	ComPtr<ID3D11PixelShader> brushPS;

	// Input Layouts for brush shader (created from brushVS bytecode)
	ComPtr<ID3D11InputLayout> brushInputLayout_P;
	ComPtr<ID3D11InputLayout> brushInputLayout_PN;
	ComPtr<ID3D11InputLayout> brushInputLayout_PT;
	ComPtr<ID3D11InputLayout> brushInputLayout_PNT;

	ComPtr<ID3D11VertexShader> dilationVS;	//	quad
	ComPtr<ID3D11PixelShader> dilationPS;

	void createBrushResources();
	ID3D11InputLayout* getBrushInputLayout(const std::string& layoutDesc);  // Get input layout by name (P/PN/PT/PNT)
	void updateBrushConstants(const BrushParams& brush, const float uv[2], float uvRadius[2], const vmmat44f& matOS2WS, const bool paint);
	void renderBrushStroke(FeedbackTexture* hoverTex,
		FeedbackTexture* paintTex,
		const MeshParams& meshParams,
		const float uv[2], float uvRadius[2]);
	void applyDilation(FeedbackTexture* sourceFBT);
};
