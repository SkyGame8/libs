#include "StdAfx.h"
#include "CRenderTarget.h"
#include "../EterLib/Camera.h"
#include "../EterLib/CRenderTargetManager.h"
#include "../EterPythonLib/PythonGraphic.h"

#include "../EterBase/CRC32.h"
#include "../GameLib/GameType.h"
#include "../GameLib/MapType.h"
#include "../GameLib/ItemData.h"
#include "../GameLib/ActorInstance.h"
#include "../UserInterface/InstanceBase.h"
#include "../UserInterface/AbstractPlayer.h"

#include "ResourceManager.h"

#include <Windows.h>
#include <thread>

CRenderTarget::CRenderTarget(const DWORD width, const DWORD height) : m_pModel(nullptr), m_background(nullptr), m_modelRotation(0), m_visible(false)
{
	auto pTex = new CGraphicRenderTargetTexture;
	if (!pTex->Create(width, height, D3DFMT_X8R8G8B8, D3DFMT_D16)) {
		delete pTex;
		TraceError("CRenderTarget::CRenderTarget: Could not create CGraphicRenderTargetTexture %dx%d", width, height);
		throw std::runtime_error("CRenderTarget::CRenderTarget: Could not create CGraphicRenderTargetTexture");
	}

	f_zoom = 0.0f;
	f_target_z = 95.0f;

	m_renderTargetTexture = std::unique_ptr<CGraphicRenderTargetTexture>(pTex);
	m_cameraPosition = D3DXVECTOR3(0.0f, -1500.0f, 500.0f);
	m_targetPosition = D3DXVECTOR3(0.0f, 0.0f, 120.0f);
	m_direction = m_targetPosition - m_cameraPosition;
	D3DXVec3Normalize(&m_direction, &m_direction);

}

CRenderTarget::~CRenderTarget(){}


void CRenderTarget::RenderPlayer()
{
	IAbstractPlayer& rPlayer = IAbstractPlayer::GetSingleton();

	if (&rPlayer == nullptr)
		return;

	CInstanceBase* player = rPlayer.NEW_GetMainActorPtr();

	if (player == nullptr)
		return;

	CInstanceBase::SCreateData kCreateData{};

	kCreateData.m_bType = CActorInstance::TYPE_PC; // Dynamic Type
	kCreateData.m_dwRace = player->GetRace();

	auto model = std::make_unique<CInstanceBase>();
	if (!model->Create(kCreateData))
	{
		if (m_pModel)
			m_pModel.reset();
		return;
	}

	m_pModel = std::move(model);
	m_pModel->NEW_SetPixelPosition(TPixelPosition(0, 0, 0));
	m_pModel->GetGraphicThingInstancePtr()->ClearAttachingEffect();
	m_modelRotation = 0.0f;
	m_pModel->SetAlwaysRender(true);
	m_pModel->SetRotation(0.0f);

	auto& camera_manager = CCameraManager::instance();
	camera_manager.SetCurrentCamera(CCameraManager::RENDERTARGET_CAMERA);
	camera_manager.GetCurrentCamera()->SetTargetHeight(110.0);
	camera_manager.ResetToPreviousCamera();
}

void CRenderTarget::ZoomCamera(int direction)
{
	const D3DXVECTOR3 difference = (m_targetPosition - m_cameraPosition);
	float distance = D3DXVec3Length(&difference);
	if (((distance > 2300.0f) && (direction < 0)) || ((distance < 1000.0f) && (direction > 0)))
		return;

	direction = direction > 0 ? 1 : -1;

	// Adjust the zoom speed or factor here
	D3DXVECTOR3 newPosition = m_cameraPosition + (m_direction * direction * 100.0f); // Adjusted zoom factor
	D3DXVec3Lerp(&m_cameraPosition, &m_cameraPosition, &newPosition, 0.5f);
}

void CRenderTarget::SetZoom(bool bZoom)
{
	if (!m_visible || !m_pModel)
		return;

	// تحديد عامل التكبير بناءً على نوع النموذج
	float scaleFactor = GetModelScaleFactor();

	// ضبط قيم التكبير بناءً على عامل التكبير
	if (!bZoom)
	{
		f_zoom += scaleFactor; // زيادة مستوى التكبير
		if (f_zoom > 10.0f) // تحديد الحد الأقصى للتكبير
			f_zoom = 10.0f;
	}
	else
	{
		f_zoom -= scaleFactor; // تقليل مستوى التكبير
		if (f_zoom < -10.0f) // تحديد الحد الأدنى للتكبير
			f_zoom = -10.0f;
	}

	// تحديث موضع الكاميرا بناءً على مستوى التكبير
	const float baseDistance = 1500.0f; // المسافة الأساسية للكاميرا
	float distance = baseDistance + f_zoom * 100.0f; // تعديل المسافة بناءً على التكبير
	m_cameraPosition = D3DXVECTOR3(0.0f, -distance, 500.0f);
	m_targetPosition = D3DXVECTOR3(0.0f, 0.0f, 120.0f);

	// تحديث الاتجاه بناءً على المواضع الجديدة
	m_direction = m_targetPosition - m_cameraPosition;
	D3DXVec3Normalize(&m_direction, &m_direction);
}



float CRenderTarget::GetModelScaleFactor()
{
	if (!m_pModel)
		return 1.0f;

	// الحصول على نوع النموذج
	DWORD race = m_pModel->GetRace();

	// تحديد عامل التكبير بناءً على نوع النموذج
	switch (race)
	{
	case 0: // نموذج اللاعب
		return 1.0f;
	case 1: // نموذج الزعيم الكبير
		return 0.5f;
	case 2: // نموذج الزعيم الصغير
		return 1.5f;
		// أضف حالات إضافية هنا بناءً على نوع النموذج
	default:
		return 1.0f;
	}
}


void CRenderTarget::SetModelRotation(float value)
{
	m_modelRotation += value;
}

void CRenderTarget::SetAutoRotate(bool value)
{
	m_autoRotate = value;
}

void CRenderTarget::SetVisibility(bool isShow)
{
	m_visible = isShow;
}

void CRenderTarget::SetArmor(DWORD vnum)
{
	if (!m_visible || !m_pModel)
		return;
	m_pModel->ChangeArmor(vnum);
}

void CRenderTarget::SetWeapon(DWORD vnum)
{
	if (!m_visible || !m_pModel)
		return;
	m_pModel->ChangeWeapon(vnum);
}

#ifdef ENABLE_EFFECT_SYSTEM
void CRenderTarget::SetBodyEffect(DWORD bodyEffect)
{
	if (!m_visible || !m_pModel)
		return;
	m_pModel->SetBodyEffect(bodyEffect);
}

void CRenderTarget::SetWeaponEffect(int vnum, int weaponType)
{
	if (!m_visible || !m_pModel)
		return;
	m_pModel->SetWeaponEffect(vnum, weaponType);
}
#endif

void CRenderTarget::SetShining(int shining, int vnum)
{
	if (!m_visible || !m_pModel)
		return;
	m_pModel->SetShining(shining, vnum);
}

void CRenderTarget::ChangeHair(DWORD vnum)
{
	if (!m_visible || !m_pModel)
		return;
	m_pModel->ChangeHair(vnum);
}

void CRenderTarget::SetSash(DWORD vnum)
{
	if (!m_visible || !m_pModel)
		return;

	m_pModel->ChangeAcce(vnum);
}

void CRenderTarget::ChangeEffect()
{
	if (!m_visible || !m_pModel)
		return;

	m_pModel->GetGraphicThingInstanceRef().RenderAllAttachingEffect(); 
	m_pModel->Refresh(CRaceMotionData::NAME_WAIT, true);
}

void CRenderTarget::RenderTexture() const
{
	m_renderTargetTexture->Render();
}

void CRenderTarget::SetRenderingRect(RECT* rect) const
{
	m_renderTargetTexture->SetRenderingRect(rect);
}

void CRenderTarget::CreateTextures() const
{
	m_renderTargetTexture->CreateTextures();
}

void CRenderTarget::ReleaseTextures() const
{
	m_renderTargetTexture->ReleaseTextures();
}

void CRenderTarget::SelectModel(const DWORD index)
{
	CInstanceBase::SCreateData kCreateData{};

	kCreateData.m_bType = CActorInstance::TYPE_PC;
	kCreateData.m_dwRace = index;

	auto model = std::make_unique<CInstanceBase>();
	if (!model->Create(kCreateData))
	{
		if (m_pModel)
			m_pModel.reset();
		return;
	}

	m_pModel = std::move(model);
	m_pModel->NEW_SetPixelPosition(TPixelPosition(0, 0, 0));
	m_pModel->GetGraphicThingInstancePtr()->ClearAttachingEffect();
	m_modelRotation = 0.0f;
	m_pModel->Refresh(CRaceMotionData::NAME_WAIT, true);
	m_pModel->SetLoopMotion(CRaceMotionData::NAME_WAIT);
	m_pModel->SetAlwaysRender(true);
	m_pModel->SetRotation(0.0f);

	auto& camera_manager = CCameraManager::instance();
	camera_manager.SetCurrentCamera(CCameraManager::RENDERTARGET_CAMERA);
	camera_manager.GetCurrentCamera()->SetTargetHeight(110.0);
	camera_manager.ResetToPreviousCamera();
}

bool CRenderTarget::CreateBackground(const char* imgPath, const DWORD width, const DWORD height)
{
	if (m_background)
		return false;

	m_background = std::make_unique<CGraphicImageInstance>();

	const auto graphic_image = dynamic_cast<CGraphicImage*>(CResourceManager::instance().GetResourcePointer(imgPath));
	if (!graphic_image)
	{
		m_background.reset();
		return false;
	}

	// تحويل المؤشر العادي إلى std::shared_ptr
	std::shared_ptr<CGraphicImage> sharedGraphicImage(graphic_image, [](CGraphicImage*) { /* لا تفعل شيئًا */ });

	m_background->SetImagePointer(sharedGraphicImage);
	m_background->SetScale(static_cast<float>(width) / graphic_image->GetWidth(), static_cast<float>(height) / graphic_image->GetHeight());
	return true;
}



void CRenderTarget::RenderBackground() const
{
	if (!m_visible)
		return;

	if (!m_background)
		return;

	auto& rectRender = *m_renderTargetTexture->GetRenderingRect();
	m_renderTargetTexture->SetRenderTarget();

	CGraphicRenderTargetTexture::Clear();
	CPythonGraphic::Instance().SetInterfaceRenderState();

	m_background->Render();
	m_renderTargetTexture->ResetRenderTarget();
}

void CRenderTarget::UpdateModel()
{
	if (!m_visible || !m_pModel)
		return;

	if (m_autoRotate)
		if (m_modelRotation < 360.0f)
			m_modelRotation += 1.0f;
		else
			m_modelRotation = 0.0f;

	m_pModel->SetRotation(m_modelRotation);
	m_pModel->Transform();
	m_pModel->GetGraphicThingInstanceRef().RotationProcess();
}

void CRenderTarget::ResetSettings()
{
	if (!m_visible || !m_pModel)
		return;

	f_zoom = 0.0f;
	f_target_z = 95.0f;
	m_modelRotation = 0.0f;
}


void CRenderTarget::DeformModel() const
{
	if (!m_visible)
		return;

	if (m_pModel)
		m_pModel->Deform();
}

void CRenderTarget::RefreshRender(float value)
{
	m_cameraPosition = D3DXVECTOR3(0.0f, -1500.0f, 500.0f);
	m_targetPosition = D3DXVECTOR3(0.0f, 0.0f, 120.0f);
	m_modelRotation = value;
}

void CRenderTarget::DownUp(float value)
{
	m_targetPosition += D3DXVECTOR3(0.0f, value, 0.0f);

	if (m_targetPosition.y == 705.0f || m_targetPosition.y == -475.0f)
		m_targetPosition += D3DXVECTOR3(0.0f, -value, 0.0f);
}

void CRenderTarget::DoEmotion()
{
	if (m_pModel->GetRace() > 7) // Close if it's not a player
		return;

	// List of Emotions ( codes from EterLib/RaceMotionData.h )
	int emotions[]	= { CRaceMotionData::NAME_CLAP,		CRaceMotionData::NAME_CONGRATULATION,	CRaceMotionData::NAME_FORGIVE, 
						CRaceMotionData::NAME_ANGRY,	CRaceMotionData::NAME_ATTRACTIVE,		CRaceMotionData::NAME_SAD, 
						CRaceMotionData::NAME_SHY,		CRaceMotionData::NAME_CHEERUP,			CRaceMotionData::NAME_BANTER, 
						CRaceMotionData::NAME_JOY,		CRaceMotionData::NAME_CHEERS_1,			CRaceMotionData::NAME_CHEERS_2, 
						CRaceMotionData::NAME_DANCE_1,	CRaceMotionData::NAME_DANCE_2,			CRaceMotionData::NAME_DANCE_3, 
						CRaceMotionData::NAME_DANCE_4,	CRaceMotionData::NAME_DANCE_5 };

	int randomValue = rand() % (sizeof(emotions) / sizeof(emotions[0])); // Get a random value between 1 and size of Emotions Array
	m_pModel->Refresh(emotions[randomValue], false); // Set up a random animation
}

void CRenderTarget::RenderModel() const
{
	if (!m_visible)
		return;

	auto& python_graphic = CPythonGraphic::Instance();
	auto& camera_manager = CCameraManager::instance();
	auto& state_manager = CStateManager::Instance();

	auto& rectRender = *m_renderTargetTexture->GetRenderingRect();

	if (!m_pModel)
		return;

	m_renderTargetTexture->SetRenderTarget();

	if (!m_background)
		m_renderTargetTexture->Clear();

	python_graphic.ClearDepthBuffer();

	const auto fov = python_graphic.GetFOV();
	const auto aspect = python_graphic.GetAspect();
	const auto near_y = python_graphic.GetNear();
	const auto far_y = python_graphic.GetFar();

	const auto width = static_cast<float>(rectRender.right - rectRender.left);
	const auto height = static_cast<float>(rectRender.bottom - rectRender.top);

	state_manager.SetRenderState(D3DRS_FOGENABLE, FALSE);

	python_graphic.SetViewport(0.0f, 0.0f, width, height);
	python_graphic.PushState();

	camera_manager.SetCurrentCamera(CCameraManager::RENDERTARGET_CAMERA);
	camera_manager.GetCurrentCamera()->SetViewParams(m_cameraPosition, m_targetPosition, D3DXVECTOR3{ 0.0f, 0.0f, 1.0f });

	python_graphic.UpdateViewMatrix();
	python_graphic.SetPerspective(10.0f + f_zoom, width / height, 100.0f, 3000.0f);


	m_pModel->Render();
	m_pModel->GetGraphicThingInstanceRef().RenderAllAttachingEffect();
#ifdef ENABLE_EFFECT_SYSTEM
	m_pModel->GetGraphicThingInstanceRef().RenderAllAttachingEffect();
#endif

	camera_manager.ResetToPreviousCamera();
	python_graphic.RestoreViewport();
	python_graphic.PopState();
	python_graphic.SetPerspective(fov, aspect, near_y, far_y);
	m_renderTargetTexture->ResetRenderTarget();
	state_manager.SetRenderState(D3DRS_FOGENABLE, TRUE);
}
