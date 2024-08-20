#include "StdAfx.h"
#include <io.h>
#include "../EterBase/CRC32.h"
#include "../EterBase/Stl.h"
#include "../EterBase/Timer.h"
#include "../EterPack/EterPackManager.h"

#include "ResourceManager.h"
#include "GrpImage.h"

int g_iLoadingDelayTime = 1;

const long c_Deleting_Wait_Time = 30000;
const long c_DeletingCountPerFrame = 30;
const long c_Reference_Decrease_Wait_Time = 30000;

CFileLoaderThread CResourceManager::ms_loadingThread;

void CResourceManager::LoadStaticCache(const char* c_szFileName)
{
	auto pkRes = GetResourcePointer(c_szFileName);
	if (!pkRes)
	{
		Lognf(1, "CResourceManager::LoadStaticCache %s - FAILED", c_szFileName);
		return;
	}

	const auto& dwCacheKey = GetCRC32(c_szFileName, strlen(c_szFileName));
	if (m_pCacheMap.end() != m_pCacheMap.find(dwCacheKey))
		return;

	pkRes->AddReference();
	m_pCacheMap.insert(TResourcePointerMap::value_type(dwCacheKey, pkRes));
}

void CResourceManager::LoadMotionFile(const std::string& stMotionFileName)
{
	if (stMotionFileName.empty())
		return;

	const char* c_pszMotionFileName = nullptr;
	const auto& dwFileCRC = __GetFileCRC(stMotionFileName.c_str(), &c_pszMotionFileName);

	const auto& dwMotionCacheKey = GetCRC32(stMotionFileName.c_str(), strlen(stMotionFileName.c_str()));
	if (m_pCacheMap.end() != m_pCacheMap.find(dwMotionCacheKey))
		return;

	auto pkResourcePtr = FindResourcePointer(dwFileCRC);
	if (pkResourcePtr)
	{
		pkResourcePtr->AddReference();
		m_pCacheMap.insert(TResourcePointerMap::value_type(dwMotionCacheKey, pkResourcePtr));
	}
	else
	{
		CResource* (*pResource) (const char*) = nullptr;
		const auto& c_pszExt = strrchr(c_pszMotionFileName, '.');
		if (c_pszExt)
		{
			static char s_szFileExt[8 + 1];
			strncpy(s_szFileExt, c_pszExt + 1, 8);

			const auto& f = m_pResNewFuncMap.find(s_szFileExt);
			if (m_pResNewFuncMap.end() != f)
				pResource = f->second;
		}

		if (pResource)
		{
			pkResourcePtr = InsertResourcePointer(dwFileCRC, pResource(c_pszMotionFileName));
			pkResourcePtr->AddReference();
			m_pCacheMap.insert(TResourcePointerMap::value_type(dwMotionCacheKey, pkResourcePtr));
		}
	}
}

void CResourceManager::ProcessBackgroundLoading()
{
	auto itor = m_RequestMap.begin();
	while (itor != m_RequestMap.end())
	{
		const auto& dwFileCRC = itor->first;
		auto& stFileName = itor->second;

		if (isResourcePointerData(dwFileCRC) || (m_WaitingMap.end() != m_WaitingMap.find(dwFileCRC)))
		{
			itor = m_RequestMap.erase(itor);
			continue;
		}

		ms_loadingThread.Request(stFileName);
		m_WaitingMap.insert(TResourceRequestMap::value_type(dwFileCRC, stFileName));
		itor = m_RequestMap.erase(itor);
	}

	const auto& dwCurrentTime = ELTimer_GetMSec();

	CFileLoaderThread::TData* pData;
	while (ms_loadingThread.Fetch(&pData))
	{
		auto pResource = GetResourcePointer(pData->stFileName.c_str());
		if (pResource)
		{
			if (pResource->IsEmpty())
			{
				pResource->OnLoad(pData->dwSize, pData->pvBuf);
				pResource->AddReferenceOnly();

				m_pResRefDecreaseWaitingMap.insert(TResourceRefDecreaseWaitingMap::value_type(dwCurrentTime, pResource));
			}
		}

		m_WaitingMap.erase(GetCRC32(pData->stFileName.c_str(), pData->stFileName.size()));

		delete[] static_cast<char*>(pData->pvBuf);
		delete pData;
	}

	const auto lCurrentTime = ELTimer_GetMSec();

	auto itorRef = m_pResRefDecreaseWaitingMap.begin();
	while (itorRef != m_pResRefDecreaseWaitingMap.end())
	{
		const auto& rCreatingTime = itorRef->first;
		if (lCurrentTime - rCreatingTime > c_Reference_Decrease_Wait_Time)
		{
			itorRef->second->Release();
			itorRef = m_pResRefDecreaseWaitingMap.erase(itorRef);
		}
		else
			++itorRef;
	}
}

void CResourceManager::PushBackgroundLoadingSet(const std::set<std::string>& LoadingSet)
{
	for (const auto& itor : LoadingSet)
	{
		const auto& dwFileCRC = __GetFileCRC(itor.c_str());
		if (isResourcePointerData(dwFileCRC))
			continue;

		m_RequestMap.insert(TResourceRequestMap::value_type(dwFileCRC, itor.c_str()));
	}
}

void CResourceManager::__DestroyCacheMap()
{
	for (auto& i : m_pCacheMap)
		i.second->Release();

	m_pCacheMap.clear();
}

void CResourceManager::__DestroyDeletingResourceMap()
{
	Tracenf("CResourceManager::__DestroyDeletingResourceMap %d", m_ResourceDeletingMap.size());
	for (auto& i : m_ResourceDeletingMap)
		(i.first)->Clear();

	m_ResourceDeletingMap.clear();
}

void CResourceManager::__DestroyResourceMap()
{
	Tracenf("CResourceManager::__DestroyResourceMap %d", m_pResMap.size());
	for (auto i : m_pResMap)
		i.second->Clear();

	stl_wipe_second(m_pResMap);
}

void CResourceManager::DestroyDeletingList()
{
	CResource::SetDeleteImmediately(true);

	__DestroyCacheMap();
	__DestroyDeletingResourceMap();
}

void CResourceManager::Destroy()
{
	assert(m_ResourceDeletingMap.empty() && "CResourceManager::Destroy - YOU MUST CALL DestroyDeletingList");
	__DestroyResourceMap();
}

void CResourceManager::RegisterResourceNewFunctionPointer(const char* c_szFileExt, CResource* (*pNewFunc)(const char* c_szFileName))
{
	m_pResNewFuncMap[c_szFileExt] = pNewFunc;
}

void CResourceManager::RegisterResourceNewFunctionByTypePointer(const int iType, CResource* (*pNewFunc) (const char* c_szFileName))
{
	assert(iType >= 0);
	m_pResNewFuncByTypeMap[iType] = pNewFunc;
}

CResource* CResourceManager::InsertResourcePointer(const DWORD dwFileCRC, CResource* pResource)
{
	const auto& itor = m_pResMap.find(dwFileCRC);
	if (m_pResMap.end() != itor)
	{
		TraceError("CResource::InsertResourcePointer: %s is already registered\n", pResource->GetFileName());
		assert(!"CResource::InsertResourcePointer: Resource already resistered");
		delete pResource;
		return itor->second;
	}

	m_pResMap.insert(TResourcePointerMap::value_type(dwFileCRC, pResource));
	return pResource;
}

int __ConvertPathName(const char* c_szPathName, char* pszRetPathName, int retLen)
{
	int len = 0;
	for (auto pc = c_szPathName; *pc && len < retLen; ++pc, ++len)
	{
		if (*pc == '/')
			*(pszRetPathName++) = '\\';
		else
			*(pszRetPathName++) = static_cast<char>(korean_tolower(*pc));
	}

	*pszRetPathName = '\0';
	return len;
}

CResource* CResourceManager::GetTypeResourcePointer(const char* c_szFileName, const int iType)
{
	if (!c_szFileName || !*c_szFileName)
	{
		assert(c_szFileName != nullptr && *c_szFileName != '\0');
		return nullptr;
	}

	const char* c_pszFile = nullptr;
	const auto& dwFileCRC = __GetFileCRC(c_szFileName, &c_pszFile);

	const auto& pResource = FindResourcePointer(dwFileCRC);
	if (pResource)
		return pResource;

	CResource* (*newFunc) (const char*) = nullptr;
	if (iType != -1)
	{
		const auto& f = m_pResNewFuncByTypeMap.find(iType);
		if (m_pResNewFuncByTypeMap.end() != f)
			newFunc = f->second;
	}
	else
	{
		const char* pcFileExt = strrchr(c_pszFile, '.');
		if (pcFileExt)
		{
			static char s_szFileExt[8 + 1];
			strncpy(s_szFileExt, pcFileExt + 1, 8);

			const auto& f = m_pResNewFuncMap.find(s_szFileExt);
			if (m_pResNewFuncMap.end() != f)
				newFunc = f->second;
		}
	}

	if (!newFunc)
	{
		TraceError("ResourceManager::GetResourcePointer: NOT SUPPORT FILE %s", c_pszFile);
		return nullptr;
	}

	return InsertResourcePointer(dwFileCRC, newFunc(c_pszFile));
}

CResource* CResourceManager::GetResourcePointer(const char* c_szFileName)
{
	if (!c_szFileName || !*c_szFileName)
	{
		TraceError("CResourceManager::GetResourcePointer: filename error!");
		return nullptr;
	}

	const char* c_pszFile = nullptr;
	const auto& dwFileCRC = __GetFileCRC(c_szFileName, &c_pszFile);

	auto pResource = FindResourcePointer(dwFileCRC);
	if (pResource)
		return pResource;

	const auto pcFileExt = strrchr(c_pszFile, '.');

#ifdef _DEBUG
#if defined(ENABLE_FOXFS)
	if (!IsFileExist(c_szFileName, __FUNCTION__))
#else
	if (!IsFileExist(c_szFileName))
#endif
	{
		if (pcFileExt == nullptr || (stricmp(pcFileExt, ".fnt") != 0)) {
			TraceError("CResourceManager::GetResourcePointer: File not exist %s", c_szFileName);
		}
	}
#endif

	CResource* (*newFunc) (const char*) = nullptr;
	if (pcFileExt)
	{
		static char s_szFileExt[8 + 1];
		strncpy(s_szFileExt, pcFileExt + 1, 8);

		const auto& f = m_pResNewFuncMap.find(s_szFileExt);
		if (m_pResNewFuncMap.end() != f)
			newFunc = f->second;
	}

	if (!newFunc)
	{
		TraceError("ResourceManager::GetResourcePointer: NOT SUPPORT FILE %s", c_pszFile);
		return nullptr;
	}

	pResource = InsertResourcePointer(dwFileCRC, newFunc(c_pszFile));
	return pResource;
}

CResource* CResourceManager::FindResourcePointer(const DWORD dwFileCRC)
{
	const auto& itor = m_pResMap.find(dwFileCRC);
	if (m_pResMap.end() == itor)
		return nullptr;

	return itor->second;
}

bool CResourceManager::isResourcePointerData(const DWORD dwFileCRC)
{
	const auto& itor = m_pResMap.find(dwFileCRC);
	if (m_pResMap.end() == itor)
		return false;

	return (itor->second)->IsData();
}

DWORD CResourceManager::__GetFileCRC(const char* c_szFileName, const char** c_ppszLowerFileName)
{
	static char s_szFullPathFileName[MAX_PATH];
	const auto src = c_szFileName;
	auto dst = s_szFullPathFileName;
	auto len = 0;

	while (src[len])
	{
		if (src[len] == '/')
			dst[len] = '\\';
		else
			dst[len] = static_cast<char>(korean_tolower(src[len]));

		++len;
	}

	dst[len] = '\0';

	if (c_ppszLowerFileName)
		*c_ppszLowerFileName = &s_szFullPathFileName[0];

	return (GetCRC32(s_szFullPathFileName, len));
}

typedef struct SDumpData
{
	const char* filename;
	float			KB;
	DWORD			cost;
} TDumpData;

bool DumpKBCompare(const TDumpData& lhs, const TDumpData& rhs)
{
	return (lhs.KB > rhs.KB);
}

bool DumpCostCompare(const TDumpData& lhs, const TDumpData& rhs)
{
	return (lhs.cost > rhs.cost);
}

struct FDumpPrint
{
	FILE* m_fp;
	static float m_totalKB;

	void operator () (const TDumpData& data) const
	{
		m_totalKB += data.KB;
		fprintf(m_fp, "%6.1f %s\n", data.KB, data.filename);
	}
};

float FDumpPrint::m_totalKB;

struct FDumpCostPrint
{
	FILE* m_fp;
	void operator() (const TDumpData& data) const
	{
		fprintf(m_fp, "%-4d %s\n", data.cost, data.filename);
	}
};

void CResourceManager::DumpFileListToTextFile(const char* c_szFileName)
{
	std::vector<TDumpData> dumpVector;
	for (auto& i : m_pResMap)
	{
		const auto& pResource = i.second;
		if (pResource->IsEmpty())
			continue;

		TDumpData data;
		data.filename = pResource->GetFileName();

		int iFileSize;
		const char* ext = strrchr(data.filename, '.');
		if (pResource->IsType(CGraphicImage::Type()) && strnicmp(ext, ".sub", 4))
			iFileSize = dynamic_cast<CGraphicImage*>(pResource)->GetWidth() * dynamic_cast<CGraphicImage*>(pResource)->GetHeight() * 4;
		else
		{
			const auto fp2 = fopen(data.filename, "rb");
			if (fp2)
			{
				fseek(fp2, 0L, SEEK_END);
				iFileSize = ftell(fp2);
				fclose(fp2);
			}
			else
				iFileSize = 0;
		}

		data.KB = static_cast<float>(iFileSize) / static_cast<float>(1024);
		data.cost = pResource->GetLoadCostMilliSecond();

		dumpVector.push_back(data);
	}

	const auto fp = fopen(c_szFileName, "w");
	if (fp)
	{
		std::sort(dumpVector.begin(), dumpVector.end(), DumpKBCompare);

		FDumpPrint DumpPrint{};
		DumpPrint.m_fp = fp;
		FDumpPrint::m_totalKB = 0;

		std::for_each(dumpVector.begin(), dumpVector.end(), DumpPrint);
		fprintf(fp, "total: %.2fmb", FDumpPrint::m_totalKB / 1024.0f);

		FDumpCostPrint DumpCostPrint{};
		DumpCostPrint.m_fp = fp;

		std::sort(dumpVector.begin(), dumpVector.end(), DumpCostCompare);
		std::for_each(dumpVector.begin(), dumpVector.end(), DumpCostPrint);
		fprintf(fp, "total: %.2fmb", FDumpPrint::m_totalKB / 1024.0f);

		fclose(fp);
	}
}

bool CResourceManager::IsFileExist(const char* c_szFileName
#if defined(ENABLE_FOXFS)
, const char* func
#endif
)
{
#if defined(ENABLE_FOXFS)
	return CFileSystem::Instance().isExist(c_szFileName, func, false);
#else
	return CEterPackManager::Instance().isExist(c_szFileName);
#endif
}

void CResourceManager::Update()
{
	const auto& dwCurrentTime = ELTimer_GetMSec();
	auto iCount = 0;

	auto itor = m_ResourceDeletingMap.begin();
	while (itor != m_ResourceDeletingMap.end())
	{
		const auto& pResource = itor->first;
		if (dwCurrentTime >= itor->second)
		{
			if (pResource->canDestroy())
				pResource->Clear();

			itor = m_ResourceDeletingMap.erase(itor);
			if (++iCount >= c_DeletingCountPerFrame)
				break;
		}
		else
			++itor;
	}

	ProcessBackgroundLoading();
}

void CResourceManager::ReserveDeletingResource(CResource* pResource)
{
	m_ResourceDeletingMap.insert(TResourceDeletingMap::value_type(pResource, ELTimer_GetMSec() + c_Deleting_Wait_Time));
}

void CResourceManager::BeginThreadLoading()
{
}

void CResourceManager::EndThreadLoading()
{
}

CResourceManager::CResourceManager()
= default;

CResourceManager::~CResourceManager()
{
	Destroy();
}
