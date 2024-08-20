#pragma once

#include <cstdint>
#include <memory>

#include "../GameLib/in_game_wiki.h"
#include "GrpWikiRenderTargetTexture.h"

class CInstanceBase;
class CGraphicImageInstance;

class CWikiRenderTarget
{
	using TCharacterInstanceMap = std::map<unsigned int, CInstanceBase*>;
	
	public:
		CWikiRenderTarget(unsigned int width, unsigned int height);
		virtual ~CWikiRenderTarget();
	
	public:
		void		SetVisibility(bool flag) { m_visible = flag; }
		
		void		CreateTextures() const;
		void		ReleaseTextures() const;
		void		RenderTexture() const;
		
		void		SetRenderingRect(RECT* rect) const;
		void		SetRenderingBox(RECT* renderBox) const;
		
		void		SelectModel(unsigned int model_vnum);
		void		UpdateModel();
		void		DeformModel() const;
		void		RenderModel() const;
		
		void		SetWeapon(unsigned int dwVnum);
		void		SetArmor(unsigned int vnum);
		void		SetHair(unsigned int vnum);
		void		SetSash(unsigned int vnum);
		void		SetModelV3Eye(float x, float y, float z);
		void		SetModelV3Target(float x, float y, float z);
		bool		CreateBackground(const char* imgPath, DWORD width, DWORD height);
		void		RenderBackground() const;

	private:
		std::unique_ptr<CInstanceBase>						m_pModel;
		std::unique_ptr<CGraphicImageInstance>				m_background;
		std::unique_ptr<CGraphicWikiRenderTargetTexture>	m_renderTargetTexture;
		float												m_modelRotation;
		bool												m_visible;
		D3DXVECTOR3											m_v3Eye;
		D3DXVECTOR3											m_v3Target;
		D3DXVECTOR3											m_v3Up;
};
