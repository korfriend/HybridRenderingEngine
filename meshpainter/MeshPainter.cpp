#include "MeshPainter.h"
#include "../vzm2/Geometrics.h"
#include <cmath>
#include <iostream>


// ============================================================================
// MeshPainter Implementation
// ============================================================================

MeshPainter::MeshPainter(__ID3D11Device *device, __ID3D11DeviceContext *context)
    : device(device), context(context), paintManager(nullptr) {
  createBrushResources();
}

MeshPainter::~MeshPainter() {
  if (paintManager) {
    delete paintManager;
    paintManager = nullptr;
  }
  dilator.reset();
}
void MeshPainter::initialize(const TextureInfo &baseTexture) {
  if (!paintManager) {
    paintManager = new PaintResourceManager(device, context);
  }
  dilator = std::make_unique<Dilator>(device, context);
}

void MeshPainter::paintOnActor(int actorId,
                               const vmobjects::PrimitiveData *primData,
                               const vmmat44f &matOS2WS,
                               const BrushParams &brush,
                               const RayHitResult &hitResult) {
  if (!paintManager || !primData || !hitResult.hit)
    return;

  ActorPaintData *paint_data = paintManager->getPaintResource(actorId);
  if (!paint_data || !paint_data->feedbackTexture)
    return;

  FeedbackTexture *feedbackTex = paint_data->feedbackTexture.get();

  // Compute UV radius from world brush size
  // Approximate by using the bounding box of the mesh
  float uvRadius[2];
  vmfloat3 aabbSize = primData->aabb_os.pos_max - primData->aabb_os.pos_min;
  float avgWorldSize = (aabbSize.x + aabbSize.y + aabbSize.z) / 3.0f;
  float normalizedBrushSize = brush.size / avgWorldSize;
  uvRadius[0] = normalizedBrushSize;
  uvRadius[1] = normalizedBrushSize;

  // Update brush constants
  updateBrushConstants(brush, hitResult.uv, uvRadius);

  // Render brush stroke to paint texture (reads from current, writes to
  // off-target)
  renderBrushStroke(feedbackTex, hitResult.uv, uvRadius);
  feedbackTex->swap();

  // Apply dilation to prevent texture bleeding at UV seams
  if (dilator) {
    dilator->apply(feedbackTex);
  }

  paintManager->markDirty(actorId);
}

bool MeshPainter::computeUVFromHit(vmobjects::PrimitiveData *primData,
                                   int triangleIndex, float baryU, float baryV,
                                   float outUV[2]) {
  if (!primData)
    return false;

  // Get UV coordinates from TEXCOORD0
  vmfloat3 *texcoords = primData->GetVerticeDefinition("TEXCOORD0");
  if (!texcoords)
    return false;

  // Get triangle indices
  uint *indices = (uint *)primData->vidx_buffer;
  int idx0, idx1, idx2;

  if (indices) {
    idx0 = indices[triangleIndex * 3 + 0];
    idx1 = indices[triangleIndex * 3 + 1];
    idx2 = indices[triangleIndex * 3 + 2];
  } else {
    // Non-indexed mesh
    idx0 = triangleIndex * 3 + 0;
    idx1 = triangleIndex * 3 + 1;
    idx2 = triangleIndex * 3 + 2;
  }

  // Get UVs of triangle vertices (stored in xy of vmfloat3)
  vmfloat3 uv0 = texcoords[idx0];
  vmfloat3 uv1 = texcoords[idx1];
  vmfloat3 uv2 = texcoords[idx2];

  // Interpolate UV using barycentric coordinates
  // P = (1 - u - v) * V0 + u * V1 + v * V2
  float w = 1.0f - baryU - baryV;
  outUV[0] = w * uv0.x + baryU * uv1.x + baryV * uv2.x;
  outUV[1] = w * uv0.y + baryU * uv1.y + baryV * uv2.y;

  return true;
}

RayHitResult MeshPainter::raycastMesh(vmobjects::PrimitiveData *primData,
                                      const vmmat44f &matOS2WS,
                                      const vmfloat3 &rayOrigin,
                                      const vmfloat3 &rayDir,
                                      const geometrics::BVH *bvh) {
  RayHitResult result;
  if (!primData)
    return result;

  vmfloat3 *positions = primData->GetVerticeDefinition("POSITION");
  if (!positions)
    return result;

  // Transform ray to object space
  vmmat44f matWS2OS;
  vmmath::fMatrixInverse(&matWS2OS, &matOS2WS);

  vmfloat3 rayOriginOS, rayDirOS;
  vmmath::fTransformPoint(&rayOriginOS, &rayOrigin, &matWS2OS);
  vmmath::fTransformVector(&rayDirOS, &rayDir, &matWS2OS);
  vmmath::fNormalizeVector(&rayDirOS, &rayDirOS);

  uint *indices = (uint *)primData->vidx_buffer;
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
      } else {
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

        // Compute UV
        computeUVFromHit(primData, i, u, v, result.uv);
      }
    });
  } else {
    for (int i = 0; i < numTriangles; i++) {
      int idx0, idx1, idx2;
      if (indices) {
        idx0 = indices[i * 3 + 0];
        idx1 = indices[i * 3 + 1];
        idx2 = indices[i * 3 + 2];
      } else {
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

        // Compute UV
        computeUVFromHit(primData, i, u, v, result.uv);
      }
    }
  }
  return result;
}

void MeshPainter::createBrushResources() {
  if (!device)
    return;

  // Create constant buffer
  D3D11_BUFFER_DESC bd = {};
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.ByteWidth = sizeof(BrushConstants);
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  device->CreateBuffer(&bd, nullptr, brushConstantBuffer.GetAddressOf());

  // Create blend state for brush rendering
  D3D11_BLEND_DESC blendDesc = {};
  blendDesc.RenderTarget[0].BlendEnable = TRUE;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_ALL;

  device->CreateBlendState(&blendDesc, brushBlendState.GetAddressOf());
}

void MeshPainter::updateBrushConstants(const BrushParams &brush,
                                       const float uv[2], float uvRadius[2]) {
  D3D11_MAPPED_SUBRESOURCE mappedResource;
  context->Map(brushConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
               &mappedResource);

  BrushConstants *data = (BrushConstants *)mappedResource.pData;
  data->brushColor[0] = brush.color[0];
  data->brushColor[1] = brush.color[1];
  data->brushColor[2] = brush.color[2];
  data->brushColor[3] = brush.color[3];
  data->brushCenter[0] = uv[0];
  data->brushCenter[1] = uv[1];
  data->brushRadius[0] = uvRadius[0];
  data->brushRadius[1] = uvRadius[1];
  data->brushStrength = brush.strength;
  data->brushHardness = brush.hardness;
  data->blendMode = static_cast<int>(brush.blendMode);
  data->padding = 0.0f;

  context->Unmap(brushConstantBuffer.Get(), 0);
}

void MeshPainter::loadBrushShaders() {
  // Shaders are loaded externally in gpures_helper.cpp and set via
  // setBrushShaders() This method is kept for potential future use (e.g.,
  // runtime shader loading)
}

void MeshPainter::renderBrushStroke(FeedbackTexture *feedbackTex,
                                    const float uv[2], float uvRadius[2]) {
  if (!feedbackTex || !brushVS || !brushPS)
    return;

  feedbackTex->renderOperation(feedbackTex->getOffRenderTarget(), [this, feedbackTex](__ID3D11DeviceContext* ctx) {
    // Bind brush shaders
    if (brushVS)
      ctx->VSSetShader(brushVS.Get(), nullptr, 0);
    if (brushPS)
      ctx->PSSetShader(brushPS.Get(), nullptr, 0);

    // Bind input layout
    if (brushInputLayout)
      ctx->IASetInputLayout(brushInputLayout.Get());

    // Bind previous paint texture as SRV at slot 2 (tex2d_previous : register(t2))
    ID3D11ShaderResourceView* prevPaintSRV = feedbackTex->getRenderTarget()->srv.Get();
    ctx->PSSetShaderResources(2, 1, &prevPaintSRV);

    // Bind brush constant buffer
    ID3D11Buffer* cb = brushConstantBuffer.Get();
    ctx->PSSetConstantBuffers(0, 1, &cb);

    // Set blend state
    float blendFactor[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(brushBlendState.Get(), blendFactor, 0xFFFFFFFF);

    // Render fullscreen quad with brush shader
    feedbackTex->renderFullscreenQuad();

    // Unbind SRV
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(2, 1, &nullSRV);
  });
}
