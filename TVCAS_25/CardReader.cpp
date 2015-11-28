#include "stdafx.h"
#include "CardReader.h"
#include "StdUtil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif




CCardReader::CCardReader()
{
}


CCardReader::~CCardReader()
{
}


LPCTSTR CCardReader::EnumReader(int Index) const
{
	if (Index != 0)
		return NULL;
	return GetReaderName();
}


CCardReader *CCardReader::CreateCardReader(ReaderType Type)
{
	CCardReader *pReader;

	switch (Type) {
	case READER_SCARD:
		pReader = new CSCardReader;
		break;

#ifdef CARDREADER_SCARD_DYNAMIC_SUPPORT
	case READER_SCARD_DYNAMIC:
		pReader = new CDynamicSCardReader;
		break;
#endif

#ifdef CARDREADER_BONCASCLIENT_SUPPORT
	case READER_BONCASCLIENT:
		pReader = new CBonCasClientCardReader;
		break;
#endif

	default:
		return NULL;
	}

	pReader->m_ReaderType = Type;

	return pReader;
}




///////////////////////////////////////////////////////////////////////////////
//
// 標準スマートカードリーダー
//
///////////////////////////////////////////////////////////////////////////////


#pragma comment(lib, "WinScard.lib")


static LPCTSTR GetSCardErrorText(LONG Code)
{
	switch (Code) {
	case ERROR_BROKEN_PIPE:
		return TEXT("ERROR_BROKEN_PIPE: The client attempted a smart card operation in a remote session.");
	case SCARD_F_INTERNAL_ERROR:
		return TEXT("SCARD_F_INTERNAL_ERROR: An internal consistency check failed.");
	case SCARD_E_CANCELLED:
		return TEXT("SCARD_E_CANCELLED: The action was cancelled by an SCardCancel request.");
	case SCARD_E_INVALID_HANDLE:
		return TEXT("SCARD_E_INVALID_HANDLE: The supplied handle was invalid.");
	case SCARD_E_INVALID_PARAMETER:
		return TEXT("SCARD_E_INVALID_PARAMETER: One or more of the supplied parameters could not be properly interpreted.");
	case SCARD_E_INVALID_TARGET:
		return TEXT("SCARD_E_INVALID_TARGET: Registry startup information is missing or invalid.");
	case SCARD_E_NO_MEMORY:
		return TEXT("SCARD_E_NO_MEMORY: Not enough memory available to complete this command.");
	case SCARD_F_WAITED_TOO_LONG:
		return TEXT("SCARD_F_WAITED_TOO_LONG: An internal consistency timer has expired.");
	case SCARD_E_INSUFFICIENT_BUFFER:
		return TEXT("SCARD_E_INSUFFICIENT_BUFFER: The data buffer to receive returned data is too small for the returned data.");
	case SCARD_E_UNKNOWN_READER:
		return TEXT("SCARD_E_UNKNOWN_READER: The specified reader name is not recognized.");
	case SCARD_E_TIMEOUT:
		return TEXT("SCARD_E_TIMEOUT: The user-specified timeout value has expired.");
	case SCARD_E_SHARING_VIOLATION:
		return TEXT("SCARD_E_SHARING_VIOLATION: The smart card cannot be accessed because of other connections outstanding.");
	case SCARD_E_NO_SMARTCARD:
		return TEXT("SCARD_E_NO_SMARTCARD: The operation requires a Smart Card, but no Smart Card is currently in the device.");
	case SCARD_E_UNKNOWN_CARD:
		return TEXT("SCARD_E_UNKNOWN_CARD: The specified smart card name is not recognized.");
	case SCARD_E_CANT_DISPOSE:
		return TEXT("SCARD_E_CANT_DISPOSE: The system could not dispose of the media in the requested manner.");
	case SCARD_E_PROTO_MISMATCH:
		return TEXT("SCARD_E_PROTO_MISMATCH: The requested protocols are incompatible with the protocol currently in use with the smart card.");
	case SCARD_E_NOT_READY:
		return TEXT("SCARD_E_NOT_READY: The reader or smart card is not ready to accept commands.");
	case SCARD_E_INVALID_VALUE:
		return TEXT("SCARD_E_INVALID_VALUE: One or more of the supplied parameters values could not be properly interpreted.");
	case SCARD_E_SYSTEM_CANCELLED:
		return TEXT("SCARD_E_SYSTEM_CANCELLED: The action was cancelled by the system, presumably to log off or shut down.");
	case SCARD_F_COMM_ERROR:
		return TEXT("SCARD_F_COMM_ERROR: An internal communications error has been detected.");
	case SCARD_F_UNKNOWN_ERROR:
		return TEXT("SCARD_F_UNKNOWN_ERROR: An internal error has been detected, but the source is unknown.");
	case SCARD_E_INVALID_ATR:
		return TEXT("SCARD_E_INVALID_ATR: An ATR obtained from the registry is not a valid ATR string.");
	case SCARD_E_NOT_TRANSACTED:
		return TEXT("SCARD_E_NOT_TRANSACTED: An attempt was made to end a non-existent transaction.");
	case SCARD_E_READER_UNAVAILABLE:
		return TEXT("SCARD_E_READER_UNAVAILABLE: The specified reader is not currently available for use.");
	case SCARD_P_SHUTDOWN:
		return TEXT("SCARD_P_SHUTDOWN: The operation has been aborted to allow the server application to exit.");
	case SCARD_E_PCI_TOO_SMALL:
		return TEXT("SCARD_E_PCI_TOO_SMALL: The PCI Receive buffer was too small.");
	case SCARD_E_READER_UNSUPPORTED:
		return TEXT("SCARD_E_READER_UNSUPPORTED: The reader driver does not meet minimal requirements for support.");
	case SCARD_E_DUPLICATE_READER:
		return TEXT("SCARD_E_DUPLICATE_READER: The reader driver did not produce a unique reader name.");
	case SCARD_E_CARD_UNSUPPORTED:
		return TEXT("SCARD_E_CARD_UNSUPPORTED: The smart card does not meet minimal requirements for support.");
	case SCARD_E_NO_SERVICE:
		return TEXT("SCARD_E_NO_SERVICE: The Smart card resource manager is not running.");
	case SCARD_E_SERVICE_STOPPED:
		return TEXT("SCARD_E_SERVICE_STOPPED: The Smart card resource manager has shut down.");
	case SCARD_E_UNEXPECTED:
		return TEXT("SCARD_E_UNEXPECTED: An unexpected card error has occurred.");
	case SCARD_E_ICC_INSTALLATION:
		return TEXT("SCARD_E_ICC_INSTALLATION: No Primary Provider can be found for the smart card.");
	case SCARD_E_ICC_CREATEORDER:
		return TEXT("SCARD_E_ICC_CREATEORDER: The requested order of object creation is not supported.");
	case SCARD_E_UNSUPPORTED_FEATURE:
		return TEXT("SCARD_E_UNSUPPORTED_FEATURE: This smart card does not support the requested feature.");
	case SCARD_E_DIR_NOT_FOUND:
		return TEXT("SCARD_E_DIR_NOT_FOUND: The identified directory does not exist in the smart card.");
	case SCARD_E_FILE_NOT_FOUND:
		return TEXT("SCARD_E_FILE_NOT_FOUND: The identified file does not exist in the smart card.");
	case SCARD_E_NO_DIR:
		return TEXT("SCARD_E_NO_DIR: The supplied path does not represent a smart card directory.");
	case SCARD_E_NO_FILE:
		return TEXT("SCARD_E_NO_FILE: The supplied path does not represent a smart card file.");
	case SCARD_E_NO_ACCESS:
		return TEXT("SCARD_E_NO_ACCESS: Access is denied to this file.");
	case SCARD_E_WRITE_TOO_MANY:
		return TEXT("SCARD_E_WRITE_TOO_MANY: The smartcard does not have enough memory to store the information.");
	case SCARD_E_BAD_SEEK:
		return TEXT("SCARD_E_BAD_SEEK: There was an error trying to set the smart card file object pointer.");
	case SCARD_E_INVALID_CHV:
		return TEXT("SCARD_E_INVALID_CHV: The supplied PIN is incorrect.");
	case SCARD_E_UNKNOWN_RES_MNG:
		return TEXT("SCARD_E_UNKNOWN_RES_MNG: An unrecognized error code was returned from a layered component.");
	case SCARD_E_NO_SUCH_CERTIFICATE:
		return TEXT("SCARD_E_NO_SUCH_CERTIFICATE: The requested certificate does not exist.");
	case SCARD_E_CERTIFICATE_UNAVAILABLE:
		return TEXT("SCARD_E_CERTIFICATE_UNAVAILABLE: The requested certificate could not be obtained.");
	case SCARD_E_NO_READERS_AVAILABLE:
		return TEXT("SCARD_E_NO_READERS_AVAILABLE: Cannot find a smart card reader.");
	case SCARD_E_COMM_DATA_LOST:
		return TEXT("SCARD_E_COMM_DATA_LOST: A communications error with the smart card has been detected.  Retry the operation.");
	case SCARD_E_NO_KEY_CONTAINER:
		return TEXT("SCARD_E_NO_KEY_CONTAINER: The requested key container does not exist on the smart card.");
	case SCARD_E_SERVER_TOO_BUSY:
		return TEXT("SCARD_E_SERVER_TOO_BUSY: The Smart card resource manager is too busy to complete this operation.");
	case SCARD_W_UNSUPPORTED_CARD:
		return TEXT("SCARD_W_UNSUPPORTED_CARD: The reader cannot communicate with the smart card, due to ATR configuration conflicts.");
	case SCARD_W_UNRESPONSIVE_CARD:
		return TEXT("SCARD_W_UNRESPONSIVE_CARD: The smart card is not responding to a reset.");
	case SCARD_W_UNPOWERED_CARD:
		return TEXT("SCARD_W_UNPOWERED_CARD: Power has been removed from the smart card, so that further communication is not possible.");
	case SCARD_W_RESET_CARD:
		return TEXT("SCARD_W_RESET_CARD: The smart card has been reset, so any shared state information is invalid.");
	case SCARD_W_REMOVED_CARD:
		return TEXT("SCARD_W_REMOVED_CARD: The smart card has been removed, so that further communication is not possible.");
	case SCARD_W_SECURITY_VIOLATION:
		return TEXT("SCARD_W_SECURITY_VIOLATION: Access was denied because of a security violation.");
	case SCARD_W_WRONG_CHV:
		return TEXT("SCARD_W_WRONG_CHV: The card cannot be accessed because the wrong PIN was presented.");
	case SCARD_W_CHV_BLOCKED:
		return TEXT("SCARD_W_CHV_BLOCKED: The card cannot be accessed because the maximum number of PIN entry attempts has been reached.");
	case SCARD_W_EOF:
		return TEXT("SCARD_W_EOF: The end of the smart card file has been reached.");
	case SCARD_W_CANCELLED_BY_USER:
		return TEXT("SCARD_W_CANCELLED_BY_USER: The action was cancelled by the user.");
	case SCARD_W_CARD_NOT_AUTHENTICATED:
		return TEXT("SCARD_W_CARD_NOT_AUTHENTICATED: No PIN was presented to the smart card.");
	case SCARD_W_CACHE_ITEM_NOT_FOUND:
		return TEXT("SCARD_W_CACHE_ITEM_NOT_FOUND: The requested item could not be found in the cache.");
	case SCARD_W_CACHE_ITEM_STALE:
		return TEXT("SCARD_W_CACHE_ITEM_STALE: The requested cache item is too old and was deleted from the cache.");
	case SCARD_W_CACHE_ITEM_TOO_BIG:
		return TEXT("SCARD_W_CACHE_ITEM_TOO_BIG: The new cache item exceeds the maximum per-item size defined for the cache.");
	}
	return NULL;
}

static DWORD GetSCardErrorMessage(LONG Code,LPTSTR pszMessage,DWORD MaxLength)
{
	LPCTSTR pszText = GetSCardErrorText(Code);
	DWORD Length = 0;
	if (pszText != NULL) {
		Length = (DWORD)(::StrStr(pszText, TEXT(" ")) - pszText + 1);
		if (Length > MaxLength)
			Length = MaxLength;
		::lstrcpyn(pszMessage, pszText, Length + 1);
	}
	Length = ::FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		Code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pszMessage + Length, MaxLength - Length, NULL);
	if (Length == 0) {
		if (pszText != NULL)
			::lstrcpyn(pszMessage, pszText, MaxLength);
		else
			pszMessage[0] = '\0';
	}
	return Length;
}

static bool CheckReaderList(LPCTSTR pReaderList, DWORD Length)
{
	if (Length < 2)
		return false;
	return pReaderList[Length - 2] == _T('\0')
		&& pReaderList[Length - 1] == _T('\0');
}


// サービスAPI用
#pragma comment(lib, "advapi32.lib")

// スマートカードサービスが有効か調べる
enum SCardCheckResult {
	SCARD_CHECK_ENABLED,
	SCARD_CHECK_DISABLED,
	SCARD_CHECK_ERR_SERVICE_NOT_FOUND,
	SCARD_CHECK_ERR_OPEN_MANAGER,
	SCARD_CHECK_ERR_OPEN_SERVICE,
	SCARD_CHECK_ERR_QUERY_CONFIG
};

static SCardCheckResult CheckSmartCardService()
{
	SCardCheckResult Result;

	SC_HANDLE hManager = ::OpenSCManager(NULL, NULL, GENERIC_READ);
	if (hManager == NULL)
		return SCARD_CHECK_ERR_OPEN_MANAGER;

	TCHAR szName[256];
	DWORD Length = _countof(szName);
	if (!::GetServiceKeyName(hManager, TEXT("Smart Card"), szName, &Length))
		::lstrcpy(szName, TEXT("SCardSvr"));

	SC_HANDLE hService = ::OpenService(hManager, szName, SERVICE_QUERY_CONFIG);
	if (hService == NULL) {
		if (::GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
			Result = SCARD_CHECK_ERR_SERVICE_NOT_FOUND;
		else
			Result = SCARD_CHECK_ERR_OPEN_SERVICE;
		::CloseServiceHandle(hManager);
		return Result;
	}

	Result = SCARD_CHECK_ERR_QUERY_CONFIG;
	DWORD Size = 0;
	::QueryServiceConfig(hService, NULL, 0, &Size);
	if (Size > 0) {
		BYTE *pBuffer = new BYTE[Size];
		QUERY_SERVICE_CONFIG *pConfig = (QUERY_SERVICE_CONFIG*)pBuffer;

		if (::QueryServiceConfig(hService, pConfig, Size, &Size)) {
			if (pConfig->dwStartType == SERVICE_DISABLED)
				Result = SCARD_CHECK_DISABLED;
			else
				Result = SCARD_CHECK_ENABLED;
		}
		delete [] pBuffer;
	}

	::CloseServiceHandle(hService);
	::CloseServiceHandle(hManager);

	return Result;
}


CSCardReader::CSCardReader()
	: m_hSCard(0)
	, m_bIsEstablish(false)
	, m_pReaderList(NULL)
	, m_NumReaders(0)
	, m_pszReaderName(NULL)
{
	TRACE(TEXT("SCardEstablishContext\n"));
	if (::SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &m_SCardContext) == SCARD_S_SUCCESS) {
		m_bIsEstablish = true;

		// カードリーダを列挙する
		DWORD dwBuffLength = SCARD_AUTOALLOCATE;

		TRACE(TEXT("SCardListReaders\n"));
		if (::SCardListReaders(m_SCardContext, NULL,
				reinterpret_cast<LPTSTR>(&m_pReaderList), &dwBuffLength) == SCARD_S_SUCCESS) {
			if (CheckReaderList(m_pReaderList, dwBuffLength)) {
				LPCTSTR p = m_pReaderList;
				while (*p) {
					TRACE(TEXT(" Reader%d : %s\n"), m_NumReaders, p);
					m_NumReaders++;
					p += ::lstrlen(p) + 1;
				}
			} else {
				::SCardFreeMemory(m_SCardContext, m_pReaderList);
				m_pReaderList = NULL;
			}
		}
	}
}


CSCardReader::~CSCardReader()
{
	Close();

	if (m_bIsEstablish) {
		if (m_pReaderList)
			::SCardFreeMemory(m_SCardContext, m_pReaderList);

		TRACE(TEXT("SCardReleaseContext\n"));
		::SCardReleaseContext(m_SCardContext);
	}
}


bool CSCardReader::Open(LPCTSTR pszReader)
{
	if (!m_bIsEstablish) {
		SetError(TEXT("コンテキストを確立できません。"),
				 TEXT("Smart Card サービスが有効であるか確認してください。"));
		return false;
	}

	// 一旦クローズする
	Close();

	if (pszReader) {
		// 指定されたカードリーダに対してオープンを試みる
		TRACE(TEXT("Open card reader \"%s\"\n"), pszReader);

		LONG Result;

		SCARD_READERSTATE ReaderState;
		::ZeroMemory(&ReaderState, sizeof(ReaderState));
		ReaderState.szReader = pszReader;

		TRACE(TEXT("SCardGetStatusChange\n"));
		Result = ::SCardGetStatusChange(m_SCardContext, 0, &ReaderState, 1);
		if (Result != SCARD_S_SUCCESS) {
			TCHAR szMessage[256];
			GetSCardErrorMessage(Result, szMessage, _countof(szMessage));
			SetError(TEXT("カードリーダの状態を取得できません。"), NULL, szMessage);
			return false;
		}

		if (!(ReaderState.dwEventState & SCARD_STATE_PRESENT)) {
			TCHAR szMessage[256];
			StdUtil::snprintf(szMessage, _countof(szMessage),
							  TEXT("カードリーダが利用できません。(State %08x)"),
							  ReaderState.dwEventState);
			SetError(szMessage);
			return false;
		}

		DWORD dwActiveProtocol = SCARD_PROTOCOL_UNDEFINED;

		TRACE(TEXT("SCardConnect\n"));
		Result = ::SCardConnect(m_SCardContext, pszReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, &m_hSCard, &dwActiveProtocol);
		if (Result != SCARD_S_SUCCESS) {
			TCHAR szMessage[256];
			GetSCardErrorMessage(Result, szMessage, _countof(szMessage));
			SetError(TEXT("カードリーダに接続できません。"), NULL, szMessage);
			return false;
		}

		if (dwActiveProtocol != SCARD_PROTOCOL_T1) {
			Close();
			SetError(TEXT("アクティブプロトコルが不正です。"));
			return false;
		}

		LPTSTR pszReaderName;
		BYTE Atr[32];
		DWORD dwReaderLen = SCARD_AUTOALLOCATE, dwState, dwProtocol, dwAtrLen = sizeof(Atr);
		TRACE(TEXT("SCardStatus\n"));
		Result = ::SCardStatus(m_hSCard, reinterpret_cast<LPTSTR>(&pszReaderName), &dwReaderLen, &dwState, &dwProtocol, Atr, &dwAtrLen);
		if (Result != SCARD_S_SUCCESS) {
			Close();
			TCHAR szMessage[256];
			GetSCardErrorMessage(Result, szMessage, _countof(szMessage));
			SetError(TEXT("カードの状態を取得できません。"), NULL, szMessage);
			return false;
		}
		TCHAR szAtr[sizeof(Atr) * 3 + 1];
		for (DWORD i = 0; i < dwAtrLen; i++)
			::wsprintf(&szAtr[i * 2], TEXT("%02x "), Atr[i]);
		TRACE(TEXT("\nName : %s\nState : %08x\nProtocol : %u\nATR size : %u\nATR : %s\n"),
			  pszReaderName, dwState, dwProtocol, dwAtrLen, dwAtrLen ? szAtr : TEXT("n/a"));
		::SCardFreeMemory(m_SCardContext, pszReaderName);

		m_pszReaderName = StdUtil::strdup(pszReader);
	} else {
		// 全てのカードリーダに対してオープンを試みる
		if (m_pReaderList == NULL) {
			SetError(TEXT("カードリーダが見付かりません。"));
			return false;
		}

		LPCTSTR p = m_pReaderList;
		while (*p) {
			if (Open(p))
				return true;
			p += ::lstrlen(p) + 1;
		}
		return false;
	}

	ClearError();

	return true;
}


void CSCardReader::Close()
{
	if (m_hSCard) {
		TRACE(TEXT("SCardDisconnect\n"));
		::SCardDisconnect(m_hSCard, SCARD_LEAVE_CARD);
		m_hSCard = 0;
	}

	if (m_pszReaderName) {
		delete [] m_pszReaderName;
		m_pszReaderName = NULL;
	}
}


LPCTSTR CSCardReader::GetReaderName() const
{
	return m_pszReaderName;
}


int CSCardReader::NumReaders() const
{
	return m_NumReaders;
}


LPCTSTR CSCardReader::EnumReader(int Index) const
{
	if (Index < 0 || Index >= m_NumReaders)
		return NULL;
	LPCTSTR p = m_pReaderList;
	for (int i = 0; i < Index; i++)
		p += ::lstrlen(p) + 1;
	return p;
}


bool CSCardReader::IsAvailable()
{
	return m_bIsEstablish
		&& ::SCardIsValidContext(m_SCardContext) == SCARD_S_SUCCESS
		&& m_NumReaders > 0;
}


bool CSCardReader::IsReaderAvailable(LPCTSTR pszReader)
{
	if (!m_bIsEstablish || pszReader == NULL)
		return false;

	LONG Result;

	SCARD_READERSTATE ReaderState;
	::ZeroMemory(&ReaderState, sizeof(ReaderState));
	ReaderState.szReader = pszReader;

	Result = ::SCardGetStatusChange(m_SCardContext, 0, &ReaderState, 1);
	if (Result != SCARD_S_SUCCESS)
		return false;

	if (!(ReaderState.dwEventState & SCARD_STATE_PRESENT))
		return false;

	return true;
}


bool CSCardReader::CheckAvailability(bool *pbAvailable, LPTSTR pszMessage, int MaxLength)
{
	bool bAvailable = false;

	switch (CheckSmartCardService()) {
	case SCARD_CHECK_ENABLED:
		if (pszMessage)
			::lstrcpyn(pszMessage, TEXT("Smart Card サービスは有効です。"), MaxLength);
		bAvailable = true;
		break;

	case SCARD_CHECK_DISABLED:
		if (pszMessage)
			::lstrcpyn(pszMessage, TEXT("Smart Card サービスが無効になっています。"), MaxLength);
		break;

	case SCARD_CHECK_ERR_SERVICE_NOT_FOUND:
		if (pszMessage)
			::lstrcpyn(pszMessage, TEXT("Smart Card サービスがインストールされていません。"), MaxLength);
		break;

	default:
		return false;
	}

	*pbAvailable = bAvailable;

	return true;
}


bool CSCardReader::Transmit(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize)
{
	if (!m_hSCard) {
		SetError(TEXT("カードリーダが開かれていません。"));
		return false;
	}

	LONG Result = ::SCardTransmit(m_hSCard, SCARD_PCI_T1,
		static_cast<LPCBYTE>(pSendData), SendSize, NULL, static_cast<LPBYTE>(pRecvData), pRecvSize);

	if (Result != SCARD_S_SUCCESS) {
		TCHAR szMessage[256];
		GetSCardErrorMessage(Result,szMessage, _countof(szMessage));
		SetError(TEXT("コマンド送信エラーです。"), NULL, szMessage);
		return false;
	}

	ClearError();

	return true;
}




#ifdef CARDREADER_SCARD_DYNAMIC_SUPPORT

///////////////////////////////////////////////////////////////////////////////
//
// winscard.dll互換カードリーダー(動的リンク)
//
///////////////////////////////////////////////////////////////////////////////


#ifdef UNICODE
#define FUNC_NAME(symbol) symbol "W"
#else
#define FUNC_NAME(symbol) symbol "A"
#endif

template<typename T> bool GetLibraryFunc(HMODULE hLib, T &Func, const char *pSymbol)
{
	return (Func = reinterpret_cast<T>(::GetProcAddress(hLib, pSymbol))) != NULL;
}


CDynamicSCardReader::CDynamicSCardReader()
	: m_hLib(NULL)
	, m_hSCard(0)
	, m_pReaderList(NULL)
	, m_pszReaderName(NULL)

	, m_pSCardReleaseContext(NULL)
	, m_pSCardConnect(NULL)
	, m_pSCardDisconnect(NULL)
	, m_pSCardTransmit(NULL)
{
}


CDynamicSCardReader::~CDynamicSCardReader()
{
	Unload();
}


bool CDynamicSCardReader::Load(LPCTSTR pszFileName)
{
	m_hLib = ::LoadLibrary(pszFileName);
	if (m_hLib == NULL) {
		SetError(TEXT("ライブラリをロードできません。"));
		return false;
	}

	typedef LONG (WINAPI *SCardEstablishContextFunc)(DWORD, LPCVOID, LPCVOID, LPSCARDCONTEXT);
	typedef LONG (WINAPI *SCardListReadersFunc)(SCARDCONTEXT, LPCTSTR, LPTSTR, LPDWORD);
	SCardEstablishContextFunc pEstablishContext;
	SCardListReadersFunc pListReaders;

	if (!GetLibraryFunc(m_hLib, pEstablishContext, "SCardEstablishContext")
	 || !GetLibraryFunc(m_hLib, pListReaders, FUNC_NAME("SCardListReaders"))
	 || !GetLibraryFunc(m_hLib, m_pSCardReleaseContext, "SCardReleaseContext")
	 || !GetLibraryFunc(m_hLib, m_pSCardConnect, FUNC_NAME("SCardConnect"))
	 || !GetLibraryFunc(m_hLib, m_pSCardDisconnect, "SCardDisconnect")
	 || !GetLibraryFunc(m_hLib, m_pSCardTransmit, "SCardTransmit")) {
		::FreeLibrary(m_hLib);
		m_hLib = NULL;
		SetError(TEXT("関数のアドレスを取得できません。"));
		return false;
	}

	if (pEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &m_SCardContext) != SCARD_S_SUCCESS) {
		::FreeLibrary(m_hLib);
		m_hLib = NULL;
		SetError(TEXT("コンテキストを確立できません。"));
		return false;
	}

	// カードリーダを列挙する
	DWORD dwBuffLength = 0;

	if (pListReaders(m_SCardContext, NULL, NULL, &dwBuffLength) == SCARD_S_SUCCESS) {
		m_pReaderList = new TCHAR[dwBuffLength];
		if (pListReaders(m_SCardContext, NULL, m_pReaderList, &dwBuffLength) == SCARD_S_SUCCESS) {
			if (!CheckReaderList(m_pReaderList, dwBuffLength)) {
				Unload();
				SetError(TEXT("カードリーダのリストが不正です。"));
				return false;
			}
		} else {
			delete [] m_pReaderList;
			m_pReaderList = NULL;
		}
	}

	return true;
}


void CDynamicSCardReader::Unload()
{
	Close();

	if (m_hLib) {
		m_pSCardReleaseContext(m_SCardContext);

		::FreeLibrary(m_hLib);
		m_hLib = NULL;
	}

	if (m_pReaderList) {
		delete [] m_pReaderList;
		m_pReaderList = NULL;
	}
}


// "ファイル名|リーダ名" の形式で指定する
bool CDynamicSCardReader::Open(LPCTSTR pszReader)
{
	if (pszReader == NULL) {
		SetError(TEXT("ファイル名が指定されていません。"));
		return false;
	}

	LPCTSTR pszReaderName = ::StrChr(pszReader, _T('|'));
	if (pszReaderName != NULL) {
		pszReaderName++;
		if (!*pszReaderName)
			pszReaderName = NULL;
	}
	TCHAR szFileName[MAX_PATH];
	if (pszReaderName != NULL) {
		size_t FileNameLength = pszReaderName - pszReader;
		if (FileNameLength > _countof(szFileName)) {
			SetError(TEXT("ファイル名が長過ぎます。"));
			return false;
		}
		::lstrcpyn(szFileName, pszReader, (int)FileNameLength);
	} else {
		if (::lstrlen(pszReader) >= _countof(szFileName)) {
			SetError(TEXT("ファイル名が長過ぎます。"));
			return false;
		}
		::lstrcpy(szFileName, pszReader);
	}
	if (m_hLib == NULL) {
		if (!Load(szFileName))
			return false;
	} else {
		// 一旦クローズする
		Close();
	}

	if (pszReaderName) {
		// 指定されたカードリーダに対してオープンを試みる
		LONG Result;
		DWORD dwActiveProtocol = SCARD_PROTOCOL_UNDEFINED;

		Result=m_pSCardConnect(m_SCardContext, pszReaderName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, &m_hSCard, &dwActiveProtocol);
		if (Result != SCARD_S_SUCCESS) {
			TCHAR szMessage[256];
			GetSCardErrorMessage(Result, szMessage, _countof(szMessage));
			SetError(TEXT("カードリーダに接続できません。"), NULL, szMessage);
			return false;
		}

		if (dwActiveProtocol != SCARD_PROTOCOL_T1) {
			Close();
			SetError(TEXT("アクティブプロトコルが不正です。"));
			return false;
		}

		m_pszReaderName = StdUtil::strdup(pszReader);
	} else {
		// 全てのカードリーダに対してオープンを試みる
		if (m_pReaderList == NULL || m_pReaderList[0] == _T('\0')) {
			SetError(TEXT("カードリーダが見付かりません。"));
			return false;
		}

		LPCTSTR p = m_pReaderList;
		while (*p) {
			TCHAR szReader[MAX_PATH + 256];

			StdUtil::snprintf(szReader, _countof(szReader), TEXT("%s|%s"), szFileName, p);
			if (Open(szReader))
				return true;
			p += ::lstrlen(p) + 1;
		}
		return false;
	}

	ClearError();

	return true;
}


void CDynamicSCardReader::Close()
{
	if (m_hSCard) {
		m_pSCardDisconnect(m_hSCard, SCARD_LEAVE_CARD);
		m_hSCard = 0;
	}

	if (m_pszReaderName) {
		delete [] m_pszReaderName;
		m_pszReaderName = NULL;
	}
}


LPCTSTR CDynamicSCardReader::GetReaderName() const
{
	return m_pszReaderName;
}


int CDynamicSCardReader::NumReaders() const
{
	if (!m_pReaderList)
		return 0;
	LPCTSTR p = m_pReaderList;
	int i;
	for (i = 0; *p; i++) {
		p += ::lstrlen(p) + 1;
	}
	return i;
}


LPCTSTR CDynamicSCardReader::EnumReader(int Index) const
{
	if (!m_pReaderList)
		return NULL;
	LPCTSTR p = m_pReaderList;
	for (int i = 0; i < Index; i++) {
		if (!*p)
			return NULL;
		p += ::lstrlen(p) + 1;
	}
	return p;
}


bool CDynamicSCardReader::IsAvailable()
{
	return true;
}


bool CDynamicSCardReader::IsReaderAvailable(LPCTSTR pszReader)
{
	return true;
}


bool CDynamicSCardReader::CheckAvailability(bool *pbAvailable, LPTSTR pszMessage, int MaxLength)
{
	return false;
}


bool CDynamicSCardReader::Transmit(const void *pSendData,DWORD SendSize,void *pRecvData,DWORD *pRecvSize)
{
	if (!m_hSCard) {
		SetError(TEXT("カードリーダが開かれていません。"));
		return false;
	}

	LONG Result = m_pSCardTransmit(m_hSCard, SCARD_PCI_T1,
		static_cast<LPCBYTE>(pSendData), SendSize, NULL, static_cast<LPBYTE>(pRecvData), pRecvSize);

	if (Result != SCARD_S_SUCCESS) {
		TCHAR szMessage[256];
		GetSCardErrorMessage(Result, szMessage, _countof(szMessage));
		SetError(TEXT("コマンド送信エラーです。"), NULL, szMessage);
		return false;
	}

	ClearError();

	return true;
}

#endif	// CARDREADER_SCARD_DYNAMIC_SUPPORT




#ifdef CARDREADER_BONCASCLIENT_SUPPORT

///////////////////////////////////////////////////////////////////////////////
//
// BonCasClient
//
///////////////////////////////////////////////////////////////////////////////

#define BONCASCLIENT_MODULE_NAME TEXT("BonCasClient.dll")


CBonCasClientCardReader::CBonCasClientCardReader()
	: m_hLib(NULL)
	, m_hSCard(0)
	, m_pReaderList(NULL)
	, m_pszReaderName(NULL)

	, m_pCasLinkReleaseContext(NULL)
	, m_pCasLinkConnect(NULL)
	, m_pCasLinkDisconnect(NULL)
	, m_pCasLinkTransmit(NULL)
{
}


CBonCasClientCardReader::~CBonCasClientCardReader()
{
	Close();
}


void CBonCasClientCardReader::GetModulePath(LPTSTR pszPath) const
{
	::GetModuleFileName(NULL, pszPath, MAX_PATH);
	::lstrcpy(::PathFindFileName(pszPath), BONCASCLIENT_MODULE_NAME);
}


bool CBonCasClientCardReader::Load(LPCTSTR pszFileName)
{
	m_hLib = ::LoadLibrary(pszFileName);
	if (m_hLib == NULL) {
		SetError(TEXT("ライブラリを読み込めません。"));
		return false;
	}

	typedef LONG (WINAPI *CasLinkEstablishContextFunc)(DWORD, LPCVOID, LPCVOID, LPSCARDCONTEXT);
	typedef LONG (WINAPI *CasLinkListReadersFunc)(SCARDCONTEXT, LPCTSTR, LPTSTR, LPDWORD);
	CasLinkEstablishContextFunc pEstablishContext;
	CasLinkListReadersFunc pListReaders;

	if (!GetLibraryFunc(m_hLib, pEstablishContext, "CasLinkEstablishContext")
	 || !GetLibraryFunc(m_hLib, pListReaders, FUNC_NAME("CasLinkListReaders"))
	 || !GetLibraryFunc(m_hLib, m_pCasLinkReleaseContext, "CasLinkReleaseContext")
	 || !GetLibraryFunc(m_hLib, m_pCasLinkConnect, "CasLinkConnect")
	 || !GetLibraryFunc(m_hLib, m_pCasLinkDisconnect, "CasLinkDisconnect")
	 || !GetLibraryFunc(m_hLib, m_pCasLinkTransmit, "CasLinkTransmit")) {
		::FreeLibrary(m_hLib);
		m_hLib = NULL;
		SetError(TEXT("関数のアドレスを取得できません。"));
		return false;
	}

	if (pEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &m_SCardContext) != SCARD_S_SUCCESS) {
		::FreeLibrary(m_hLib);
		m_hLib = NULL;
		SetError(TEXT("コンテキストを確立できません。"));
		return false;
	}

	// カードリーダを列挙する
	DWORD dwBuffLength = 0;

	if (pListReaders(m_SCardContext, NULL, NULL , &dwBuffLength) == SCARD_S_SUCCESS) {
		m_pReaderList = new TCHAR[dwBuffLength];
		if (pListReaders(m_SCardContext, NULL, m_pReaderList, &dwBuffLength) == SCARD_S_SUCCESS) {
			if (!CheckReaderList(m_pReaderList, dwBuffLength)) {
				Close();
				SetError(TEXT("カードリーダのリストが不正です。"));
				return false;
			}
		} else {
			delete [] m_pReaderList;
			m_pReaderList = NULL;
		}
	}

	return true;
}


bool CBonCasClientCardReader::Connect(LPCTSTR pszReader)
{
	LONG Result;
	DWORD dwActiveProtocol = SCARD_PROTOCOL_UNDEFINED;

	Result = m_pCasLinkConnect(m_SCardContext, pszReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, &m_hSCard, &dwActiveProtocol);
	if (Result != SCARD_S_SUCCESS) {
		TCHAR szMessage[256];
		GetSCardErrorMessage(Result, szMessage, _countof(szMessage));
		SetError(TEXT("カードリーダに接続できません。"), NULL, szMessage);
		return false;
	}

	if (dwActiveProtocol != SCARD_PROTOCOL_T1) {
		SetError(TEXT("アクティブプロトコルが不正です。"));
		return false;
	}

	return true;
}


bool CBonCasClientCardReader::Open(LPCTSTR pszReader)
{
	// 一旦クローズする
	Close();

	// ライブラリ読み込み
	TCHAR szFileName[MAX_PATH];
	GetModulePath(szFileName);
	if (!Load(szFileName))
		return false;

	if (pszReader) {
		// 指定されたカードリーダに対して接続を試みる
		if (!Connect(pszReader)) {
			Close();
			return false;
		}
	} else {
		// 全てのカードリーダに対して接続を試みる
		if (m_pReaderList == NULL || m_pReaderList[0] == _T('\0')) {
			Close();
			SetError(TEXT("カードリーダが見付かりません。"));
			return false;
		}

		LPCTSTR p = m_pReaderList;
		while (*p) {
			if (Connect(p)) {
				pszReader = p;
				break;
			}
			p += ::lstrlen(p) + 1;
		}
		if (!pszReader) {
			Close();
			return false;
		}
	}

	m_pszReaderName = StdUtil::strdup(pszReader);

	ClearError();

	return true;
}


void CBonCasClientCardReader::Close()
{
	if (m_hSCard) {
		m_pCasLinkDisconnect(m_hSCard, SCARD_LEAVE_CARD);
		m_hSCard = 0;
	}

	if (m_pszReaderName) {
		delete [] m_pszReaderName;
		m_pszReaderName = NULL;
	}

	if (m_hLib) {
		m_pCasLinkReleaseContext(m_SCardContext);

		::FreeLibrary(m_hLib);
		m_hLib = NULL;
	}

	if (m_pReaderList) {
		delete [] m_pReaderList;
		m_pReaderList = NULL;
	}
}


LPCTSTR CBonCasClientCardReader::GetReaderName() const
{
	return m_pszReaderName;
}


int CBonCasClientCardReader::NumReaders() const
{
	if (!m_pReaderList)
		return 0;
	LPCTSTR p = m_pReaderList;
	int i;
	for (i = 0; *p; i++) {
		p += ::lstrlen(p) + 1;
	}
	return i;
}


LPCTSTR CBonCasClientCardReader::EnumReader(int Index) const
{
	if (!m_pReaderList)
		return NULL;
	LPCTSTR p = m_pReaderList;
	for (int i = 0; i < Index; i++) {
		if (!*p)
			return NULL;
		p += ::lstrlen(p) + 1;
	}
	return p;
}


bool CBonCasClientCardReader::IsAvailable()
{
	TCHAR szPath[MAX_PATH];

	GetModulePath(szPath);

	return ::PathFileExists(szPath) != FALSE;
}


bool CBonCasClientCardReader::IsReaderAvailable(LPCTSTR pszReader)
{
	return true;
}


bool CBonCasClientCardReader::CheckAvailability(bool *pbAvailable, LPTSTR pszMessage, int MaxLength)
{
	bool bAvailable = IsAvailable();

	*pbAvailable = bAvailable;

	if (pszMessage) {
		if (bAvailable) {
			::lstrcpyn(pszMessage, BONCASCLIENT_MODULE_NAME TEXT(" が見付かりました。"), MaxLength);
		} else {
			::lstrcpyn(pszMessage, BONCASCLIENT_MODULE_NAME TEXT(" が見付かりません。"), MaxLength);
		}
	}

	return true;
}


bool CBonCasClientCardReader::Transmit(const void *pSendData,DWORD SendSize,void *pRecvData,DWORD *pRecvSize)
{
	if (!m_hSCard) {
		SetError(TEXT("カードリーダが開かれていません。"));
		return false;
	}

	LONG Result = m_pCasLinkTransmit(m_hSCard, SCARD_PCI_T1,
		static_cast<LPCBYTE>(pSendData), SendSize, NULL, static_cast<LPBYTE>(pRecvData), pRecvSize);

	if (Result != SCARD_S_SUCCESS) {
		TCHAR szMessage[256];
		GetSCardErrorMessage(Result, szMessage, _countof(szMessage));
		SetError(TEXT("コマンド送信エラーです。"), NULL, szMessage);
		return false;
	}

	ClearError();

	return true;
}

#endif	// CARDREADER_BONCASCLIENT_SUPPORT
