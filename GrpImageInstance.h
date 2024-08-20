#pragma once

#include "GrpImage.h"
#include "GrpIndexBuffer.h"
#include "GrpVertexBufferDynamic.h"
#include "Pool.h"
#include "../UserInterface/Locale_inc.h"
#include <memory>
#include <optional>

class CGraphicImageInstance
{
public:
    static DWORD Type();
    BOOL IsType(DWORD dwType);

public:
    CGraphicImageInstance();
    virtual ~CGraphicImageInstance();

    void Destroy();

    void Render(
#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
        const double scaling = 1.0
#endif
    );
    void RenderCoolTime(float fCoolTime);

    void SetDiffuseColor(float fr, float fg, float fb, float fa);
    void SetPosition(float fx, float fy);
#if defined(ENABLE_IMAGE_SCALE)
    void SetScale(float fx, float fy);
#endif
    const D3DXVECTOR2& GetScale() const;

    void SetImagePointer(std::shared_ptr<CGraphicImage> pImage);
    void ReloadImagePointer(std::shared_ptr<CGraphicImage> pImage);
    [[nodiscard]] bool IsEmpty() const;

    [[nodiscard]] int GetWidth() const;
    [[nodiscard]] int GetHeight() const;

    void LeftRightReverse();

    [[nodiscard]] std::shared_ptr<CGraphicTexture> GetTexturePointer();
    [[nodiscard]] const CGraphicTexture& GetTextureReference() const;
    [[nodiscard]] std::shared_ptr<CGraphicImage> GetGraphicImagePointer();

    bool operator == (const CGraphicImageInstance& rhs) const;

protected:
    void Initialize();

    virtual void OnRender(
#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
        const double scaling = 1.0
#endif
    );
    virtual void OnSetImagePointer();
    virtual void OnRenderCoolTime(float fCoolTime);

    virtual BOOL OnIsType(DWORD dwType);

protected:
    D3DXCOLOR m_DiffuseColor;
    D3DXVECTOR2 m_v2Position;
    D3DXVECTOR2 m_v2Scale;

    std::shared_ptr<CGraphicImage> m_roImage;

    bool m_bLeftRightReverse;

public:
    static void CreateSystem(UINT uCapacity);
    static void DestroySystem();

    static std::unique_ptr<CGraphicImageInstance> New();
    static void Delete(std::unique_ptr<CGraphicImageInstance> pkImgInst);

    static CDynamicPool<CGraphicImageInstance> ms_kPool;
};
