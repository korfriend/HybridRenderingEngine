#pragma once
#include "gpures_helper.h"
#include "TextureTransformer.h"
#include "Dilator.h"
#include <d3d11.h>
#include <wrl/client.h>

class MeshPainter {
public:
	MeshPainter(Renderer* renderer);
	~MeshPainter();

	void initialize(const TextureInfo& baseTexture);
	void update(float deltaTime);
	void draw(const float* brushPos, const float* brushColor, float size, float strength);

private:
	Renderer* renderer;
	std::unique_ptr<TextureTransformer> textureTransformer;
	std::unique_ptr<Dilator> dilator;

	// Brush Constants
	struct BrushConstants {
		float brushColor[4];
		float brushPoint[3];
		float brushHardness;
		float brushSize[3];
		float brushStrength;
	};

	ComPtr<ID3D11Buffer> brushConstantBuffer;

	void updateBrushConstants(const float* pos, const float* color, float size, float strength);
};
