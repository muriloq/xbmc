/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#define WIN32_LEAN_AND_MEAN
#include "WinRenderer.h"
#include "RenderCapture.h"
#include "RenderFactory.h"
#include "RenderFlags.h"
#include "rendering/dx/RenderContext.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/log.h"
#include "windows/RendererDXVA.h"
#include "windows/RendererSoftware.h"
#include "windows/RendererShaders.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvapi.h"
#include "uhdDisplay.h"


struct render_details
{
  using map = std::map<RenderMethod, int>;
  using weights_fn = std::function<void(map&, const VideoPicture&)>;
  using create_fn = std::function<CRendererBase*(CVideoSettings&)>;

  RenderMethod method;
  std::string name;
  create_fn create;
  weights_fn weights;

  template<class T>
  constexpr static render_details get(RenderMethod method, const std::string& name)
  {
    return { method, name, T::Create, T::GetWeight };
  }
};
static bool first = true;
if (picture.hasDisplayMetadata || picture.hasLightMetadata)
void SetHdrMonitorMode(bool enableHDR)
  {
      if (first)
    {
        NvAPI_Initialize();
        first = false;
    }

    NvAPI_Status nvStatus = NVAPI_OK;
    NvDisplayHandle hNvDisplay = NULL;

    // get first display handle which should work for all NVAPI calls for all GPUs
    if ((nvStatus = NvAPI_EnumNvidiaDisplayHandle(0, &hNvDisplay)) != NVAPI_OK)
    {
        printf("NvAPI_EnumNvidiaDisplayHandle returned error code %d\r\n", nvStatus);

        return;
    }

    NvU32 gpuCount = 0;
    NvU32 maxDisplayIndex = 0;
    NvPhysicalGpuHandle ahGPU[NVAPI_MAX_PHYSICAL_GPUS] = {};

    // get the list of displays connected, populate the dynamic components
    nvStatus = NvAPI_EnumPhysicalGPUs(ahGPU, &gpuCount);

    if (NVAPI_OK != nvStatus)
    {
        printf("NvAPI_EnumPhysicalGPUs returned error code %d\r\n", nvStatus);

        return;
    }

    for (NvU32 i = 0; i < gpuCount; ++i)
    {
        NvU32 displayIdCount = 16;
        NvU32 flags = 0;
        NV_GPU_DISPLAYIDS displayIdArray[16] = {};
        displayIdArray[0].version = NV_GPU_DISPLAYIDS_VER;

        nvStatus = NvAPI_GPU_GetConnectedDisplayIds(ahGPU, displayIdArray, &displayIdCount, flags);

        if (NVAPI_OK == nvStatus)
        {
            printf("Display count %d\r\n", displayIdCount);

            for (maxDisplayIndex = 0; maxDisplayIndex < displayIdCount; ++maxDisplayIndex)
            {
                printf("Display tested %d\r\n", maxDisplayIndex);

                NV_HDR_CAPABILITIES hdrCapabilities = {};

                hdrCapabilities.version = NV_HDR_CAPABILITIES_VER;

                if (NVAPI_OK == NvAPI_Disp_GetHdrCapabilities(displayIdArray[maxDisplayIndex].displayId, &hdrCapabilities))
                {
                    if (hdrCapabilities.isST2084EotfSupported)
                    {
                        printf("Display %d supports ST2084 EOTF\r\n", maxDisplayIndex);

                        NV_HDR_COLOR_DATA hdrColorData = {};

                        memset(&hdrColorData, 0, sizeof(hdrColorData));

                        hdrColorData.version = NV_HDR_COLOR_DATA_VER;
                        hdrColorData.cmd = NV_HDR_CMD_SET;
                        hdrColorData.static_metadata_descriptor_id = NV_STATIC_METADATA_TYPE_1;

                        hdrColorData.hdrMode = enableHDR ? NV_HDR_MODE_UHDBD : NV_HDR_MODE_OFF;


                        nvStatus = NvAPI_Disp_HdrColorControl(displayIdArray[maxDisplayIndex].displayId, &hdrColorData);

                        if (NVAPI_OK == nvStatus)
                        {
                            printf("NvAPI_Disp_SethdrColorData call has succeeded: ");
                        }
                        else
                        {
                            NvAPI_ShortString szDesc;
                            NvAPI_GetErrorMessage(nvStatus, szDesc);
                            printf("NvAPI_Disp_HdrColorControl returned %s (%x)\r\n", szDesc, nvStatus);
                        }
                    }
                }
                else
                {
                    NvAPI_ShortString szDesc;
                    NvAPI_GetErrorMessage(nvStatus, szDesc);
                    printf("NvAPI_Disp_GetHdrCapabilities returned %s (%x)\r\n", szDesc, nvStatus);
                }
            }
        }
        else
        {
            NvAPI_ShortString szDesc;
            NvAPI_GetErrorMessage(nvStatus, szDesc);
            printf("NvAPI_GPU_GetConnectedDisplayIds returned %s (%x)\r\n", szDesc, nvStatus);
        }
    }
  
  
static std::vector<render_details> RenderMethodDetails =
{
  render_details::get<CRendererSoftware>(RENDER_SW, "Software"),
  render_details::get<CRendererShaders>(RENDER_PS, "Pixel Shaders"),
  render_details::get<CRendererDXVA>(RENDER_DXVA, "DXVA"),
};

CBaseRenderer* CWinRenderer::Create(CVideoBuffer*)
{
  return new CWinRenderer();
}

bool CWinRenderer::Register()
{
  VIDEOPLAYER::CRendererFactory::RegisterRenderer("default", Create);
  return true;
}

CWinRenderer::CWinRenderer()
{
  m_format = AV_PIX_FMT_NONE;
  PreInit();
}

CWinRenderer::~CWinRenderer()
{
  CWinRenderer::UnInit();
}

CRendererBase* CWinRenderer::SelectRenderer(const VideoPicture& picture)
{
  int iRequestedMethod = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_VIDEOPLAYER_RENDERMETHOD);
  CLog::LogF(LOGDEBUG, "requested render method: %d", iRequestedMethod);

  std::map<RenderMethod, int> weights;
  for (auto& details : RenderMethodDetails)
    details.weights(weights, picture);

  RenderMethod method;
  switch (iRequestedMethod)
  {
  case RENDER_METHOD_SOFTWARE:
    if (weights[RENDER_SW])
    {
      method = RENDER_SW;
      break;
    }
    // fallback to PS
  case RENDER_METHOD_D3D_PS:
    if (weights[RENDER_PS])
    {
      method = RENDER_PS;
      break;
    }
    //fallback to DXVA
  case RENDER_METHOD_DXVA:
    if (weights[RENDER_DXVA])
    {
      method = RENDER_DXVA;
      break;
    }
    // fallback to AUTO
  case RENDER_METHOD_AUTO:
  default:
  {
    const auto it = std::max_element(weights.begin(), weights.end(),
      [](auto& w1, auto& w2) { return w1.second < w2.second; });

    if (it != weights.end())
    {
      method = it->first;
      break;
    }

    // there is no elements in weights, so no renderer which supports incoming video buffer
    CLog::LogF(LOGERROR, "unable to select render method for video buffer");
    return nullptr;
  }
  }

  const auto it = std::find_if(RenderMethodDetails.begin(), RenderMethodDetails.end(),
    [method](render_details& d) { return d.method == method; });

  if (it != RenderMethodDetails.end())
  {
    CLog::LogF(LOGDEBUG, "selected render method: {}", it->name);
    return it->create(m_videoSettings);
  }

  // something goes really wrong
  return nullptr;
}

CRect CWinRenderer::GetScreenRect() const
{
  CRect screenRect(0.f, 0.f,
    static_cast<float>(CServiceBroker::GetWinSystem()->GetGfxContext().GetWidth()),
    static_cast<float>(CServiceBroker::GetWinSystem()->GetGfxContext().GetHeight()));

  switch (CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode())
  {
  case RENDER_STEREO_MODE_SPLIT_HORIZONTAL:
    screenRect.y2 *= 2;
    break;
  case RENDER_STEREO_MODE_SPLIT_VERTICAL:
    screenRect.x2 *= 2;
    break;
  default:
    break;
  }

  return screenRect;
}

bool CWinRenderer::Configure(const VideoPicture &picture, float fps, unsigned int orientation)
{
  m_sourceWidth       = picture.iWidth;
  m_sourceHeight      = picture.iHeight;
  m_renderOrientation = orientation;
  m_fps = fps;
  m_iFlags = GetFlagsChromaPosition(picture.chroma_position)
           | GetFlagsColorMatrix(picture.color_space, picture.iWidth, picture.iHeight)
           | GetFlagsColorPrimaries(picture.color_primaries)
           | GetFlagsStereoMode(picture.stereoMode);
  m_format = picture.videoBuffer->GetFormat();

  // calculate the input frame aspect ratio
  CalculateFrameAspectRatio(picture.iDisplayWidth, picture.iDisplayHeight);
  SetViewMode(m_videoSettings.m_ViewMode);
  ManageRenderArea();

  m_renderer.reset(SelectRenderer(picture));
  if (!m_renderer || !m_renderer->Configure(picture, fps, orientation))
  {
    m_renderer.reset();
    return false;
  }

  m_bConfigured = true;
  return true;
}

int CWinRenderer::NextBuffer() const
{
  return m_renderer->NextBuffer();
}

void CWinRenderer::AddVideoPicture(const VideoPicture &picture, int index)
{
  m_renderer->AddVideoPicture(picture, index);
}

void CWinRenderer::Update()
{
  if (!m_bConfigured)
    return;

  ManageRenderArea();
  m_renderer->ManageTextures();
}

void CWinRenderer::RenderUpdate(int index, int index2, bool clear, unsigned int flags, unsigned int alpha)
{
  if (!m_bConfigured)
    return;

  if (clear)
    CServiceBroker::GetWinSystem()->GetGfxContext().Clear(DX::Windowing()->UseLimitedColor() ? 0x101010 : 0);
  DX::Windowing()->SetAlphaBlendEnable(alpha < 255);

  ManageRenderArea();
  m_renderer->Render(index, index2, DX::Windowing()->GetBackBuffer(), 
                     m_sourceRect, m_destRect, GetScreenRect(), flags);
}

bool CWinRenderer::RenderCapture(CRenderCapture* capture)
{
  if (!m_bConfigured)
    return false;

  capture->BeginRender();
  if (capture->GetState() != CAPTURESTATE_FAILED)
  {
    const CRect destRect(0, 0, static_cast<float>(capture->GetWidth()), static_cast<float>(capture->GetHeight()));

    m_renderer->Render(capture->GetTarget(), m_sourceRect, destRect, GetScreenRect());
    capture->EndRender();

    return true;
  }

  return false;
}

void CWinRenderer::SetBufferSize(int numBuffers)
{
  if (!m_bConfigured)
    return;

  m_renderer->SetBufferSize(numBuffers);
}

void CWinRenderer::PreInit()
{
  CSingleLock lock(CServiceBroker::GetWinSystem()->GetGfxContext());
  m_bConfigured = false;
  UnInit();
}

void CWinRenderer::UnInit()
{
  CSingleLock lock(CServiceBroker::GetWinSystem()->GetGfxContext());

  m_renderer.reset();
  m_bConfigured = false;
}

bool CWinRenderer::Flush(bool saveBuffers)
{
  if (!m_bConfigured)
    return false;

  return m_renderer->Flush(saveBuffers);
}

bool CWinRenderer::Supports(ERENDERFEATURE feature)
{
  if(feature == RENDERFEATURE_BRIGHTNESS)
    return true;

  if(feature == RENDERFEATURE_CONTRAST)
    return true;

  if (feature == RENDERFEATURE_STRETCH         ||
      feature == RENDERFEATURE_NONLINSTRETCH   ||
      feature == RENDERFEATURE_ZOOM            ||
      feature == RENDERFEATURE_VERTICAL_SHIFT  ||
      feature == RENDERFEATURE_PIXEL_RATIO     ||
      feature == RENDERFEATURE_ROTATION        ||
      feature == RENDERFEATURE_POSTPROCESS     ||
      feature == RENDERFEATURE_TONEMAP)
    return true;

  return false;
}

bool CWinRenderer::Supports(ESCALINGMETHOD method)
{
  if (!m_bConfigured)
    return false;

  return m_renderer->Supports(method);
}

bool CWinRenderer::WantsDoublePass()
{
  if (!m_bConfigured)
    return false;

  return m_renderer->WantsDoublePass();
}

bool CWinRenderer::ConfigChanged(const VideoPicture& picture)
{
  if (!m_bConfigured)
    return true;

  return picture.videoBuffer->GetFormat() != m_format;
}

CRenderInfo CWinRenderer::GetRenderInfo()
{
  if (!m_bConfigured)
    return {};

  return m_renderer->GetRenderInfo();
}

void CWinRenderer::ReleaseBuffer(int idx)
{
  if (!m_bConfigured)
    return;

  m_renderer->ReleaseBuffer(idx);
}

bool CWinRenderer::NeedBuffer(int idx)
{
  if (!m_bConfigured)
    return false;

  return m_renderer->NeedBuffer(idx);
}
