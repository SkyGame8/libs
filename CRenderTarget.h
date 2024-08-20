#pragma once

#include <memory>
#include "GrpRenderTargetTexture.h"

class CInstanceBase;
class CGraphicImageInstance;
class CRenderTarget
{
	using TCharacterInstanceMap = std::map<DWORD, CInstanceBase*>;

public:
	CRenderTarget(DWORD width, DWORD height);
	~CRenderTarget();


	void SetVisibility(bool isShow);
	void RenderTexture() const;
	void SetRenderingRect(RECT* rect) const;
	void RenderPlayer();
	void ZoomCamera(int direction);
	void SetZoom(bool bZoom);
	float GetModelScaleFactor();
	void ResetSettings();
	void RefreshRender(float value);
	void DownUp(float value);
	void DoEmotion();
	void SetModelRotation(float value);
	void SetAutoRotate(bool value);
	void SelectModel(DWORD index);
	bool CreateBackground(const char* imgPath, DWORD width, DWORD height);
	void RenderBackground() const;
	void UpdateModel();
	void DeformModel() const;
	void RenderModel() const;
	void SetWeapon(DWORD dwVnum);
	void SetArmor(DWORD vnum);
	void SetSash(DWORD vnum);

	void SetShining(int shining, int vnum);

#ifdef ENABLE_EFFECT_SYSTEM
	void SetBodyEffect(DWORD bodyEffect);
	void SetWeaponEffect(int vnum, int weaponType);
#endif

	void ChangeHair(DWORD vnum);
	void ChangeEffect();
	void CreateTextures() const;
	void ReleaseTextures() const;


private:
	std::unique_ptr<CInstanceBase> m_pModel;
	std::unique_ptr<CGraphicImageInstance> m_background;
	std::unique_ptr<CGraphicRenderTargetTexture> m_renderTargetTexture;
	float m_modelRotation;
	bool m_visible;
	bool m_autoRotate = true;
	D3DXVECTOR3 m_cameraPosition;
	D3DXVECTOR3 m_targetPosition;
	D3DXVECTOR3 m_direction;
	float f_zoom;
	float f_target_z;
};
