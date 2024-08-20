#pragma once

#if defined(RENDER_TARGET)

#include <memory>
#include "GrpRenderTargetTexture.h"

class CInstanceBase;
class CGraphicImageInstance;

class CRenderTarget
{
	using TCharacterInstanceMap = std::map<DWORD, CInstanceBase*>;

public:
	CRenderTarget(DWORD dwWidth, DWORD dwHeight);
	~CRenderTarget();

	void SetVisibility(bool bShow);
	void RenderTexture() const;
	void SetRenderingRect(RECT* pRect) const;

	void SelectModel(DWORD dwIndex);
	bool CreateBackground(const char* c_szImgPath, DWORD dwWidth, DWORD dwHeight);
	void RenderBackground() const;
	void UpdateModel();
	void DeformModel() const;
	void RenderModel() const;
	void SetZoom(bool bZoom);

	void CreateTextures() const;
	void ReleaseTextures() const;

private:
	std::unique_ptr<CInstanceBase> m_pModelInstance;
	std::unique_ptr<CGraphicImageInstance> m_pBackgroundImage;
	std::unique_ptr<CGraphicRenderTargetTexture> m_pRenderTargetTexture;

	float m_fModelRotation;
	float m_fEyeY;
	float m_fTargetY;
	float m_fTargetHeight;
	float m_fZoomY;
	bool m_bShow;
};

#endif
