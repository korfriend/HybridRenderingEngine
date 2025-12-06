#include "MeshPainter.h"

MeshPainter::MeshPainter(Renderer* renderer)
	: renderer(renderer)
{
	// Create Constant Buffer
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(BrushConstants);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	renderer->getDevice()->CreateBuffer(&bd, nullptr, brushConstantBuffer.GetAddressOf());
}

MeshPainter::~MeshPainter() = default;

void MeshPainter::initialize(const TextureInfo& baseTexture) {
	textureTransformer = std::make_unique<TextureTransformer>(baseTexture, renderer);
	dilator = std::make_unique<Dilator>(textureTransformer.get(), renderer);
}

void MeshPainter::update(float deltaTime) {
	// Update logic if needed
}

void MeshPainter::draw(const float* brushPos, const float* brushColor, float size, float strength) {
	updateBrushConstants(brushPos, brushColor, size, strength);

	// Bind Constant Buffer
	ID3D11Buffer* cb = brushConstantBuffer.Get();
	renderer->getContext()->PSSetConstantBuffers(0, 1, &cb);

	// Render to UV map
	textureTransformer->renderUVMeshToTarget(textureTransformer->getFeedbackTexture()->getOffRenderTarget());

	// Apply Dilation
	dilator->apply(textureTransformer->getFeedbackTexture());
}

void MeshPainter::updateBrushConstants(const float* pos, const float* color, float size, float strength) {
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	renderer->getContext()->Map(brushConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

	BrushConstants* data = (BrushConstants*)mappedResource.pData;

	data->brushPoint[0] = pos[0];
	data->brushPoint[1] = pos[1];
	data->brushPoint[2] = pos[2];

	data->brushColor[0] = color[0];
	data->brushColor[1] = color[1];
	data->brushColor[2] = color[2];
	data->brushColor[3] = color[3];

	data->brushSize[0] = size;
	data->brushSize[1] = size;
	data->brushSize[2] = size;

	data->brushStrength = strength;
	data->brushHardness = 0.5f; // Default hardness

	renderer->getContext()->Unmap(brushConstantBuffer.Get(), 0);
}
