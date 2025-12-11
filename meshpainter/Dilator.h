#pragma once
#include "FeedbackTexture.h"
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class Dilator {
public:
	Dilator(__ID3D11Device* device, __ID3D11DeviceContext* context);
	~Dilator() = default;

	void apply(FeedbackTexture* sourceFBT);

	// Set dilation shaders (loaded externally)
	void setDilationShaders(ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* layout) {
		dilationVS = vs;
		dilationPS = ps;
		dilationInputLayout = layout;
	}

private:
	__ID3D11Device* device;
	__ID3D11DeviceContext* context;

	// Dilation shader resources
	ComPtr<ID3D11VertexShader> dilationVS;
	ComPtr<ID3D11PixelShader> dilationPS;
	ComPtr<ID3D11InputLayout> dilationInputLayout;
};
