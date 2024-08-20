#include "StdAfx.h"

#if defined(RENDER_TARGET)

#include "RenderTarget.h"

#include "../EterLib/Camera.h"
#include "../EterLib/RenderTargetManager.h"
#include "../EterPythonLib/PythonGraphic.h"

#include "../EterBase/CRC32.h"
#include "../GameLib/GameType.h"
#include "../GameLib/MapType.h"
#include "../GameLib/ItemData.h"
#include "../GameLib/ActorInstance.h"
#include "../UserInterface/InstanceBase.h"

#include "ResourceManager.h"

CRenderTarget::CRenderTarget(const DWORD dwWidth, const DWORD dwHeight) :
	m_pModelInstance(nullptr),
	m_pBackgroundImage(nullptr),
	m_fModelRotation(0),
	m_fEyeY(0.0f),
	m_fZoomY(0.0f),
	m_fTargetHeight(0.0f),
	m_fTargetY(0.0f),
	m_bShow(false)
{
	auto pTex = new CGraphicRenderTargetTexture;
	if (!pTex->Create(dwWidth, dwHeight, D3DFMT_X8R8G8B8, D3DFMT_D16))
	{
		delete pTex;
		TraceError("CRenderTarget::CRenderTarget: Could not create CGraphicRenderTargetTexture %dx%d", dwWidth, dwHeight);
		throw std::runtime_error("CRenderTarget::CRenderTarget: Could not create CGraphicRenderTargetTexture");
	}

	m_pRenderTargetTexture = std::unique_ptr<CGraphicRenderTargetTexture>(pTex);
}

CRenderTarget::~CRenderTarget()
{
	m_pModelInstance = nullptr;
	m_pBackgroundImage = NULL;
	m_fModelRotation = NULL;
	m_fEyeY = 0.0f;
	m_fZoomY = 0.0f;
	m_fTargetHeight = 0.0f;
	m_fTargetY = 0.0f;
	m_bShow = false;
}

void CRenderTarget::SetVisibility(bool bShow)
{
	m_bShow = bShow;
}

void CRenderTarget::RenderTexture() const
{
	m_pRenderTargetTexture->Render();
}

void CRenderTarget::SetRenderingRect(RECT* pRect) const
{
	m_pRenderTargetTexture->SetRenderingRect(pRect);
}

void CRenderTarget::CreateTextures() const
{
	m_pRenderTargetTexture->CreateTextures();
}

void CRenderTarget::ReleaseTextures() const
{
	m_pRenderTargetTexture->ReleaseTextures();
}

void CRenderTarget::SelectModel(const DWORD dwIndex)
{
	if (dwIndex == 0)
	{
		//delete m_pModelInstance;
		m_pModelInstance.reset();
		return;
	}

	CInstanceBase::SCreateData kCreateData{};

	kCreateData.m_bType = CActorInstance::TYPE_NPC; // Dynamic Type
	kCreateData.m_dwRace = dwIndex;

	auto model = std::make_unique<CInstanceBase>();
	if (!model->Create(kCreateData))
	{
		if (m_pModelInstance)
		{
			m_pModelInstance.reset();
		}
		return;
	}

	m_pModelInstance = std::move(model);

	m_pModelInstance->Refresh(CRaceMotionData::NAME_WAIT, true);
	m_pModelInstance->SetLoopMotion(CRaceMotionData::NAME_WAIT);
	m_pModelInstance->SetAlwaysRender(true);
	m_pModelInstance->SetRotation(0.0f);
	m_pModelInstance->NEW_SetPixelPosition(TPixelPosition(0.0f, 0.0f, 0.0f));

	CActorInstance& rkActor = m_pModelInstance->GetGraphicThingInstanceRef();
	float fHeight = rkActor.GetHeight();
	m_fEyeY = fHeight;
	m_fZoomY = 0.0f;
	m_fTargetY = 0.0f;
	m_fTargetHeight = m_fEyeY * 8.9f / 140.0f;

	CCameraManager::instance().SetCurrentCamera(CCameraManager::SHOPDECO_CAMERA);
	CCamera* pCam = CCameraManager::instance().GetCurrentCamera();
	pCam->SetTargetHeight(m_fEyeY / 2.0f);
	CCameraManager::instance().ResetToPreviousCamera();
}

void CRenderTarget::SetZoom(bool bZoom)
{
	if (!m_pModelInstance)
		return;

	if (bZoom)
	{
		m_fZoomY = m_fZoomY - m_fTargetHeight;
		float v3 = -(m_fEyeY * 8.9f - m_fEyeY * 3.0f);
		m_fZoomY = fmax(v3, m_fZoomY);
	}
	else
	{
		m_fZoomY = m_fZoomY + m_fTargetHeight;
		float v6 = 14000.0f - m_fEyeY * 8.9f;
		float v7 = m_fEyeY * 8.9f + m_fEyeY * 5.0f;
		m_fZoomY = fmin(m_fZoomY, v6);
		m_fZoomY = fmin(m_fZoomY, v7);
	}
}

bool CRenderTarget::CreateBackground(const char* c_szImgPath, const DWORD dwWidth, const DWORD dwHeight)
{
	if (m_pBackgroundImage)
		return false;

	m_pBackgroundImage = std::make_unique<CGraphicImageInstance>();

	const auto pResource = dynamic_cast<CGraphicImage*>(CResourceManager::instance().GetResourcePointer(c_szImgPath));
	if (!pResource)
	{
		m_pBackgroundImage.reset();
		return false;
	}

	m_pBackgroundImage->SetImagePointer(pResource);
	m_pBackgroundImage->SetScale(static_cast<float>(dwWidth) / pResource->GetWidth(), static_cast<float>(dwHeight) / pResource->GetHeight());
	return true;
}

void CRenderTarget::RenderBackground() const
{
	if (!m_bShow)
		return;

	if (!m_pBackgroundImage)
		return;

	m_pRenderTargetTexture->SetRenderTarget();

	CGraphicRenderTargetTexture::Clear();
	CPythonGraphic::Instance().SetInterfaceRenderState();

	//auto& pRectRender = *m_pRenderTargetTexture->GetRenderingRect();
	//float fWidth = static_cast<float>(pRectRender.right) - static_cast<float>(pRectRender.left);
	//float fHeight = static_cast<float>(pRectRender.bottom) - static_cast<float>(pRectRender.top);

	//CPythonGraphic::Instance().SetViewport(0.0f, 0.0f, fWidth, fHeight);

	m_pBackgroundImage->Render();
	m_pRenderTargetTexture->ResetRenderTarget();
	//CPythonGraphic::Instance().RestoreViewport();
}

void CRenderTarget::UpdateModel()
{
	if (!m_bShow || !m_pModelInstance)
		return;

	if (m_fModelRotation < 360.0f)
		m_fModelRotation += 1.0f;
	else
		m_fModelRotation = 0.0f;

	m_pModelInstance->Transform();
	m_pModelInstance->SetRotation(m_fModelRotation);
	CActorInstance& rkModelActor = m_pModelInstance->GetGraphicThingInstanceRef();
	rkModelActor.RotationProcess();
}

void CRenderTarget::DeformModel() const
{
	if (!m_bShow)
		return;

	if (m_pModelInstance)
		m_pModelInstance->Deform();
}

void CRenderTarget::RenderModel() const
{
	if (!m_bShow)
		return;

	auto& pRectRender = *m_pRenderTargetTexture->GetRenderingRect();

	if (!m_pModelInstance)
		return;

	m_pRenderTargetTexture->SetRenderTarget();

	if (!m_pBackgroundImage)
	{
		m_pRenderTargetTexture->Clear();
	}

	CPythonGraphic::Instance().ClearDepthBuffer();

	float fFov = CPythonGraphic::Instance().GetFOV();
	float fAspect = CPythonGraphic::Instance().GetAspect();
	float fNearY = CPythonGraphic::Instance().GetNearY();
	float fFarY = CPythonGraphic::Instance().GetFarY();

	float fWidth = static_cast<float>(pRectRender.right) - static_cast<float>(pRectRender.left);
	float fHeight = static_cast<float>(pRectRender.bottom) - static_cast<float>(pRectRender.top);

	BOOL bIsFog = STATEMANAGER.GetRenderState(D3DRS_FOGENABLE);
	STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, 0);

	CCameraManager::instance().SetCurrentCamera(CCameraManager::SHOPDECO_CAMERA);
	CCamera* pCam = CCameraManager::instance().GetCurrentCamera();

	CPythonGraphic::instance().SetViewport(0.0f, 0.0f, fWidth, fHeight);
	CPythonGraphic::Instance().PushState();

	// YMD.2021.01.12.Owsap - Model Height
	// NOTE : Actor's Height / 2 ( ~ mid camera position)
	CActorInstance& rkActor = m_pModelInstance->GetGraphicThingInstanceRef();
	float fActorHeight = rkActor.GetHeight();

	D3DXVECTOR3 v3Eye(0.0f, -(m_fEyeY * 8.9f + m_fZoomY), 0.0f); // x, y z
	D3DXVECTOR3 v3Target(0.0f, m_fTargetY, fActorHeight / 2); // x, y, z
	D3DXVECTOR3 v3Up(0.0f, 0.0f, 1.0f);
	pCam->SetViewParams(v3Eye, v3Target, v3Up);

	CPythonGraphic::instance().UpdateViewMatrix();

	CPythonGraphic::Instance().SetPerspective(10.0f, fWidth / fHeight, 100.0f, 15000.0f);

	m_pModelInstance->Render();

	CCameraManager::instance().ResetToPreviousCamera();
	CPythonGraphic::instance().RestoreViewport();
	CPythonGraphic::Instance().PopState();
	CPythonGraphic::Instance().SetPerspective(fFov, fAspect, fNearY, fFarY);
	m_pRenderTargetTexture->ResetRenderTarget();
	STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, bIsFog);
}

#endif
