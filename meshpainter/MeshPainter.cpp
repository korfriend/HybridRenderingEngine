#include "MeshPainter.h"
#include "../vzm2/Geometrics.h"
#include "../vzm2/Backlog.h"
#include "gpures_helper.h"
#include <cmath>
#include <iostream>


// ============================================================================
// MeshPainter Implementation
// ============================================================================

MeshPainter::MeshPainter(__ID3D11Device* device, __ID3D11DeviceContext* context)
	: device(device), context(context), paintManager(nullptr) {
	createBrushResources();

	paintManager = std::make_unique<PaintResourceManager>(device, context);
	loadShaders();
}

MeshPainter::~MeshPainter() {
	paintManager.reset();
}

void MeshPainter::loadShaders()
{
	static bool is_loaded = false;
	if (!is_loaded)
	{
		HMODULE hModule = GetModuleHandleA(__DLLNAME);

#define COMPILE_SHADER_RCDATA_ATTACH(ShaderIF, RC_ID, PROFILE, OUT_COM_PTR)        \
    do {                                                                           \
        ShaderIF* _shader = nullptr;                                               \
        HRESULT _hr = grd_helper::PresetCompiledShader(                            \
            device, hModule, MAKEINTRESOURCE(RC_ID), PROFILE,                      \
            (ID3D11DeviceChild**)&_shader, nullptr, 0, nullptr);                   \
        if (FAILED(_hr) || !_shader) {                                             \
            VMSAFE_RELEASE(_shader);                                               \
            vzlog_error("Compile Error!" #OUT_COM_PTR );			               \
            return;                                                                \
        }                                                                          \
        (OUT_COM_PTR).Attach(_shader);                                             \
    } while (0)

		COMPILE_SHADER_RCDATA_ATTACH(ID3D11VertexShader, IDR_RCDATA80001, "vs_5_0", brushVS);
		COMPILE_SHADER_RCDATA_ATTACH(ID3D11VertexShader, IDR_RCDATA80002, "vs_5_0", dilationVS);
		COMPILE_SHADER_RCDATA_ATTACH(ID3D11PixelShader, IDR_RCDATA80003, "ps_5_0", brushPS);
		COMPILE_SHADER_RCDATA_ATTACH(ID3D11PixelShader, IDR_RCDATA80008, "ps_5_0", dilationPS);

		{
			// Define input element descriptors
			D3D11_INPUT_ELEMENT_DESC layoutP[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			D3D11_INPUT_ELEMENT_DESC layoutPN[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			D3D11_INPUT_ELEMENT_DESC layoutPT[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			D3D11_INPUT_ELEMENT_DESC layoutPNT[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			HRSRC hrSrc = FindResource(hModule, MAKEINTRESOURCE(IDR_RCDATA80001), RT_RCDATA);
			HGLOBAL hGlobalByte = LoadResource(hModule, hrSrc);
			LPVOID pdata = LockResource(hGlobalByte);
			ullong ullFileSize = SizeofResource(hModule, hrSrc);

			device->CreateInputLayout(layoutP, 1, pdata, ullFileSize, brushInputLayout_P.GetAddressOf());
			device->CreateInputLayout(layoutPN, 2, pdata, ullFileSize, brushInputLayout_PN.GetAddressOf());
			device->CreateInputLayout(layoutPT, 2, pdata, ullFileSize, brushInputLayout_PT.GetAddressOf());
			device->CreateInputLayout(layoutPNT, 3, pdata, ullFileSize, brushInputLayout_PNT.GetAddressOf());
		}

		is_loaded = true;
		return;
	}

	char ownPth[2048];
	GetModuleFileNameA(NULL, ownPth, (sizeof(ownPth)));
	string exe_path = ownPth;
	size_t pos = 0;
	std::string token;
	string delimiter = "\\";
	string hlslobj_path = "";
	while ((pos = exe_path.find(delimiter)) != std::string::npos) {
		token = exe_path.substr(0, pos);
		if (token.find(".exe") != std::string::npos) break;
		hlslobj_path += token + "\\";
		exe_path.erase(0, pos + delimiter.length());
	}

	hlslobj_path += "..\\..\\VmProjects\\RendererGPU\\";
	//cout << hlslobj_path << endl;

	string enginePath;
	if (grd_helper::GetEnginePath(enginePath)) {
		hlslobj_path = enginePath;
	}
	string hlslobj_path_4_0 = hlslobj_path + "shader_compiled_objs_4_0\\";

#ifdef DX10_0
	hlslobj_path += "shader_compiled_objs_4_0\\";
#else
	hlslobj_path += "shader_compiled_objs\\";
#endif

	auto loadShaderVS = [&](const std::string& shaderName) -> ID3D11VertexShader*
		{

			string prefix_path = hlslobj_path;

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + shaderName).c_str(), "rb") != 0)
				return nullptr;

			fseek(pFile, 0, SEEK_END);
			ullong ullFileSize = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			byte* pyRead = new byte[ullFileSize];
			fread(pyRead, sizeof(byte), ullFileSize, pFile);
			fclose(pFile);

			ID3D11VertexShader* dx11VShader = NULL;
			if (device->CreateVertexShader(pyRead, ullFileSize, NULL, &dx11VShader) != S_OK)
			{
				vzlog_error("SHADER COMPILE FAILURE! --> %s", shaderName.c_str());
			}
			VMSAFE_DELETEARRAY(pyRead);

			return dx11VShader;
		};
	auto loadShaderPS = [&](const std::string& shaderName) -> ID3D11PixelShader*
		{

			string prefix_path = hlslobj_path;

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + shaderName).c_str(), "rb") != 0)
				return nullptr;

			fseek(pFile, 0, SEEK_END);
			ullong ullFileSize = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			byte* pyRead = new byte[ullFileSize];
			fread(pyRead, sizeof(byte), ullFileSize, pFile);
			fclose(pFile);

			ID3D11PixelShader* dx11PShader = NULL;
			if (device->CreatePixelShader(pyRead, ullFileSize, NULL, &dx11PShader) != S_OK)
			{
				vzlog_error("SHADER COMPILE FAILURE! --> %s", shaderName.c_str());
			}
			VMSAFE_DELETEARRAY(pyRead);

			return dx11PShader;
		};

	brushVS.Attach(loadShaderVS("MP_Brush_vs_5_0"));
	dilationVS.Attach(loadShaderVS("MP_FullscreenQuad_vs_5_0"));
	brushPS.Attach(loadShaderPS("MP_BrushStroke_ps_5_0"));
	dilationPS.Attach(loadShaderPS("MP_Dilate_ps_5_0"));
}

ID3D11InputLayout* MeshPainter::getBrushInputLayout(const std::string& layoutDesc) {
	if (layoutDesc == "P")
		return brushInputLayout_P.Get();
	else if (layoutDesc == "PN")
		return brushInputLayout_PN.Get();
	else if (layoutDesc == "PT")
		return brushInputLayout_PT.Get();
	else if (layoutDesc == "PNT")
		return brushInputLayout_PNT.Get();
	return nullptr;
}

void MeshPainter::paintOnActor(
	int actorId,
	const MeshParams& meshParams,
	const vmmat44f& matOS2WS,
	const BrushParams& brush,
	const RayHitResult& hitResult, const bool paint) {
	if (!paintManager || !meshParams.primData || !hitResult.hit)
		return;

	ActorPaintData* paint_data = paintManager->getPaintResource(actorId);
	if (!paint_data || !paint_data->hoverTexture)
		return;

	FeedbackTexture* hoverTex = paint_data->hoverTexture.get();
	FeedbackTexture* paintTex = paint_data->paintTexture.get();

	// Compute UV radius from world brush size
	// Approximate by using the bounding box of the mesh
	float uvRadius[2];
	vmfloat3 aabbSize = meshParams.primData->aabb_os.pos_max - meshParams.primData->aabb_os.pos_min;
	float avgWorldSize = (aabbSize.x + aabbSize.y + aabbSize.z) / 3.0f;
	float normalizedBrushSize = brush.size / avgWorldSize;
	uvRadius[0] = normalizedBrushSize;
	uvRadius[1] = normalizedBrushSize;

	// Update brush constants
	updateBrushConstants(brush, hitResult.uv, uvRadius, matOS2WS, paint);

	// Render brush stroke to paint texture (reads from current, writes to
	// off-target)
	renderBrushStroke(hoverTex, paintTex, meshParams, hitResult.uv, uvRadius);
	hoverTex->swap();

	// Apply dilation to prevent texture bleeding at UV seams
	applyDilation(hoverTex);

	paintManager->markDirty(actorId);
}

bool MeshPainter::computeUVFromHit(vmobjects::PrimitiveData* primData,
	int triangleIndex, float baryU, float baryV,
	float outUV[2], const std::vector<float>* painterUVs) {
	if (!primData)
		return false;

	// Get triangle indices
	uint* indices = (uint*)primData->vidx_buffer;
	int idx0, idx1, idx2;

	if (indices) {
		idx0 = indices[triangleIndex * 3 + 0];
		idx1 = indices[triangleIndex * 3 + 1];
		idx2 = indices[triangleIndex * 3 + 2];
	}
	else {
		// Non-indexed mesh
		idx0 = triangleIndex * 3 + 0;
		idx1 = triangleIndex * 3 + 1;
		idx2 = triangleIndex * 3 + 2;
	}

	// Use painter UVs if provided
	// painterUVs is triangle-vertex indexed: [tri0_v0_u, tri0_v0_v, tri0_v1_u, tri0_v1_v, tri0_v2_u, tri0_v2_v, tri1_v0_u, ...]
	if (painterUVs && painterUVs->size() >= (size_t)(triangleIndex * 3 + 3) * 2) {
		// Read UVs directly from triangle-vertex indices (not vertex indices!)
		int triVertexIdx0 = triangleIndex * 3 + 0;
		int triVertexIdx1 = triangleIndex * 3 + 1;
		int triVertexIdx2 = triangleIndex * 3 + 2;

		float uv0_x = (*painterUVs)[triVertexIdx0 * 2 + 0];
		float uv0_y = (*painterUVs)[triVertexIdx0 * 2 + 1];
		float uv1_x = (*painterUVs)[triVertexIdx1 * 2 + 0];
		float uv1_y = (*painterUVs)[triVertexIdx1 * 2 + 1];
		float uv2_x = (*painterUVs)[triVertexIdx2 * 2 + 0];
		float uv2_y = (*painterUVs)[triVertexIdx2 * 2 + 1];

		// Interpolate UV using barycentric coordinates
		// P = (1 - u - v) * V0 + u * V1 + v * V2
		float w = 1.0f - baryU - baryV;
		outUV[0] = w * uv0_x + baryU * uv1_x + baryV * uv2_x;
		outUV[1] = w * uv0_y + baryU * uv1_y + baryV * uv2_y;
		return true;
	}

	// Fallback to TEXCOORD0 if painter UVs not available
	vmfloat3* texcoords = primData->GetVerticeDefinition("TEXCOORD0");
	if (!texcoords)
		return false;

	vmfloat3 uv0 = texcoords[idx0];
	vmfloat3 uv1 = texcoords[idx1];
	vmfloat3 uv2 = texcoords[idx2];

	float w = 1.0f - baryU - baryV;
	outUV[0] = w * uv0.x + baryU * uv1.x + baryV * uv2.x;
	outUV[1] = w * uv0.y + baryU * uv1.y + baryV * uv2.y;

	return true;
}

RayHitResult MeshPainter::raycastMesh(vmobjects::PrimitiveData* primData,
	const vmmat44f& matOS2WS,
	const vmfloat3& rayOrigin,
	const vmfloat3& rayDir,
	const geometrics::BVH* bvh,
	int actorId) {
	RayHitResult result;
	if (!primData)
		return result;

	vmfloat3* positions = primData->GetVerticeDefinition("POSITION");
	if (!positions)
		return result;

	// Get painter UVs for this actor (triangle-vertex indexed for accurate CPU raycast)
	const std::vector<float>* painterUVs = nullptr;
	if (paintManager) {
		ActorPaintData* paintData = paintManager->getPaintResource(actorId);
		if (paintData && paintData->vbUVs) {
			painterUVs = &paintData->vbUVs->vbPainterUVs_TriVertex;  // Use triangle-vertex indexed UVs
		}
	}

	// Transform ray to object space
	vmmat44f matWS2OS;
	vmmath::fMatrixInverse(&matWS2OS, &matOS2WS);

	vmfloat3 rayOriginOS, rayDirOS;
	vmmath::fTransformPoint(&rayOriginOS, &rayOrigin, &matWS2OS);
	vmmath::fTransformVector(&rayDirOS, &rayDir, &matWS2OS);
	vmmath::fNormalizeVector(&rayDirOS, &rayDirOS);

	uint* indices = (uint*)primData->vidx_buffer;
	int numTriangles = primData->num_prims;

	float closestT = FLT_MAX;

	if (bvh) {
		using namespace vz::geometrics;
		XMFLOAT3 origin(rayOriginOS.x, rayOriginOS.y, rayOriginOS.z);
		XMFLOAT3 direction(rayDirOS.x, rayDirOS.y, rayDirOS.z);
		Ray ray(origin, direction);

		bvh->Intersects(ray, 0, [&](uint32_t i) {
			int idx0, idx1, idx2;
			if (indices) {
				idx0 = indices[i * 3 + 0];
				idx1 = indices[i * 3 + 1];
				idx2 = indices[i * 3 + 2];
			}
			else {
				idx0 = i * 3 + 0;
				idx1 = i * 3 + 1;
				idx2 = i * 3 + 2;
			}

			vmfloat3 v0 = positions[idx0];
			vmfloat3 v1 = positions[idx1];
			vmfloat3 v2 = positions[idx2];

			// Moller-Trumbore ray-triangle intersection
			vmfloat3 edge1 = v1 - v0;
			vmfloat3 edge2 = v2 - v0;
			vmfloat3 h;
			vmmath::fCrossDotVector(&h, &rayDirOS, &edge2);
			float a = vmmath::fDotVector(&edge1, &h);

			if (a > -1e-6f && a < 1e-6f)
				return; // Ray parallel to triangle

			float f = 1.0f / a;
			vmfloat3 s = rayOriginOS - v0;
			float u = f * vmmath::fDotVector(&s, &h);
			if (u < 0.0f || u > 1.0f)
				return;

			vmfloat3 q;
			vmmath::fCrossDotVector(&q, &s, &edge1);
			float v = f * vmmath::fDotVector(&rayDirOS, &q);
			if (v < 0.0f || u + v > 1.0f)
				return;

			float t = f * vmmath::fDotVector(&edge2, &q);
			if (t > 1e-6f && t < closestT) {
				closestT = t;
				result.hit = true;
				result.triangleIndex = i;
				result.barycentricU = u;
				result.barycentricV = v;
				result.hitDistance = t;

				// Compute world position
				vmfloat3 hitPosOS = rayOriginOS + rayDirOS * t, hitPosWS;
				vmmath::fTransformPoint(&hitPosWS, &hitPosOS, &matOS2WS);
				result.worldPos[0] = hitPosWS.x;
				result.worldPos[1] = hitPosWS.y;
				result.worldPos[2] = hitPosWS.z;

				// Compute UV using painter UVs
				computeUVFromHit(primData, i, u, v, result.uv, painterUVs);
			}
			});
	}
	else {
		for (int i = 0; i < numTriangles; i++) {
			int idx0, idx1, idx2;
			if (indices) {
				idx0 = indices[i * 3 + 0];
				idx1 = indices[i * 3 + 1];
				idx2 = indices[i * 3 + 2];
			}
			else {
				idx0 = i * 3 + 0;
				idx1 = i * 3 + 1;
				idx2 = i * 3 + 2;
			}

			vmfloat3 v0 = positions[idx0];
			vmfloat3 v1 = positions[idx1];
			vmfloat3 v2 = positions[idx2];

			// Moller-Trumbore ray-triangle intersection
			vmfloat3 edge1 = v1 - v0;
			vmfloat3 edge2 = v2 - v0;
			vmfloat3 h;
			vmmath::fCrossDotVector(&h, &rayDirOS, &edge2);
			float a = vmmath::fDotVector(&edge1, &h);

			if (a > -1e-6f && a < 1e-6f)
				continue; // Ray parallel to triangle

			float f = 1.0f / a;
			vmfloat3 s = rayOriginOS - v0;
			float u = f * vmmath::fDotVector(&s, &h);
			if (u < 0.0f || u > 1.0f)
				continue;

			vmfloat3 q;
			vmmath::fCrossDotVector(&q, &s, &edge1);
			float v = f * vmmath::fDotVector(&rayDirOS, &q);
			if (v < 0.0f || u + v > 1.0f)
				continue;

			float t = f * vmmath::fDotVector(&edge2, &q);
			if (t > 1e-6f && t < closestT) {
				closestT = t;
				result.hit = true;
				result.triangleIndex = i;
				result.barycentricU = u;
				result.barycentricV = v;
				result.hitDistance = t;

				// Compute world position
				vmfloat3 hitPosOS = rayOriginOS + rayDirOS * t, hitPosWS;
				vmmath::fTransformPoint(&hitPosWS, &hitPosOS, &matOS2WS);
				result.worldPos[0] = hitPosWS.x;
				result.worldPos[1] = hitPosWS.y;
				result.worldPos[2] = hitPosWS.z;

				// Compute UV using painter UVs
				computeUVFromHit(primData, i, u, v, result.uv, painterUVs);
			}
		}
	}
	return result;
}

void MeshPainter::createBrushResources() {
	if (!device || brushConstantBuffer)
		return;

	// Create constant buffer
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(BrushConstants);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	device->CreateBuffer(&bd, nullptr, brushConstantBuffer.GetAddressOf());
}

void MeshPainter::updateBrushConstants(const BrushParams& brush,
	const float uv[2], float uvRadius[2], const vmmat44f& matOS2WS, const bool paint) {
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	context->Map(brushConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
		&mappedResource);

	BrushConstants* data = (BrushConstants*)mappedResource.pData;
	data->brushColor[0] = brush.color[0];
	data->brushColor[1] = brush.color[1];
	data->brushColor[2] = brush.color[2];
	data->brushColor[3] = brush.color[3];

	// World space center and radius
	data->brushCenter[0] = brush.position[0];
	data->brushCenter[1] = brush.position[1];
	data->brushCenter[2] = brush.position[2];
	data->brushStrength = brush.strength;

	// World space radius (using brush.size)
	data->brushRadius[0] = brush.size;
	data->brushRadius[1] = brush.size;
	data->brushRadius[2] = brush.size;
	data->brushHardness = brush.hardness;

	// Copy matOS2WS (row-major to column-major for HLSL)
	// HLSL expects column-major, so transpose
	vmmat44f matTranspose = TRANSPOSE(matOS2WS);
	memcpy(data->matOS2WS, &matTranspose, sizeof(float) * 16);

	data->blendMode = static_cast<int>(brush.blendMode);
	data->painterFlags = paint? 1 : 0;
	data->padding0 = 0.0f;
	data->padding1 = 0.0f;

	context->Unmap(brushConstantBuffer.Get(), 0);
}

void MeshPainter::renderBrushStroke(FeedbackTexture* hoverTex,
	FeedbackTexture* paintTex,
	const MeshParams& meshParams,
	const float uv[2], float uvRadius[2]) {
	if (!hoverTex || !brushVS || !brushPS)
		return;

	RenderTarget* hover_rt = hoverTex->getOffRenderTarget();
	RenderTarget* paint_rt = hoverTex->getOffRenderTarget();
	ID3D11RenderTargetView* rtvPtrs[2] = { hover_rt->rtv.Get(), paint_rt->rtv.Get() };
	context->OMSetRenderTargets(2, rtvPtrs, nullptr); // No depth stencil for now

	int width = hover_rt->width;
	int height = hover_rt->height;
	D3D11_VIEWPORT vp;
	vp.Width = (float)width;
	vp.Height = (float)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);

	// Bind brush shaders
	if (brushVS)
		context->VSSetShader(brushVS.Get(), nullptr, 0);
	if (brushPS)
		context->PSSetShader(brushPS.Get(), nullptr, 0);
		
	// Disable Geometry Shader (prevents linkage errors with main renderer's GS)
	context->GSSetShader(nullptr, nullptr, 0);

	// Bind input layout (use mesh painter's own input layout created from MP_Brush_vs_5_0)
	ID3D11InputLayout* brushLayout = getBrushInputLayout(meshParams.inputLayerDesc);
	if (brushLayout)
		context->IASetInputLayout(brushLayout);
	else
		vzlog_error("MeshPainter: Failed to get brush input layout for %s", meshParams.inputLayerDesc.c_str());

	// Bind previous paint texture as SRV at slot 2 (tex2d_previous : register(t2))
	ID3D11ShaderResourceView* prevPaintSRV = paintTex->getRenderTarget()->srv.Get();
	context->PSSetShaderResources(2, 1, &prevPaintSRV);

	// Bind UV buffer as SRV at slot 61 (g_uvBuffer : register(t61))
	if (meshParams.uvBufferSRV) {
		context->VSSetShaderResources(61, 1, &meshParams.uvBufferSRV);
	}

	// Bind brush constant buffer
	ID3D11Buffer* cb = brushConstantBuffer.Get();
	context->VSSetConstantBuffers(1, 1, &cb);
	context->PSSetConstantBuffers(1, 1, &cb);

	// Set blend state
	float blendFactor[4] = { 0, 0, 0, 0 };
	ID3D11BlendState* bs = grd_helper::GetPSOManager()->get_blender("ADD");
	//context->OMSetBlendState(bs, blendFactor, 0xFFFFFFFF);
	context->OMSetBlendState(NULL, NULL, 0xffffffff);
		
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&meshParams.vbMesh, &meshParams.stride, &meshParams.offset);
	// Input layout already set above

	assert(meshParams.primData->num_vtx == meshParams.primData->num_prims * 3);
	context->Draw(meshParams.primData->num_vtx, 0);

	// Unbind SRV
	ID3D11ShaderResourceView* nullSRV = nullptr;
	context->PSSetShaderResources(2, 1, &nullSRV);
		
	// Unbind constant buffer at slot 1 to prevent conflicts with main renderer
	ID3D11Buffer* nullCB = nullptr;
	context->PSSetConstantBuffers(1, 1, &nullCB);

	ID3D11RenderTargetView* nullRTVs[2] = { nullptr, nullptr };
	context->OMSetRenderTargets(2, nullRTVs, nullptr);
}

void MeshPainter::applyDilation(FeedbackTexture* sourceFBT) {
	if (!sourceFBT || !dilationVS || !dilationPS)
		return;

	sourceFBT->renderOperation(sourceFBT->getOffRenderTarget(), [this, sourceFBT](__ID3D11DeviceContext* ctx) {
		// Bind dilation shaders
		if (dilationVS)
			ctx->VSSetShader(dilationVS.Get(), nullptr, 0);
		if (dilationPS)
			ctx->PSSetShader(dilationPS.Get(), nullptr, 0);

		// Disable Geometry Shader (prevents linkage errors with main renderer's GS)
		ctx->GSSetShader(nullptr, nullptr, 0);

		// Bind source texture to slot 0 (tex2d_base : register(t0))
		ID3D11ShaderResourceView* srv = sourceFBT->getRenderTarget()->srv.Get();
		ctx->PSSetShaderResources(0, 1, &srv);

		// Render fullscreen quad with dilation shader
		sourceFBT->renderFullscreenQuad();

		// Unbind
		//ID3D11ShaderResourceView* nullSRV = nullptr;
		//ctx->PSSetShaderResources(0, 1, &nullSRV);
		});

	sourceFBT->swap();
}
