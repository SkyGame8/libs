#pragma once

#include "GrpImageInstance.h"

#if defined(ENABLE_INGAME_WIKI)
typedef struct tagExpandedRECT
{
	LONG	left_top;
	LONG	left_bottom;
	LONG	top_left;
	LONG	top_right;
	LONG	right_top;
	LONG	right_bottom;
	LONG	bottom_left;
	LONG	bottom_right;
} ExpandedRECT, * PEXPANDEDRECT;
#endif

class CGraphicExpandedImageInstance : public CGraphicImageInstance
{
public:
	static DWORD Type();
	static void DeleteExpandedImageInstance(CGraphicExpandedImageInstance* pkInstance)
	{
		pkInstance->Destroy();
		ms_kPool.Free(pkInstance);
	}

	enum ERenderingMode
	{
		RENDERING_MODE_NORMAL,
		RENDERING_MODE_SCREEN,
		RENDERING_MODE_COLOR_DODGE,
		RENDERING_MODE_MODULATE,
	};

public:
	CGraphicExpandedImageInstance();
	virtual ~CGraphicExpandedImageInstance();

	void Destroy();

	void SetDepth(float fDepth);
	void SetOrigin();
	void SetOrigin(float fx, float fy);
	void SetRotation(float fRotation);
	void SetScale(float fx, float fy);
	void SetRenderingRect(float fLeft, float fTop, float fRight, float fBottom);
	void SetRenderingRectWithScale(float fLeft, float fTop, float fRight, float fBottom);
	void SetRenderingMode(int iMode);
#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
	long GetRenderingWidth();
	long GetRenderingHeight();
#endif
	void SetDiffuseColor(float fr, float fg, float fb, float fa);
	void RenderCoolTime(float fCoolTime);
#if defined(ENABLE_INGAME_WIKI)
	void SetRenderBox(RECT& renderBox);
	DWORD GetPixelColor(DWORD x, DWORD y);
	int GetRenderWidth();
	int GetRenderHeight();
	void SetExpandedRenderingRect(float fLeftTop, float fLeftBottom, float fTopLeft, float fTopRight, float fRightTop, float fRightBottom, float fBottomLeft, float fBottomRight);
	void iSetRenderingRect(int iLeft, int iTop, int iRight, int iBottom);
	void iSetExpandedRenderingRect(int iLeftTop, int iLeftBottom, int iTopLeft, int iTopRight, int iRightTop, int iRightBottom, int iBottomLeft, int iBottomRight);
	void SetTextureRenderingRect(float fLeft, float fTop, float fRight, float fBottom);
	void SetExpandedRenderingRectWithScale(float fLeftTop, float fLeftBottom, float fTopLeft, float fTopRight, float fRightTop, float fRightBottom, float fBottomLeft, float fBottomRight);
	void LeftRightReverseExpand();
#endif

protected:
	void Initialize();

	void OnRender(
#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
const double scaling = 1.0
#endif
	);
	void OnSetImagePointer();
	void OnRenderCoolTime(float fCoolTime);

	BOOL OnIsType(DWORD dwType);
#if defined(ENABLE_INGAME_WIKI)
private:
	void SaveColorMap();
#endif

protected:
	float m_fDepth;
	D3DXVECTOR2 m_v2Origin;
#ifndef ENABLE_RENDER_TARGET_SYSTEM
	D3DXVECTOR2 m_v2Scale;
#endif
	float m_fRotation;
#if defined(ENABLE_INGAME_WIKI)
	ExpandedRECT m_RenderingRect;
	RECT m_TextureRenderingRect;
	RECT m_renderBox;
	DWORD* m_pColorMap;
	bool m_bLeftRightReverse;
#else
	RECT m_RenderingRect;
#endif
	int m_iRenderingMode;

public:
	static void CreateSystem(UINT uCapacity);
	static void DestroySystem();

	static CGraphicExpandedImageInstance* New();
	static void Delete(CGraphicExpandedImageInstance* pkImgInst);

	static CDynamicPool<CGraphicExpandedImageInstance> ms_kPool;
};
