#include "StdAfx.h"
#include "GrpTextInstance.h"
#include "StateManager.h"
#include "IME.h"
#include "TextTag.h"
#include "../EterLocale/StringCodec.h"
#include "../EterBase/Utils.h"
#include "../EterLocale/Arabic.h"

#include "../UserInterface/Locale_inc.h"
#ifdef ENABLE_TEXT_IMAGE_LINE
#include "ResourceManager.h"
#endif

extern DWORD GetDefaultCodePage();

const float c_fFontFeather = 0.5f;

CDynamicPool<CGraphicTextInstance> CGraphicTextInstance::ms_kPool;

static int gs_mx = 0;
static int gs_my = 0;

static std::wstring gs_hyperlinkText;

void CGraphicTextInstance::Hyperlink_UpdateMousePos(int x, int y)
{
	gs_mx = x;
	gs_my = y;
	gs_hyperlinkText = L"";
}

int CGraphicTextInstance::Hyperlink_GetText(char* buf, int len)
{
	if (gs_hyperlinkText.empty())
		return 0;

	int codePage = GetDefaultCodePage();

	return Ymir_WideCharToMultiByte(codePage, 0, gs_hyperlinkText.c_str(), gs_hyperlinkText.length(), buf, len, NULL, NULL);
}

int CGraphicTextInstance::__DrawCharacter(CGraphicFontTexture* pFontTexture, WORD codePage, wchar_t text, DWORD dwColor)
{
	CGraphicFontTexture::TCharacterInfomation* pInsCharInfo = pFontTexture->GetCharacterInfomation(codePage, text);

#ifdef WJ_MULTI_TEXTLINE
	bool bHasToken = false;
	if (pInsCharInfo && !m_bDisableEnterToken)
	{
		if (m_wcEndLine != NULL)
		{
			if (m_wcEndLine == '\\' && text == 'n')
			{
				m_pCharInfoVector.erase(m_pCharInfoVector.begin() + m_pCharInfoVector.size() - 1);
				m_bEnterTokenVector.erase(m_bEnterTokenVector.begin() + m_bEnterTokenVector.size() - 1);
				bHasToken = true;
			}
		}
	}
	m_wcEndLine = text;
#endif

	if (pInsCharInfo)
	{
#ifdef WJ_MULTI_TEXTLINE
		m_bEnterTokenVector.push_back(bHasToken);
#endif
		m_dwColorInfoVector.push_back(dwColor);
		m_pCharInfoVector.push_back(pInsCharInfo);

		m_textWidth += pInsCharInfo->advance;
		m_textHeight = max(pInsCharInfo->height, m_textHeight);
		return pInsCharInfo->advance;
	}

	return 0;
}

void CGraphicTextInstance::__GetTextPos(DWORD index, float* x, float* y)
{
	index = min(index, m_pCharInfoVector.size());

	float sx = 0;
	float sy = 0;
	float fFontMaxHeight = 0;

#if defined(ENABLE_INGAME_WIKI)
	for (DWORD i = m_startPos; i < index; ++i)
#else
	for (DWORD i = 0; i < index; ++i)
#endif
	{
		if (sx + float(m_pCharInfoVector[i]->width) > m_fLimitWidth) {
			sx = 0;
			sy += fFontMaxHeight;
		}

		sx += float(m_pCharInfoVector[i]->advance);
		fFontMaxHeight = max(float(m_pCharInfoVector[i]->height), fFontMaxHeight);
	}

	*x = sx;
	*y = sy;
}

bool isNumberic(const char chr)
{
	if (chr >= '0' && chr <= '9')
		return true;
	return false;
}

bool IsValidToken(const char* iter)
{
	return	iter[0] == '@' &&
		isNumberic(iter[1]) &&
		isNumberic(iter[2]) &&
		isNumberic(iter[3]) &&
		isNumberic(iter[4]);
}

const char* FindToken(const char* begin, const char* end)
{
	while (begin < end)
	{
		begin = std::find(begin, end, '@');

		if (end - begin > 5 && IsValidToken(begin))
		{
			return begin;
		}
		else
		{
			++begin;
		}
	}

	return end;
}

int ReadToken(const char* token)
{
	int nRet = (token[1] - '0') * 1000 + (token[2] - '0') * 100 + (token[3] - '0') * 10 + (token[4] - '0');
	if (nRet == 9999)
		return CP_UTF8;
	return nRet;
}

void CGraphicTextInstance::Update()
{
	if (m_isUpdate) // 문자열이 바뀌었을 때만 업데이트 한다.
		return;

	if (m_roText.IsNull())
	{
		Tracef("CGraphicTextInstance::Update - 폰트가 설정되지 않았습니다\n");
		return;
	}

	if (m_roText->IsEmpty())
		return;

	CGraphicFontTexture* pFontTexture = m_roText->GetFontTexturePointer();
	if (!pFontTexture)
		return;

	UINT defCodePage = GetDefaultCodePage();

	UINT dataCodePage = defCodePage; // 아랍 및 베트남 내부 데이터를 UTF8 을 사용하려 했으나 실패

	CGraphicFontTexture::TCharacterInfomation* pSpaceInfo = pFontTexture->GetCharacterInfomation(dataCodePage, ' ');

	int spaceHeight = pSpaceInfo ? pSpaceInfo->height : 12;

	m_pCharInfoVector.clear();
	m_dwColorInfoVector.clear();
	m_hyperlinkVector.clear();

#ifdef WJ_MULTI_TEXTLINE
	m_bEnterTokenVector.clear();
	m_wcEndLine = NULL;
#endif

#ifdef ENABLE_TEXT_IMAGE_LINE
	if (m_imageVector.size() != 0)
	{
		for (std::vector<SImage>::iterator itor = m_imageVector.begin(); itor != m_imageVector.end(); ++itor)
		{
			SImage& rImg = *itor;
			if (rImg.pInstance)
			{
				CGraphicImageInstance::Delete(rImg.pInstance);
				rImg.pInstance = NULL;
			}
		}
	}
	m_imageVector.clear();
#endif

	m_textWidth = 0;
#ifdef WJ_MULTI_TEXTLINE
	m_textHeight = (m_iLineHeight != 0) ? m_iLineHeight : spaceHeight;
#else
	m_textHeight = spaceHeight;
#endif

	/* wstring begin */

	const char* begin = m_stText.c_str();
	const char* end = begin + m_stText.length();

	int wTextMax = (end - begin) * 2;
	wchar_t* wText = (wchar_t*)_alloca(sizeof(wchar_t) * wTextMax);

	DWORD dwColor = m_dwTextColor;

	/* wstring end */
	while (begin < end)
	{
		const char* token = FindToken(begin, end);

		int wTextLen = Ymir_MultiByteToWideChar(dataCodePage, 0, begin, token - begin, wText, wTextMax);

		if (m_isSecret)
		{
			for (int i = 0; i < wTextLen; ++i)
				__DrawCharacter(pFontTexture, dataCodePage, '*', dwColor);
		}
		else
		{
			if (defCodePage == CP_ARABIC) // ARABIC
			{

				wchar_t* wArabicText = (wchar_t*)_alloca(sizeof(wchar_t) * wTextLen);
				int wArabicTextLen = Arabic_MakeShape(wText, wTextLen, wArabicText, wTextLen);

				bool isEnglish = true;
				int nEnglishBase = wArabicTextLen - 1;

				// <<하이퍼 링크>>
				int x = 0;

				int len;
				int hyperlinkStep = 0;
				SHyperlink kHyperlink;
				std::wstring hyperlinkBuffer;
				SImage kImage;
				int imageStep = 0;
				std::wstring imageBuffer;
				int no_hyperlink = 0;

				// 심볼로 끝나면 아랍어 모드로 시작해야한다
				if (Arabic_IsInSymbol(wArabicText[wArabicTextLen - 1]))
				{
					isEnglish = false;
				}

				int i = 0;
				for (i = wArabicTextLen - 1; i >= 0; --i)
				{
					wchar_t wArabicChar = wArabicText[i];

					if (isEnglish)
					{
						// <<심볼의 경우 (ex. 기호, 공백)>> -> 영어 모드 유지.
						// <<(심볼이 아닌 것들 : 숫자, 영어, 아랍어)>>
						//	(1) 맨 앞의 심볼 or
						//	(2)
						//		1) 앞 글자가 아랍어 아님 &&
						//		2) 뒷 글자가 아랍어 아님 &&
						//		3) 뒷 글자가 심볼'|'이 아님 &&
						//		or
						//	(3) 현재 심볼이 '|'
						// <<아랍어 모드로 넘어가는 경우 : 심볼에서.>>
						//	1) 앞글자 아랍어
						//	2) 뒷글자 아랍어
						//
						if (Arabic_IsInSymbol(wArabicChar) && (
							(i == 0) ||
							(i > 0 &&
								!(Arabic_HasPresentation(wArabicText, i - 1) || Arabic_IsInPresentation(wArabicText[i + 1])) && // 앞글자, 뒷글자가 아랍어 아님.
								wArabicText[i + 1] != '|'
								) ||
							wArabicText[i] == '|'
							)) // if end.
						{
							// pass
							int temptest = 1;
						}
						// (1) 아랍어이거나 (2)아랍어 다음의 심볼이라면 아랍어 모드 전환
						else if (Arabic_IsInPresentation(wArabicChar) || Arabic_IsInSymbol(wArabicChar))
						{
							// 그 전까지의 영어를 그린다.
							for (int e = i + 1; e <= nEnglishBase;)
							{
								int ret = GetTextTag(&wArabicText[e], wArabicTextLen - e, len, hyperlinkBuffer);

								if (ret == TEXT_TAG_PLAIN || ret == TEXT_TAG_TAG)
								{
									if (hyperlinkStep == 1)
										hyperlinkBuffer.append(1, wArabicText[e]);
									else if (imageStep == 1)
										imageBuffer.append(1, wArabicText[e]);
									else
									{
										int charWidth = __DrawCharacter(pFontTexture, dataCodePage, wArabicText[e], dwColor);
										kHyperlink.ex += charWidth;
										//x += charWidth;

										// 기존 추가한 하이퍼링크의 좌표 수정.
										for (int j = 1; j <= no_hyperlink; j++)
										{
											if (m_hyperlinkVector.size() < j)
												break;

											SHyperlink& tempLink = m_hyperlinkVector[m_hyperlinkVector.size() - j];
											tempLink.ex += charWidth;
											tempLink.sx += charWidth;
										}
									}
								}
								else
								{
									if (ret == TEXT_TAG_COLOR)
										dwColor = htoi(hyperlinkBuffer.c_str(), 8);
									else if (ret == TEXT_TAG_RESTORE_COLOR)
										dwColor = m_dwTextColor;
									else if (ret == TEXT_TAG_HYPERLINK_START)
									{
										hyperlinkStep = 1;
										hyperlinkBuffer = L"";
									}
									else if (ret == TEXT_TAG_HYPERLINK_END)
									{
										if (hyperlinkStep == 1)
										{
											++hyperlinkStep;
											kHyperlink.ex = kHyperlink.sx = 0; // 실제 텍스트가 시작되는 위치
										}
										else
										{
											kHyperlink.text = hyperlinkBuffer;
											m_hyperlinkVector.push_back(kHyperlink);
											no_hyperlink++;

											hyperlinkStep = 0;
											hyperlinkBuffer = L"";
										}
									}
									else if (ret == TEXT_TAG_IMAGE_START)
									{
										imageStep = 1;
										imageBuffer = L"";
									}
									else if (ret == TEXT_TAG_IMAGE_END)
									{
										kImage.x = x;

										char retBuf[1024];
										int retLen = Ymir_WideCharToMultiByte(GetDefaultCodePage(), 0, imageBuffer.c_str(), imageBuffer.length(), retBuf, sizeof(retBuf) - 1, NULL, NULL);
										retBuf[retLen] = '\0';

										char szPath[255];
										_snprintf(szPath, sizeof(szPath), "icon/%s.tga", retBuf);
#if defined(ENABLE_FOXFS)
										if (CResourceManager::Instance().IsFileExist(szPath, __FUNCTION__))
#else
										if (CResourceManager::Instance().IsFileExist(szPath))
#endif
										{
											CGraphicImage* pImage = (CGraphicImage*)CResourceManager::Instance().GetResourcePointer(szPath);
											kImage.pInstance = CGraphicImageInstance::New();
											kImage.pInstance->SetImagePointer(pImage);
											m_imageVector.push_back(kImage);
											memset(&kImage, 0, sizeof(SImage));
											for (int i = 0; i < pImage->GetWidth() / (pSpaceInfo->width - 1); ++i)
												x += __DrawCharacter(pFontTexture, dataCodePage, ' ', dwColor);
											if (pImage->GetWidth() % (pSpaceInfo->width - 1) > 1)
												x += __DrawCharacter(pFontTexture, dataCodePage, ' ', dwColor);
										}
										imageStep = 0;
										imageBuffer = L"";
									}
								}
								e += len;
							}

							int charWidth = __DrawCharacter(pFontTexture, dataCodePage, Arabic_ConvSymbol(wArabicText[i]), dwColor);
							kHyperlink.ex += charWidth;

							// 기존 추가한 하이퍼링크의 좌표 수정.
							for (int j = 1; j <= no_hyperlink; j++)
							{
								if (m_hyperlinkVector.size() < j)
									break;

								SHyperlink& tempLink = m_hyperlinkVector[m_hyperlinkVector.size() - j];
								tempLink.ex += charWidth;
								tempLink.sx += charWidth;
							}

							isEnglish = false;
						}
					}
					else // [[[아랍어 모드]]]
					{
						// 아랍어이거나 아랍어 출력중 나오는 심볼이라면
						if (Arabic_IsInPresentation(wArabicChar) || Arabic_IsInSymbol(wArabicChar))
						{
							int charWidth = __DrawCharacter(pFontTexture, dataCodePage, Arabic_ConvSymbol(wArabicText[i]), dwColor);
							kHyperlink.ex += charWidth;
							x += charWidth;

							// 기존 추가한 하이퍼링크의 좌표 수정.
							for (int j = 1; j <= no_hyperlink; j++)
							{
								if (m_hyperlinkVector.size() < j)
									break;

								SHyperlink& tempLink = m_hyperlinkVector[m_hyperlinkVector.size() - j];
								tempLink.ex += charWidth;
								tempLink.sx += charWidth;
							}
						}
						else // 영어이거나, 영어 다음에 나오는 심볼이라면,
						{
							nEnglishBase = i;
							isEnglish = true;
						}
					}
				}

				if (isEnglish)
				{
					for (int e = i + 1; e <= nEnglishBase;)
					{
						int ret = GetTextTag(&wArabicText[e], wArabicTextLen - e, len, hyperlinkBuffer);

						if (ret == TEXT_TAG_PLAIN || ret == TEXT_TAG_TAG)
						{
							if (hyperlinkStep == 1)
								hyperlinkBuffer.append(1, wArabicText[e]);
							else
							{
								int charWidth = __DrawCharacter(pFontTexture, dataCodePage, wArabicText[e], dwColor);
								kHyperlink.ex += charWidth;

								// 기존 추가한 하이퍼링크의 좌표 수정.
								for (int j = 1; j <= no_hyperlink; j++)
								{
									if (m_hyperlinkVector.size() < j)
										break;

									SHyperlink& tempLink = m_hyperlinkVector[m_hyperlinkVector.size() - j];
									tempLink.ex += charWidth;
									tempLink.sx += charWidth;
								}
							}
						}
						else
						{
							if (ret == TEXT_TAG_COLOR)
								dwColor = htoi(hyperlinkBuffer.c_str(), 8);
							else if (ret == TEXT_TAG_RESTORE_COLOR)
								dwColor = m_dwTextColor;
							else if (ret == TEXT_TAG_HYPERLINK_START)
							{
								hyperlinkStep = 1;
								hyperlinkBuffer = L"";
							}
							else if (ret == TEXT_TAG_HYPERLINK_END)
							{
								if (hyperlinkStep == 1)
								{
									++hyperlinkStep;
									kHyperlink.ex = kHyperlink.sx = 0; // 실제 텍스트가 시작되는 위치
								}
								else
								{
									kHyperlink.text = hyperlinkBuffer;
									m_hyperlinkVector.push_back(kHyperlink);
									no_hyperlink++;

									hyperlinkStep = 0;
									hyperlinkBuffer = L"";
								}
							}
							else if (ret == TEXT_TAG_IMAGE_START)
							{
								imageStep = 1;
								imageBuffer = L"";
							}
							else if (ret == TEXT_TAG_IMAGE_END)
							{
								kImage.x = x;

								char retBuf[1024];
								int retLen = Ymir_WideCharToMultiByte(GetDefaultCodePage(), 0, imageBuffer.c_str(), imageBuffer.length(), retBuf, sizeof(retBuf) - 1, NULL, NULL);
								retBuf[retLen] = '\0';

								char szPath[255];
								_snprintf(szPath, sizeof(szPath), "icon/%s.tga", retBuf);
#if defined(ENABLE_FOXFS)
								if (CResourceManager::Instance().IsFileExist(szPath, __FUNCTION__))
#else
								if (CResourceManager::Instance().IsFileExist(szPath))
#endif
								{
									CGraphicImage* pImage = (CGraphicImage*)CResourceManager::Instance().GetResourcePointer(szPath);
									kImage.pInstance = CGraphicImageInstance::New();
									kImage.pInstance->SetImagePointer(pImage);
									m_imageVector.push_back(kImage);
									memset(&kImage, 0, sizeof(SImage));
									for (int i = 0; i < pImage->GetWidth() / (pSpaceInfo->width - 1); ++i)
										x += __DrawCharacter(pFontTexture, dataCodePage, ' ', dwColor);
									if (pImage->GetWidth() % (pSpaceInfo->width - 1) > 1)
										x += __DrawCharacter(pFontTexture, dataCodePage, ' ', dwColor);
								}
								imageStep = 0;
								imageBuffer = L"";
							}
						}
						e += len;
					}

				}
			}
			else // 아랍외 다른 지역.
			{
				int x = 0;
				int len;
				int hyperlinkStep = 0;
				SHyperlink kHyperlink;
				std::wstring hyperlinkBuffer;

#ifdef ENABLE_TEXT_IMAGE_LINE
				SImage kImage;
				int imageStep = 0;
				std::wstring imageBuffer;
#endif

				for (int i = 0; i < wTextLen; )
				{
					int ret = GetTextTag(&wText[i], wTextLen - i, len, hyperlinkBuffer);

					if (ret == TEXT_TAG_PLAIN || ret == TEXT_TAG_TAG)
					{
						if (hyperlinkStep == 1)
							hyperlinkBuffer.append(1, wText[i]);
#ifdef ENABLE_TEXT_IMAGE_LINE
						else if (imageStep == 1)
							imageBuffer.append(1, wText[i]);
#endif
						else
						{
							int charWidth = __DrawCharacter(pFontTexture, dataCodePage, wText[i], dwColor);
							kHyperlink.ex += charWidth;
							x += charWidth;
						}
					}
					else
					{
						if (ret == TEXT_TAG_COLOR)
							dwColor = htoi(hyperlinkBuffer.c_str(), 8);
						else if (ret == TEXT_TAG_RESTORE_COLOR)
							dwColor = m_dwTextColor;
						else if (ret == TEXT_TAG_HYPERLINK_START)
						{
							hyperlinkStep = 1;
							hyperlinkBuffer = L"";
						}
						else if (ret == TEXT_TAG_HYPERLINK_END)
						{
							if (hyperlinkStep == 1)
							{
								++hyperlinkStep;
								kHyperlink.ex = kHyperlink.sx = x; // 실제 텍스트가 시작되는 위치
							}
							else
							{
								kHyperlink.text = hyperlinkBuffer;
								m_hyperlinkVector.push_back(kHyperlink);

								hyperlinkStep = 0;
								hyperlinkBuffer = L"";
							}
						}
#ifdef ENABLE_TEXT_IMAGE_LINE
						else if (ret == TEXT_TAG_IMAGE_START)
						{
							imageStep = 1;
							imageBuffer = L"";
						}
						else if (ret == TEXT_TAG_IMAGE_END)
						{
							kImage.x = x;

							char retBuf[1024];
							int retLen = Ymir_WideCharToMultiByte(GetDefaultCodePage(), 0, imageBuffer.c_str(), imageBuffer.length(), retBuf, sizeof(retBuf) - 1, NULL, NULL);
							retBuf[retLen] = '\0';

							char szPath[255];
							_snprintf(szPath, sizeof(szPath), "icon/%s.tga", retBuf);
#if defined(ENABLE_FOXFS)
							if (CResourceManager::Instance().IsFileExist(szPath, __FUNCTION__))
#else
							if (CResourceManager::Instance().IsFileExist(szPath))
#endif
							{
								CGraphicImage* pImage = (CGraphicImage*)CResourceManager::Instance().GetResourcePointer(szPath);
								kImage.pInstance = CGraphicImageInstance::New();
								kImage.pInstance->SetImagePointer(pImage);
								m_imageVector.push_back(kImage);
								memset(&kImage, 0, sizeof(SImage));
								for (int i = 0; i < pImage->GetWidth() / (pSpaceInfo->width - 1); ++i)
									x += __DrawCharacter(pFontTexture, dataCodePage, ' ', dwColor);
								if (pImage->GetWidth() % (pSpaceInfo->width - 1) > 1)
									x += __DrawCharacter(pFontTexture, dataCodePage, ' ', dwColor);
							}
							imageStep = 0;
							imageBuffer = L"";
						}
#endif
					}
					i += len;
				}
			}
		}

		if (token < end)
		{
			int newCodePage = ReadToken(token);
			dataCodePage = newCodePage; // 아랍 및 베트남 내부 데이터를 UTF8 을 사용하려 했으나 실패
			begin = token + 5;
		}
		else
		{
			begin = token;
		}
	}

	pFontTexture->UpdateTexture();

	m_isUpdate = true;
}

void CGraphicTextInstance::Render(RECT* pClipRect
#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
, const double scaling
#endif
)
{
	if (!m_isUpdate)
		return;

	CGraphicText* pkText = m_roText.GetPointer();
	if (!pkText)
		return;

	CGraphicFontTexture* pFontTexture = pkText->GetFontTexturePointer();
	if (!pFontTexture)
		return;

#if defined(ENABLE_INGAME_WIKI)
	float textureWidth, textureHeight;
	pFontTexture->GetTextureSize(textureWidth, textureHeight);
#endif

	float fStanX = m_v3Position.x;
	float fStanY = m_v3Position.y + 1.0f;

	UINT defCodePage = GetDefaultCodePage();

	if (defCodePage == CP_ARABIC)
	{
		switch (m_hAlign)
		{
			case HORIZONTAL_ALIGN_LEFT:
			{
				fStanX -= m_textWidth;
				break;
			}
			case HORIZONTAL_ALIGN_CENTER:
			{
				fStanX -= float(m_textWidth / 2);
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else
	{
		switch (m_hAlign)
		{
			case HORIZONTAL_ALIGN_RIGHT:
			{
				fStanX -= m_textWidth;
				break;
			}
			case HORIZONTAL_ALIGN_CENTER:
			{
				fStanX -= float(m_textWidth / 2);
				break;
			}
			default:
			{
				break;
			}
		}
	}

	switch (m_vAlign)
	{
	case VERTICAL_ALIGN_BOTTOM:
		fStanY -= m_textHeight;
		break;

	case VERTICAL_ALIGN_CENTER:
		fStanY -= float(m_textHeight) / 2.0f;
		break;
	}

	//WORD FillRectIndices[6] = { 0, 2, 1, 2, 3, 1 };

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
	const auto xoff = (m_textWidth  * (1.0 - scaling)) / 2;
	const auto yoff = (m_textHeight * (1.0 - scaling)) / 2;
#endif

	STATEMANAGER.SaveRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	STATEMANAGER.SaveRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	DWORD dwFogEnable = STATEMANAGER.GetRenderState(D3DRS_FOGENABLE);
	DWORD dwLighting = STATEMANAGER.GetRenderState(D3DRS_LIGHTING);
	STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, false);
	STATEMANAGER.SetRenderState(D3DRS_LIGHTING, false);

	STATEMANAGER.SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

	{
		const float fFontHalfWeight = 1.0f;

		float fCurX;
		float fCurY;
#if defined(ENABLE_INGAME_WIKI)
		float fCurLocalX;
#endif

		float fFontSx;
		float fFontSy;
		float fFontEx;
		float fFontEy;
		float fFontWidth;
		float fFontHeight;
		float fFontMaxHeight;
		float fFontAdvance;

		SVertex akVertex[4];
		akVertex[0].z = m_v3Position.z;
		akVertex[1].z = m_v3Position.z;
		akVertex[2].z = m_v3Position.z;
		akVertex[3].z = m_v3Position.z;

#ifdef WJ_MULTI_TEXTLINE
		if (HasEnterToken())
		{
			ReAdjustedStanXY(0, fStanX, fStanY);
			fCurX = fStanX;
			//fCurY += fFontMaxHeight;
		}
#endif

		CGraphicFontTexture::TCharacterInfomation* pCurCharInfo;

#if defined(ENABLE_INGAME_WIKI)
		m_startPos = max(0, m_startPos);
		m_endPos = min(m_endPos, WORD(m_pCharInfoVector.size()));
		if (!m_isFixedRenderPos && (m_startPos >= m_endPos || m_isMultiLine || !m_isCursor || !m_isOutline))
		{
			m_startPos = 0;
			m_endPos = WORD(m_pCharInfoVector.size());
		}
#endif

		// 테두리
		if (m_isOutline)
		{
#if defined(ENABLE_INGAME_WIKI)
			if (m_isCursor && !m_isMultiLine && !m_isFixedRenderPos)
			{
				int curPos = min(CIME::GetCurPos(), m_pCharInfoVector.size());
				if (curPos < m_startPos)
					m_startPos = max(curPos, 0);
				else if (curPos > m_endPos)
				{
					m_endPos = curPos;
					m_startPos = min(m_endPos, WORD(m_pCharInfoVector.size()) - 1);
					fCurX = 0;
					for (; m_startPos >= 0; --m_startPos)
					{
						if (fCurX + float(m_pCharInfoVector[m_startPos]->width) > m_fLimitWidth)
						{
							++m_startPos;
							break;
						}
						fCurX += float(m_pCharInfoVector[m_startPos]->advance);

						if (m_startPos == 0)
							break;
					}
				}
			}
#endif

			fCurX = fStanX;
			fCurY = fStanY;
			fFontMaxHeight = 0.0f;
#if defined(ENABLE_INGAME_WIKI)
			for (WORD i = m_startPos; i < m_endPos; ++i)
			{
				pCurCharInfo = m_pCharInfoVector[i];

				fFontWidth = float(pCurCharInfo->width);
				fFontHeight = float(pCurCharInfo->height);
				fFontAdvance = float(pCurCharInfo->advance);

				float fXRenderLeft = 0.0f;
				float fXRenderRight = 0.0f;

				if (m_bUseRenderingRect)
				{
					if (fCurX - fStanX < m_RenderingRect.left)
					{
						if (fCurX - fStanX + fFontWidth <= m_RenderingRect.left)
						{
							fCurX += fFontAdvance;
							continue;
						}

						fXRenderLeft = -((float)(m_RenderingRect.left - (fCurX - fStanX)) / fFontWidth);
					}
					else if ((fCurX - fStanX) + fFontWidth > m_RenderingRect.right)
					{
						if ((fCurX - fStanX) >= m_RenderingRect.right)
						{
							fCurX += fFontAdvance;
							continue;
						}

						fXRenderRight = -((float)((fCurX - fStanX) + fFontWidth - m_RenderingRect.right) / fFontWidth);
					}
				}

				// NOTE : ÆuÆ® Aa·A¿¡ Width A|CNA≫ μO´I´U. - [levites]
				if ((fCurX + fFontWidth) - fStanX > m_fLimitWidth)
				{
					if (m_isMultiLine)
					{
						fCurX = fStanX;
						fCurY += fFontMaxHeight;
					}
					else
					{
						m_endPos = i;
						break;
					}
				}

#ifdef WJ_MULTI_TEXTLINE
				// 2019.09.20 - Owsap - Fix font outline.
				if (HasEnterToken())
				{
					ReAdjustedStanXY(0, fStanX, fStanY);
					fCurX = fStanX;
					//fCurY += fFontMaxHeight;
					break;
				}
#endif

				if (pClipRect)
				{
					if (fCurY <= pClipRect->top)
					{
						fCurX += fFontAdvance;
						continue;
					}
				}

				fFontSx = fCurX - 0.5f;
				fFontSy = fCurY - 0.5f;
				fFontEx = fFontSx + fFontWidth;
				fFontEy = fFontSy + fFontHeight;

				pFontTexture->SelectTexture(pCurCharInfo->index);
				STATEMANAGER.SetTexture(0, pFontTexture->GetD3DTexture());

				akVertex[0].u = pCurCharInfo->left;
				akVertex[0].v = pCurCharInfo->top;
				akVertex[1].u = pCurCharInfo->left;
				akVertex[1].v = pCurCharInfo->bottom;
				akVertex[2].u = pCurCharInfo->right;
				akVertex[2].v = pCurCharInfo->top;
				akVertex[3].u = pCurCharInfo->right;
				akVertex[3].v = pCurCharInfo->bottom;
				akVertex[3].color = akVertex[2].color = akVertex[1].color = akVertex[0].color = m_dwOutLineColor;


				float feather = 0.0f; // m_fFontFeather

				akVertex[0].y = fFontSy - feather;
				akVertex[1].y = fFontEy + feather;
				akVertex[2].y = fFontSy - feather;
				akVertex[3].y = fFontEy + feather;

				// ¿Þ
				akVertex[0].x = fFontSx - fFontHalfWeight - feather;
				akVertex[1].x = fFontSx - fFontHalfWeight - feather;
				akVertex[2].x = fFontEx - fFontHalfWeight + feather;
				akVertex[3].x = fFontEx - fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0) {
					akVertex[0].y += yoff;
					akVertex[1].y -= yoff;
					akVertex[2].y += yoff;
					akVertex[3].y -= yoff;
					akVertex[0].x += xoff;
					akVertex[1].x += xoff;
					akVertex[2].x -= xoff;
					akVertex[3].x -= xoff;

					auto new_color = D3DXCOLOR(m_dwOutLineColor);
					new_color.a *= scaling*scaling;

					for (auto& vert : akVertex) {
						vert.color = new_color;
					}
				}
#endif

				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);


				// ¿A¸￥
				akVertex[0].x = fFontSx + fFontHalfWeight - feather;
				akVertex[1].x = fFontSx + fFontHalfWeight - feather;
				akVertex[2].x = fFontEx + fFontHalfWeight + feather;
				akVertex[3].x = fFontEx + fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0) {
					akVertex[0].x += xoff;
					akVertex[1].x += xoff;
					akVertex[2].x -= xoff;
					akVertex[3].x -= xoff;
				}
#endif

				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

				akVertex[0].x = fFontSx - feather;
				akVertex[1].x = fFontSx - feather;
				akVertex[2].x = fFontEx + feather;
				akVertex[3].x = fFontEx + feather;

				// A§
				akVertex[0].y = fFontSy - fFontHalfWeight - feather;
				akVertex[1].y = fFontEy - fFontHalfWeight + feather;
				akVertex[2].y = fFontSy - fFontHalfWeight - feather;
				akVertex[3].y = fFontEy - fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0) {
					akVertex[0].x += xoff;
					akVertex[1].x += xoff;
					akVertex[2].x -= xoff;
					akVertex[3].x -= xoff;
					akVertex[0].y += yoff;
					akVertex[1].y -= yoff;
					akVertex[2].y += yoff;
					akVertex[3].y -= yoff;
				}
#endif

				// 20041216.myevan.DrawPrimitiveUP
				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

				// ¾Æ·¡
				akVertex[0].y = fFontSy + fFontHalfWeight - feather;
				akVertex[1].y = fFontEy + fFontHalfWeight + feather;
				akVertex[2].y = fFontSy + fFontHalfWeight - feather;
				akVertex[3].y = fFontEy + fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0) {
					akVertex[0].y += yoff;
					akVertex[1].y -= yoff;
					akVertex[2].y += yoff;
					akVertex[3].y -= yoff;
				}
#endif

				// 20041216.myevan.DrawPrimitiveUP
				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

				fCurX += fFontAdvance;
		}
#else
			CGraphicFontTexture::TPCharacterInfomationVector::iterator i;
			for (i = m_pCharInfoVector.begin(); i != m_pCharInfoVector.end(); ++i)
			{
				pCurCharInfo = *i;

				fFontWidth = float(pCurCharInfo->width);
				fFontHeight = float(pCurCharInfo->height);
				fFontAdvance = float(pCurCharInfo->advance);

				// NOTE : 폰트 출력에 Width 제한을 둡니다. - [levites]
				if ((fCurX + fFontWidth) - m_v3Position.x > m_fLimitWidth)
				{
					if (m_isMultiLine)
					{
						fCurX = fStanX;
						fCurY += fFontMaxHeight;
					}
					else
					{
						break;
					}
				}

#ifdef WJ_MULTI_TEXTLINE
				// 2019.09.20 - Owsap - Fix font outline.
				if (HasEnterToken())
				{
					ReAdjustedStanXY(0, fStanX, fStanY);
					fCurX = fStanX;
					//fCurY += fFontMaxHeight;
					break;
				}
#endif

				if (pClipRect)
				{
					if (fCurY <= pClipRect->top)
					{
						fCurX += fFontAdvance;
						continue;
					}
				}

				fFontSx = fCurX - 0.5f;
				fFontSy = fCurY - 0.5f;
				fFontEx = fFontSx + fFontWidth;
				fFontEy = fFontSy + fFontHeight;

				pFontTexture->SelectTexture(pCurCharInfo->index);
				STATEMANAGER.SetTexture(0, pFontTexture->GetD3DTexture());

				akVertex[0].u = pCurCharInfo->left;
				akVertex[0].v = pCurCharInfo->top;
				akVertex[1].u = pCurCharInfo->left;
				akVertex[1].v = pCurCharInfo->bottom;
				akVertex[2].u = pCurCharInfo->right;
				akVertex[2].v = pCurCharInfo->top;
				akVertex[3].u = pCurCharInfo->right;
				akVertex[3].v = pCurCharInfo->bottom;

				akVertex[3].color = akVertex[2].color = akVertex[1].color = akVertex[0].color = m_dwOutLineColor;

				float feather = 0.0f; //m_fFontFeather

				akVertex[0].y = fFontSy - feather;
				akVertex[1].y = fFontEy + feather;
				akVertex[2].y = fFontSy - feather;
				akVertex[3].y = fFontEy + feather;

				// 왼
				akVertex[0].x = fFontSx - fFontHalfWeight - feather;
				akVertex[1].x = fFontSx - fFontHalfWeight - feather;
				akVertex[2].x = fFontEx - fFontHalfWeight + feather;
				akVertex[3].x = fFontEx - fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0) {
					akVertex[0].y += yoff;
					akVertex[1].y -= yoff;
					akVertex[2].y += yoff;
					akVertex[3].y -= yoff;
					akVertex[0].x += xoff;
					akVertex[1].x += xoff;
					akVertex[2].x -= xoff;
					akVertex[3].x -= xoff;

					auto new_color = D3DXCOLOR(m_dwOutLineColor);
					new_color.a *= scaling*scaling;

					for (auto& vert : akVertex) {
						vert.color = new_color;
					}
				}
#endif

				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

				// 오른
				akVertex[0].x = fFontSx + fFontHalfWeight - feather;
				akVertex[1].x = fFontSx + fFontHalfWeight - feather;
				akVertex[2].x = fFontEx + fFontHalfWeight + feather;
				akVertex[3].x = fFontEx + fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0) {
					akVertex[0].x += xoff;
					akVertex[1].x += xoff;
					akVertex[2].x -= xoff;
					akVertex[3].x -= xoff;
				}
#endif

				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

				akVertex[0].x = fFontSx - feather;
				akVertex[1].x = fFontSx - feather;
				akVertex[2].x = fFontEx + feather;
				akVertex[3].x = fFontEx + feather;

				// 위
				akVertex[0].y = fFontSy - fFontHalfWeight - feather;
				akVertex[1].y = fFontEy - fFontHalfWeight + feather;
				akVertex[2].y = fFontSy - fFontHalfWeight - feather;
				akVertex[3].y = fFontEy - fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0) {
					akVertex[0].x += xoff;
					akVertex[1].x += xoff;
					akVertex[2].x -= xoff;
					akVertex[3].x -= xoff;
					akVertex[0].y += yoff;
					akVertex[1].y -= yoff;
					akVertex[2].y += yoff;
					akVertex[3].y -= yoff;
				}
#endif

				// 20041216.myevan.DrawPrimitiveUP
				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

				// 아래
				akVertex[0].y = fFontSy + fFontHalfWeight - feather;
				akVertex[1].y = fFontEy + fFontHalfWeight + feather;
				akVertex[2].y = fFontSy + fFontHalfWeight - feather;
				akVertex[3].y = fFontEy + fFontHalfWeight + feather;

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
				if (scaling != 1.0)	{
					akVertex[0].y += yoff;
					akVertex[1].y -= yoff;
					akVertex[2].y += yoff;
					akVertex[3].y -= yoff;
				}
#endif

				// 20041216.myevan.DrawPrimitiveUP
				if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
					STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

				fCurX += fFontAdvance;
			}
#endif
		}

		fCurX = fStanX;
		fCurY = fStanY;
		fFontMaxHeight = 0.0f;
#if defined(ENABLE_INGAME_WIKI)
		float fCountX = 0.0f;
		float fCountY = 0.0f;
		float addXL, addYT, addXR, addYB;

		for (WORD i = m_startPos; i < m_endPos; ++i)
		{
			pCurCharInfo = m_pCharInfoVector[i];
			fCurLocalX = fCurX - fStanX;
			fFontWidth = float(pCurCharInfo->width);
			fFontHeight = float(pCurCharInfo->height);
			fFontMaxHeight = max(fFontHeight, pCurCharInfo->height);
			fFontAdvance = float(pCurCharInfo->advance);
			
			float fXRenderLeft = 0.0f;
			float fXRenderRight = 0.0f;
			float fYRenderTop = 0.0f;
			float fYRenderBottom = 0.0f;
			
			if (m_bUseRenderingRect)
			{
				if (fCurLocalX < m_RenderingRect.left && !m_isMultiLine)
				{
					if (fCurLocalX + fFontWidth <= m_RenderingRect.left)
					{
						fCurX += fFontAdvance;
						continue;
					}
					
					fXRenderLeft = ((float)(m_RenderingRect.left - fCurLocalX) / fFontWidth);
				}
				else if (fCurLocalX + fFontWidth > m_RenderingRect.right && !m_isMultiLine)
				{
					if (fCurLocalX >= m_RenderingRect.right)
					{
						fCurX += fFontAdvance;
						continue;
					}
					
					fXRenderRight = ((float)(fCurLocalX + fFontWidth - m_RenderingRect.right) / fFontWidth);
				}
				
				if (m_RenderingRect.top)
					fYRenderTop = m_RenderingRect.top / fFontHeight;
				if (m_RenderingRect.bottom < fFontHeight)
					fYRenderBottom = 1.0f - (m_RenderingRect.bottom / fFontHeight);
			}
			

			if (fCurLocalX + fFontWidth > m_fLimitWidth)
			{
				if (m_isMultiLine)
				{
					if (GetDefaultCodePage() == CP_ARABIC) {
					} else {
						fCurX = fStanX;
						fCountX = 0.0f;
						fCurY += fFontMaxHeight;
						fCountY += fFontMaxHeight;
					}
				} else {
					break;
				}
			}

#ifdef WJ_MULTI_TEXTLINE
			if (HasEnterToken() && m_bEnterTokenVector[i] && !m_bDisableEnterToken)
			{
				ReAdjustedStanXY(i, fStanX, fStanY);
				fCurX = fStanX;
				fCurY += fFontMaxHeight;
				continue;
			}
#endif

			if (pClipRect) {
				if (fCurY <= pClipRect->top) {
					fCurX += fFontAdvance;
					continue;
				}
			}

			fFontSx = fCurX - 0.5f + fFontWidth * fXRenderLeft;
			fFontSy = fCurY - 0.5f + fFontHeight * fYRenderTop;
			fFontEx = fFontSx + fFontWidth * (1.0 - fXRenderRight - fXRenderLeft);
			fFontEy = fFontSy + fFontHeight * (1.0 - fYRenderBottom - fYRenderTop);

			addXR = addXL = addYT = addYB = 0.0f;

			if (!m_isMultiLine)
			{
				if (fCountX + fFontWidth < float(m_renderBox.left))
				{
					fCurX += fFontAdvance;
					fCountX += fFontAdvance;
					continue;
				}
				else if (fCountX < float(m_renderBox.left))
					addXL = float(m_renderBox.left) - fCountX;
				
				if (fCountY + fFontHeight < float(m_renderBox.top))
				{
					fCurX += fFontAdvance;
					fCountX += fFontAdvance;
					continue;
				}
				else if (fCountY < float(m_renderBox.top))
					addYT = float(m_renderBox.top) - fCountY;
				
				if (fCountX > m_textWidth - float(m_renderBox.right))
				{
					fCurX += fFontAdvance;
					fCountX += fFontAdvance;
					continue;
				}
				else if (fCountX + fFontWidth > m_textWidth - float(m_renderBox.right))
					addXR = fCountX + fFontWidth - m_textWidth + float(m_renderBox.right);
				
				if (fCountY > m_textHeight - float(m_renderBox.bottom))
				{
					fCurX += fFontAdvance;
					fCountX += fFontAdvance;
					continue;
				}
				else if (fCountY + fFontHeight > m_textHeight - float(m_renderBox.bottom))
					addYB = fCountY + fFontHeight - m_textHeight + float(m_renderBox.bottom);
			}
			
			pFontTexture->SelectTexture(pCurCharInfo->index);
			STATEMANAGER.SetTexture(0, pFontTexture->GetD3DTexture());

			float fTextureRenderLeft = (pCurCharInfo->right - pCurCharInfo->left) * fXRenderLeft;
			float fTextureRenderTop = (pCurCharInfo->bottom - pCurCharInfo->top) * fYRenderTop;
			float fTextureRenderRight = (pCurCharInfo->right - pCurCharInfo->left) * fXRenderRight;
			float fTextureRenderBottom = (pCurCharInfo->bottom - pCurCharInfo->top) * fYRenderBottom;
			akVertex[0].x = fFontSx + addXL;
			akVertex[0].y = fFontSy + addYT;
			akVertex[0].u = pCurCharInfo->left + fTextureRenderLeft + addXL / textureWidth;
			akVertex[0].v = pCurCharInfo->top + fTextureRenderTop + addYT / textureHeight;
			
			akVertex[1].x = fFontSx + addXL;
			akVertex[1].y = fFontEy - addYB;
			akVertex[1].u = pCurCharInfo->left + fTextureRenderLeft + addXL / textureWidth;
			akVertex[1].v = pCurCharInfo->bottom - fTextureRenderBottom - addYB / textureHeight;
			
			akVertex[2].x = fFontEx - addXR;
			akVertex[2].y = fFontSy + addYT;
			akVertex[2].u = pCurCharInfo->right - fTextureRenderRight - addXR / textureWidth;
			akVertex[2].v = pCurCharInfo->top + fTextureRenderTop + addYT / textureHeight;
			
			akVertex[3].x = fFontEx - addXR;
			akVertex[3].y = fFontEy - addYB;
			akVertex[3].u = pCurCharInfo->right - fTextureRenderRight - addXR / textureWidth;
			akVertex[3].v = pCurCharInfo->bottom - fTextureRenderBottom - addYB / textureHeight;
			akVertex[0].color = akVertex[1].color = akVertex[2].color = akVertex[3].color = m_dwColorInfoVector[i];
			
#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
			if (scaling != 1.0) {
				akVertex[0].x += xoff;
				akVertex[0].y += yoff;

				akVertex[1].x += xoff;
				akVertex[1].y -= yoff;

				akVertex[2].x -= xoff;
				akVertex[2].y += yoff;

				akVertex[3].x -= xoff;
				akVertex[3].y -= yoff;

				auto new_color = D3DXCOLOR(m_dwColorInfoVector[i]);
				new_color.a *= scaling*scaling;

				for (auto& vert : akVertex) {
					vert.color = new_color;
				}
			}
#endif

			if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
				STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
			
			fCurX += fFontAdvance;
			fCountX += fFontAdvance;
		}
	}
#else
		for (int i = 0; i < m_pCharInfoVector.size(); ++i)
		{
#ifdef WJ_MULTI_TEXTLINE
			if (HasEnterToken() && m_bEnterTokenVector[i] && !m_bDisableEnterToken)
			{
				ReAdjustedStanXY(i, fStanX, fStanY);
				fCurX = fStanX;
				fCurY += fFontMaxHeight;
				continue;
			}
#endif

			pCurCharInfo = m_pCharInfoVector[i];

			fFontWidth = float(pCurCharInfo->width);
			fFontHeight = float(pCurCharInfo->height);
			fFontMaxHeight = max(fFontHeight, pCurCharInfo->height);
			fFontAdvance = float(pCurCharInfo->advance);

			// NOTE : 폰트 출력에 Width 제한을 둡니다. - [levites]
			if ((fCurX + fFontWidth) - m_v3Position.x > m_fLimitWidth)
			{
				if (m_isMultiLine)
				{
					fCurX = fStanX;
					fCurY += fFontMaxHeight;
				}
				else
				{
					break;
				}
			}

			if (pClipRect)
			{
				if (fCurY <= pClipRect->top)
				{
					fCurX += fFontAdvance;
					continue;
				}
			}

			fFontSx = fCurX - 0.5f;
			fFontSy = fCurY - 0.5f;
			fFontEx = fFontSx + fFontWidth;
			fFontEy = fFontSy + fFontHeight;

			pFontTexture->SelectTexture(pCurCharInfo->index);
			STATEMANAGER.SetTexture(0, pFontTexture->GetD3DTexture());

			akVertex[0].x = fFontSx;
			akVertex[0].y = fFontSy;
			akVertex[0].u = pCurCharInfo->left;
			akVertex[0].v = pCurCharInfo->top;

			akVertex[1].x = fFontSx;
			akVertex[1].y = fFontEy;
			akVertex[1].u = pCurCharInfo->left;
			akVertex[1].v = pCurCharInfo->bottom;

			akVertex[2].x = fFontEx;
			akVertex[2].y = fFontSy;
			akVertex[2].u = pCurCharInfo->right;
			akVertex[2].v = pCurCharInfo->top;

			akVertex[3].x = fFontEx;
			akVertex[3].y = fFontEy;
			akVertex[3].u = pCurCharInfo->right;
			akVertex[3].v = pCurCharInfo->bottom;

			//m_dwColorInfoVector[i];
			//m_dwTextColor;
			akVertex[0].color = akVertex[1].color = akVertex[2].color = akVertex[3].color = m_dwColorInfoVector[i];

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
			if (scaling != 1.0) {
				akVertex[0].x += xoff;
				akVertex[0].y += yoff;

				akVertex[1].x += xoff;
				akVertex[1].y -= yoff;

				akVertex[2].x -= xoff;
				akVertex[2].y += yoff;

				akVertex[3].x -= xoff;
				akVertex[3].y -= yoff;

				auto new_color = D3DXCOLOR(m_dwColorInfoVector[i]);
				new_color.a *= scaling*scaling;

				for (auto& vert : akVertex) {
					vert.color = new_color;
				}
			}
#endif

			// 20041216.myevan.DrawPrimitiveUP
			if (CGraphicBase::SetPDTStream((SPDTVertex*)akVertex, 4))
				STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
			//STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, akVertex, sizeof(SVertex));

			fCurX += fFontAdvance;
		}
	}
#endif

	if (m_isCursor)
	{
		// Draw Cursor
		float sx, sy, ex, ey;
		TDiffuse diffuse;

		int curpos = CIME::GetCurPos();
		int compend = curpos + CIME::GetCompLen();

		__GetTextPos(curpos, &sx, &sy);

		// If Composition
		if (curpos < compend)
		{
			diffuse = 0x7fffffff;
			__GetTextPos(compend, &ex, &sy);
		}
		else
		{
			diffuse = 0xffffffff;
			ex = sx + 2;
		}

		// FOR_ARABIC_ALIGN
		if (defCodePage == CP_ARABIC)
		{
			sx += m_v3Position.x - m_textWidth;
			ex += m_v3Position.x - m_textWidth;
			sy += m_v3Position.y;
			ey = sy + m_textHeight;
		}
		else
		{
			sx += m_v3Position.x;
			sy += m_v3Position.y;
			ex += m_v3Position.x;
			ey = sy + m_textHeight;
		}

		switch (m_vAlign)
		{
		case VERTICAL_ALIGN_BOTTOM:
			sy -= m_textHeight;
			break;

		case VERTICAL_ALIGN_CENTER:
			sy -= float(m_textHeight) / 2.0f;
			break;
		}
		// 최적화 사항
		// 같은텍스쳐를 사용한다면... STRIP을 구성하고, 텍스쳐가 변경되거나 끝나면 DrawPrimitive를 호출해
		// 최대한 숫자를 줄이도록하자!

		TPDTVertex vertices[4];
		vertices[0].diffuse = diffuse;
		vertices[1].diffuse = diffuse;
		vertices[2].diffuse = diffuse;
		vertices[3].diffuse = diffuse;
		vertices[0].position = TPosition(sx, sy, 0.0f);
		vertices[1].position = TPosition(ex, sy, 0.0f);
		vertices[2].position = TPosition(sx, ey, 0.0f);
		vertices[3].position = TPosition(ex, ey, 0.0f);

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
		if (scaling != 1.0) {
			vertices[0].position.x += xoff;
			vertices[0].position.y += yoff;

			vertices[1].position.x -= xoff;
			vertices[1].position.y += yoff;

			vertices[2].position.x += xoff;
			vertices[2].position.y -= yoff;

			vertices[3].position.x -= xoff;
			vertices[3].position.y -= yoff;

			auto ncolor = D3DXCOLOR(diffuse);
			ncolor.a *= scaling*scaling;

			for (auto& vert : vertices) {
				vert.diffuse = ncolor;
			}
		}
#endif

		STATEMANAGER.SetTexture(0, NULL);

		// 2004.11.18.myevan.DrawIndexPrimitiveUP -> DynamicVertexBuffer
		CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT);
		if (CGraphicBase::SetPDTStream(vertices, 4))
			STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

		int ulbegin = CIME::GetULBegin();
		int ulend = CIME::GetULEnd();

		if (ulbegin < ulend)
		{
			__GetTextPos(curpos + ulbegin, &sx, &sy);
			__GetTextPos(curpos + ulend, &ex, &sy);

			sx += m_v3Position.x;
			sy += m_v3Position.y + m_textHeight;
			ex += m_v3Position.x;
			ey = sy + 2;

			vertices[0].diffuse = 0xFFFF0000;
			vertices[1].diffuse = 0xFFFF0000;
			vertices[2].diffuse = 0xFFFF0000;
			vertices[3].diffuse = 0xFFFF0000;
			vertices[0].position = TPosition(sx, sy, 0.0f);
			vertices[1].position = TPosition(ex, sy, 0.0f);
			vertices[2].position = TPosition(sx, ey, 0.0f);
			vertices[3].position = TPosition(ex, ey, 0.0f);

#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
			if (scaling != 1.0) {
				vertices[0].position.x += xoff;
				vertices[0].position.y += yoff;

				vertices[1].position.x -= xoff;
				vertices[1].position.y += yoff;

				vertices[2].position.x += xoff;
				vertices[2].position.y -= yoff;

				vertices[3].position.x -= xoff;
				vertices[3].position.y -= yoff;

				auto ncolor = D3DXCOLOR(diffuse);
				ncolor.a *= scaling*scaling;

				for (auto& vert : vertices) {
					vert.diffuse = ncolor;
				}
			}
#endif

			STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, c_FillRectIndices, D3DFMT_INDEX16, vertices, sizeof(TPDTVertex));
		}
	}

	STATEMANAGER.RestoreRenderState(D3DRS_SRCBLEND);
	STATEMANAGER.RestoreRenderState(D3DRS_DESTBLEND);

	STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, dwFogEnable);
	STATEMANAGER.SetRenderState(D3DRS_LIGHTING, dwLighting);

	// 금강경 링크 띄워주는 부분.
	if (m_hyperlinkVector.size() != 0)
	{
		int lx = gs_mx - m_v3Position.x;
		int ly = gs_my - m_v3Position.y;

		// 아랍은 좌표 부호를 바꿔준다.
		if (GetDefaultCodePage() == CP_ARABIC)
		{
			lx = -lx;
			ly = -ly + m_textHeight;
		}

		if (lx >= 0 && ly >= 0 && lx < m_textWidth && ly < m_textHeight)
		{
			std::vector<SHyperlink>::iterator it = m_hyperlinkVector.begin();

			while (it != m_hyperlinkVector.end())
			{
				SHyperlink& link = *it++;
				if (lx >= link.sx && lx < link.ex)
				{
					gs_hyperlinkText = link.text;
					/*
					OutputDebugStringW(link.text.c_str());
					OutputDebugStringW(L"\n");
					*/
					break;
				}
			}
		}
	}

#ifdef ENABLE_TEXT_IMAGE_LINE
	if (m_imageVector.size() != 0)
	{
		for (std::vector<SImage>::iterator itor = m_imageVector.begin(); itor != m_imageVector.end(); ++itor)
		{
			SImage& rImg = *itor;
			if (rImg.pInstance)
			{
				rImg.pInstance->SetPosition(fStanX + rImg.x, (fStanY + 7.0) - (rImg.pInstance->GetHeight() / 2));
				rImg.pInstance->Render(
#if defined(ENABLE_WINDOW_SLIDE_EFFECT)
scaling
#endif
				);
			}
		}
	}
#endif
}

#ifdef WJ_MULTI_TEXTLINE
void CGraphicTextInstance::SetLineHeight(int iLineHeight)
{
	m_iLineHeight += m_iLineHeight;
}

int CGraphicTextInstance::GetLineHeight()
{
	return m_iLineHeight;
}

bool CGraphicTextInstance::HasEnterToken()
{
	bool bHasEnterToken = false;

	for (DWORD i = 0; i < m_bEnterTokenVector.size() && !bHasEnterToken; ++i)
		bHasEnterToken = m_bEnterTokenVector[i];

	return bHasEnterToken;
}

void CGraphicTextInstance::ReAdjustedStanXY(int startPosition, float& fStanX, float& fStanY)
{
	startPosition = (startPosition > 0) ? startPosition + 1 : startPosition;

	UINT defCodePage = GetDefaultCodePage();
	WORD textWith = 0;

	for (DWORD i = startPosition; i < m_pCharInfoVector.size(); ++i)
	{
		if (m_bEnterTokenVector[i])
			break;

		textWith += m_pCharInfoVector[i]->advance;
	}

	fStanX = m_v3Position.x;
	fStanY = m_v3Position.y + 1.0f;

	if (defCodePage == CP_ARABIC)
	{
		switch (m_hAlign)
		{
		case HORIZONTAL_ALIGN_LEFT:
			fStanX -= textWith;
			break;

		case HORIZONTAL_ALIGN_CENTER:
			fStanX -= float(textWith / 2);
			break;
		}
	}
	else
	{
		switch (m_hAlign)
		{
		case HORIZONTAL_ALIGN_RIGHT:
			fStanX -= textWith;
			break;

		case HORIZONTAL_ALIGN_CENTER:
			fStanX -= float(textWith / 2);
			break;
		}
	}

	switch (m_vAlign)
	{
	case VERTICAL_ALIGN_BOTTOM:
		fStanY -= m_textHeight;
		break;

	case VERTICAL_ALIGN_CENTER:
		fStanY -= float(m_textHeight) / 2.0f;
		break;
	}
}

void CGraphicTextInstance::DisableEnterToken()
{
	m_bDisableEnterToken = true;
}
#endif

void CGraphicTextInstance::CreateSystem(UINT uCapacity)
{
	ms_kPool.Create(uCapacity);
}

void CGraphicTextInstance::DestroySystem()
{
	ms_kPool.Destroy();
}

CGraphicTextInstance* CGraphicTextInstance::New()
{
	return ms_kPool.Alloc();
}

void CGraphicTextInstance::Delete(CGraphicTextInstance* pkInst)
{
	pkInst->Destroy();
	ms_kPool.Free(pkInst);
}

void CGraphicTextInstance::ShowCursor()
{
	m_isCursor = true;
}

#if defined(ENABLE_INGAME_WIKI)
bool CGraphicTextInstance::IsShowCursor()
{
	return m_isCursor;
}
#endif

void CGraphicTextInstance::HideCursor()
{
	m_isCursor = false;
}

void CGraphicTextInstance::ShowOutLine()
{
	m_isOutline = true;
}

void CGraphicTextInstance::HideOutLine()
{
	m_isOutline = false;
}

void CGraphicTextInstance::SetColor(DWORD color)
{
	if (m_dwTextColor != color)
	{
		for (int i = 0; i < m_pCharInfoVector.size(); ++i)
			if (m_dwColorInfoVector[i] == m_dwTextColor)
				m_dwColorInfoVector[i] = color;

		m_dwTextColor = color;
	}
}

void CGraphicTextInstance::SetColor(float r, float g, float b, float a)
{
	SetColor(D3DXCOLOR(r, g, b, a));
}

void CGraphicTextInstance::SetOutLineColor(DWORD color)
{
	m_dwOutLineColor = color;
}

void CGraphicTextInstance::SetOutLineColor(float r, float g, float b, float a)
{
	m_dwOutLineColor = D3DXCOLOR(r, g, b, a);
}

void CGraphicTextInstance::SetSecret(bool Value)
{
	m_isSecret = Value;
}

void CGraphicTextInstance::SetOutline(bool Value)
{
	m_isOutline = Value;
}

void CGraphicTextInstance::SetFeather(bool Value)
{
	if (Value)
	{
		m_fFontFeather = c_fFontFeather;
	}
	else
	{
		m_fFontFeather = 0.0f;
	}
}

void CGraphicTextInstance::SetMultiLine(bool Value)
{
	m_isMultiLine = Value;
}

void CGraphicTextInstance::SetHorizonalAlign(int hAlign)
{
	m_hAlign = hAlign;
}

void CGraphicTextInstance::SetVerticalAlign(int vAlign)
{
	m_vAlign = vAlign;
}

void CGraphicTextInstance::SetMax(int iMax)
{
	m_iMax = iMax;
}

void CGraphicTextInstance::SetLimitWidth(float fWidth)
{
	m_fLimitWidth = fWidth;
}

void CGraphicTextInstance::SetValueString(const std::string& c_stValue)
{
	if (0 == m_stText.compare(c_stValue))
		return;

	m_stText = c_stValue;
	m_isUpdate = false;
}

void CGraphicTextInstance::SetValue(const char* c_szText, size_t len)
{
	if (0 == m_stText.compare(c_szText))
		return;

	m_stText = c_szText;
	m_isUpdate = false;
}

void CGraphicTextInstance::SetPosition(float fx, float fy, float fz)
{
	m_v3Position.x = fx;
	m_v3Position.y = fy;
	m_v3Position.z = fz;
}

void CGraphicTextInstance::SetTextPointer(CGraphicText* pText)
{
	m_roText = pText;
}

const std::string& CGraphicTextInstance::GetValueStringReference()
{
	return m_stText;
}

WORD CGraphicTextInstance::GetTextLineCount()
{
	CGraphicFontTexture::TCharacterInfomation* pCurCharInfo;
	CGraphicFontTexture::TPCharacterInfomationVector::iterator itor;

	float fx = 0.0f;
	WORD wLineCount = 1;
	for (itor = m_pCharInfoVector.begin(); itor != m_pCharInfoVector.end(); ++itor)
	{
		pCurCharInfo = *itor;

		float fFontWidth = float(pCurCharInfo->width);
		float fFontAdvance = float(pCurCharInfo->advance);
		//float fFontHeight = float(pCurCharInfo->height);

		// NOTE : 폰트 출력에 Width 제한을 둡니다. - [levites]
		if (fx + fFontWidth > m_fLimitWidth)
		{
			fx = 0.0f;
			++wLineCount;
		}

		fx += fFontAdvance;
	}

#ifdef WJ_MULTI_TEXTLINE
	if (HasEnterToken())
		++wLineCount;
#endif

	return wLineCount;
}

void CGraphicTextInstance::GetTextSize(int* pRetWidth, int* pRetHeight)
{
#ifdef WJ_MULTI_TEXTLINE
	if (HasEnterToken())
	{
		WORD wTextWidth = 0, wMaxTextWidth = 0;
		for (DWORD i = 0; i < m_pCharInfoVector.size(); i++)
		{
			if (m_bEnterTokenVector[i])
			{
				if (wTextWidth >= wMaxTextWidth)
					wMaxTextWidth = wTextWidth;

				wTextWidth = 0;
				continue;
			}
			wTextWidth += m_pCharInfoVector[i]->width;
		}
		*pRetWidth = wMaxTextWidth;
	}
	else
		*pRetWidth = m_textWidth;
	*pRetHeight = m_textHeight;
#else
	* pRetWidth = m_textWidth;
	*pRetHeight = m_textHeight;
#endif
}

int CGraphicTextInstance::PixelPositionToCharacterPosition(int iPixelPosition)
{
	int icurPosition = 0;
	for (int i = 0; i < (int)m_pCharInfoVector.size(); ++i)
	{
		CGraphicFontTexture::TCharacterInfomation* pCurCharInfo = m_pCharInfoVector[i];
		icurPosition += pCurCharInfo->width;

		if (iPixelPosition < icurPosition)
			return i;
	}

	return -1;
}

#if defined(ENABLE_INGAME_WIKI)
DWORD CGraphicTextInstance::GetColor() const
{
	return m_dwTextColor;
}

void CGraphicTextInstance::SetRenderingRect(float fLeft, float fTop, float fRight, float fBottom)
{
	if (m_textWidth == 0 || m_textHeight == 0)
		return;

	m_bUseRenderingRect = true;

	float fWidth = float(m_textWidth);
	float fHeight = float(m_textHeight);

	m_RenderingRect.left = fWidth * fLeft;
	m_RenderingRect.top = fHeight * fTop;
	m_RenderingRect.right = fWidth * fRight;
	m_RenderingRect.bottom = fHeight * fBottom;
}

void CGraphicTextInstance::iSetRenderingRect(int iLeft, int iTop, int iRight, int iBottom)
{
	if (m_textWidth == 0 || m_textHeight == 0)
		return;

	m_bUseRenderingRect = true;

	m_RenderingRect.left = iLeft;
	m_RenderingRect.top = iTop;
	m_RenderingRect.right = iRight;
	m_RenderingRect.bottom = iBottom;
}

void CGraphicTextInstance::SetRenderBox(RECT& renderBox)
{
	memcpy(&m_renderBox, &renderBox, sizeof(m_renderBox));
}
#endif

int CGraphicTextInstance::GetHorizontalAlign()
{
	return m_hAlign;
}

void CGraphicTextInstance::__Initialize()
{
	m_roText = NULL;

	m_hAlign = HORIZONTAL_ALIGN_LEFT;
	m_vAlign = VERTICAL_ALIGN_TOP;

	m_iMax = 0;
	m_fLimitWidth = 1600.0f;

	m_isCursor = false;
	m_isSecret = false;
	m_isMultiLine = false;

	m_isOutline = false;
	m_fFontFeather = c_fFontFeather;

#ifdef WJ_MULTI_TEXTLINE
	m_iLineHeight = 0;
	m_bDisableEnterToken = false;
#endif

	m_isUpdate = false;

	m_textWidth = 0;
	m_textHeight = 0;

	m_v3Position.x = m_v3Position.y = m_v3Position.z = 0.0f;

	m_dwOutLineColor = 0xff000000;
#if defined(ENABLE_INGAME_WIKI)
	memset(&m_RenderingRect, 0, sizeof(RECT));
	m_bUseRenderingRect = false;
	
	memset(&m_renderBox, 0, sizeof(m_renderBox));
	m_startPos = m_endPos = 0;
	m_isFixedRenderPos = false;
#endif
}

void CGraphicTextInstance::Destroy()
{
	m_stText = "";
	m_pCharInfoVector.clear();
	m_dwColorInfoVector.clear();
	m_hyperlinkVector.clear();

#ifdef WJ_MULTI_TEXTLINE
	m_bEnterTokenVector.clear();
	m_bDisableEnterToken = false;
	m_wcEndLine = NULL;
#endif

#ifdef ENABLE_TEXT_IMAGE_LINE
	if (m_imageVector.size() != 0)
	{
		for (std::vector<SImage>::iterator itor = m_imageVector.begin(); itor != m_imageVector.end(); ++itor)
		{
			SImage& rImg = *itor;
			if (rImg.pInstance)
			{
				CGraphicImageInstance::Delete(rImg.pInstance);
				rImg.pInstance = NULL;
			}
		}
	}
	m_imageVector.clear();
#endif

	__Initialize();
}

CGraphicTextInstance::CGraphicTextInstance()
{
	__Initialize();
}

CGraphicTextInstance::~CGraphicTextInstance()
{
	Destroy();
}
