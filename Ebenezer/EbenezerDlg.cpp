// EbenezerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Ebenezer.h"
#include "EbenezerDlg.h"
#include "User.h"
#include "AiPacket.h"

#include "../shared/database/ItemTableSet.h"
#include "../shared/database/MagicTableSet.h"
#include "../shared/database/MagicType1Set.h"
#include "../shared/database/MagicType2Set.h"
#include "../shared/database/MagicType3Set.h"
#include "../shared/database/MagicType4Set.h"
#include "../shared/database/MagicType5Set.h"
#include "../shared/database/MagicType8Set.h"
#include "../shared/database/ZoneInfoSet.h"
#include "../shared/database/CoefficientSet.h"
#include "../shared/database/LevelUpTableSet.h"
#include "../shared/database/ServerResourceSet.h"
#include "../shared/database/KnightsSet.h"
#include "../shared/database/KnightsUserSet.h"
#include "../shared/database/KnightsRankSet.h"
#include "../shared/database/HomeSet.h"
#include "../shared/database/StartPositionSet.h"
#include "../shared/database/BattleSet.h"

#include "../shared/lzf.h"
#include "../shared/crc32.h"

#define GAME_TIME       	100
#define SEND_TIME			200
#define ALIVE_TIME			400

#define NUM_FLAG_VICTORY    4
#define AWARD_GOLD          5000

// Cryption
#if __VERSION >= 1700
T_KEY		g_private_key = 0x1207500120128966;
#elif __VERSION >= 1298 && __VERSION < 1453
T_KEY		g_private_key = 0x1234567890123456;
#else
T_KEY		g_private_key = 0x1257091582190465;
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRITICAL_SECTION g_serial_critical;
CRITICAL_SECTION g_region_critical;
CRITICAL_SECTION g_LogFile_critical;
CIOCPort	CEbenezerDlg::m_Iocport;

WORD	g_increase_serial = 1;
BYTE	g_serverdown_flag = FALSE;

DWORD WINAPI	ReadQueueThread(LPVOID lp);

DWORD WINAPI ReadQueueThread(LPVOID lp)
{
	CEbenezerDlg* pMain = (CEbenezerDlg*)lp;
	int recvlen = 0, index = 0, uid = -1, send_index = 0, buff_length = 0;
	BYTE command, result;
	char pBuf[1024], send_buff[1024];
	memset( pBuf, NULL, 1024 );
	memset( send_buff, NULL, 1024 );
	CUser* pUser = NULL;
	int currenttime = 0;

	while (TRUE)
	{
		if (pMain->m_LoggerRecvQueue.GetFrontMode() != R)
		{
			index = 0;
			recvlen = pMain->m_LoggerRecvQueue.GetData(pBuf);

			if (recvlen > MAX_PKTSIZE || recvlen == 0)
			{
				Sleep(1);
				continue;
			}

			command = GetByte(pBuf, index);
			uid = GetShort(pBuf, index);
			if( command == KNIGHTS_ALLLIST_REQ+0x10 && uid == -1 )	{
				pMain->m_KnightsManager.RecvKnightsAllList( pBuf+index );
				continue;
			}

			if ((pUser = pMain->GetUserPtr(uid)) == NULL)
				goto loop_pass;

			switch (command)
			{
				case WIZ_LOGIN:
					result = GetByte( pBuf, index );
					if( result == 0xFF )
						memset( pUser->m_strAccountID, NULL, MAX_ID_SIZE+1 );
					SetByte( send_buff, WIZ_LOGIN, send_index );
					SetByte( send_buff, result, send_index );					// 성공시 국가 정보
					pUser->Send( send_buff, send_index );
					break;
				case WIZ_SEL_NATION:
					SetByte( send_buff, WIZ_SEL_NATION, send_index );
					SetByte( send_buff, GetByte( pBuf, index ), send_index );	// 국가 정보
					pUser->Send( send_buff, send_index );
					break;
				case WIZ_NEW_CHAR:
					result = GetByte( pBuf, index );
					SetByte( send_buff, WIZ_NEW_CHAR, send_index );
					SetByte( send_buff, result, send_index );					// 성공시 국가 정보
					pUser->Send( send_buff, send_index );
					break;
				case WIZ_DEL_CHAR:
					pUser->RecvDeleteChar( pBuf + index );
					break;
				case WIZ_SEL_CHAR:
					pUser->SelectCharacter( pBuf + index );
					break;
				case WIZ_ALLCHAR_INFO_REQ:
					buff_length = GetShort( pBuf, index );
					if( buff_length > recvlen )
						break;
					SetByte( send_buff, WIZ_ALLCHAR_INFO_REQ, send_index );
					SetString( send_buff, pBuf+index, buff_length, send_index );
					pUser->Send( send_buff, send_index );
					break;
				case WIZ_SHOPPING_MALL:
					pUser->RecvStore(pBuf+index);
					break;
				case WIZ_SKILLDATA:
					{ 
						BYTE opcode = GetByte(pBuf, index);
						if (opcode == SKILL_DATA_LOAD)
							pUser->RecvSkillDataLoad(pBuf+index);
					} break;
				case WIZ_LOGOUT:
					if (pUser->m_pUserData->m_id[0] != 0)
					{
						TRACE("Logout Strange...%s\n", pUser->m_pUserData->m_id);
						pUser->Close();
					}
					break;
				case WIZ_FRIEND_PROCESS:
					pUser->RecvFriendProcess(pBuf+index);
					break;
				case KNIGHTS_CREATE+0x10:
				case KNIGHTS_JOIN+0x10:
				case KNIGHTS_WITHDRAW+0x10:
				case KNIGHTS_REMOVE+0x10:
				case KNIGHTS_ADMIT+0x10:
				case KNIGHTS_REJECT+0x10:
				case KNIGHTS_CHIEF+0x10:
				case KNIGHTS_VICECHIEF+0x10:
				case KNIGHTS_OFFICER+0x10:
				case KNIGHTS_PUNISH+0x10:
				case KNIGHTS_DESTROY+0x10:
				case KNIGHTS_MEMBER_REQ+0x10:
				case KNIGHTS_STASH+0x10:
				case KNIGHTS_LIST_REQ+0x10:
				case KNIGHTS_ALLLIST_REQ+0x10:
					pMain->m_KnightsManager.ReceiveKnightsProcess( pUser, pBuf+index, command );
					break;
				case WIZ_LOGIN_INFO:
					result = GetByte( pBuf, index );
					if( result == 0x00 )
						pUser->Close();
					break;
			}
		}
		else
			continue;

loop_pass:
		recvlen = 0;
		memset( pBuf, NULL, 1024 );
		send_index = 0;
		memset( send_buff, NULL, 1024 );
	}
}

/////////////////////////////////////////////////////////////////////////////
// CEbenezerDlg dialog

CEbenezerDlg::CEbenezerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CEbenezerDlg::IDD, pParent), m_Ini("gameserver.ini")
{
	//{{AFX_DATA_INIT(CEbenezerDlg)
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_bMMFCreate = FALSE;
	m_hReadQueueThread = NULL;

	m_nYear = 0; 
	m_nMonth = 0;
	m_nDate = 0;
	m_nHour = 0;
	m_nMin = 0;
	m_nWeather = 0;
	m_nAmount = 0;
	m_sPartyIndex = 0;

	m_nCastleCapture = 0;

	m_bKarusFlag = 0;
	m_bElmoradFlag = 0;

	m_byKarusOpenFlag = m_byElmoradOpenFlag = 0;
	m_byBanishFlag = 0;
	m_sBanishDelay = 0;

	m_sKarusDead = 0;
	m_sElmoradDead = 0;

	m_bVictory = 0;	
	m_byOldVictory = 0;
	m_byBattleSave = 0;
	m_sKarusCount = 0;
	m_sElmoradCount = 0;

	m_nBattleZoneOpenWeek=m_nBattleZoneOpenHourStart=m_nBattleZoneOpenHourEnd = 0;

	m_byBattleOpen = NO_BATTLE;
	m_byOldBattleOpen = NO_BATTLE;
	m_bFirstServerFlag = FALSE;
	m_bPointCheckFlag = FALSE;

	m_nServerNo = 0;
	m_nServerGroupNo = 0;
	m_nServerGroup = 0;
	m_sDiscount = 0;

	m_pUdpSocket = NULL;

	for (int h = 0 ; h < MAX_BBS_POST ; h++) {
		m_sBuyID[h] = -1;
		memset(m_strBuyTitle[h], NULL, MAX_BBS_TITLE);
		memset(m_strBuyMessage[h], NULL, MAX_BBS_MESSAGE);
		m_iBuyPrice[h] = 0;
		m_fBuyStartTime[h] = 0.0f;

		m_sSellID[h] = -1;
		memset(m_strSellTitle[h], NULL, MAX_BBS_TITLE);
		memset(m_strSellMessage[h], NULL, MAX_BBS_MESSAGE);
		m_iSellPrice[h] = 0;
		m_fSellStartTime[h] = 0.0f;
	}

	for(int i=0; i<20; i++)
		memset( m_ppNotice[i], NULL, 128 );
	memset( m_AIServerIP, NULL, 20 );

	m_bPermanentChatMode = FALSE;			// 비러머글 남는 공지 --;
	m_bPermanentChatFlag = FALSE;
	memset(m_strPermanentChat, NULL, 1024);
	memset( m_strKarusCaptain, 0x00, MAX_ID_SIZE+1 );
	memset( m_strElmoradCaptain, 0x00, MAX_ID_SIZE+1 );

	m_bSanta = FALSE;		// 갓댐 산타!!! >.<
}

void CEbenezerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEbenezerDlg)
	DDX_Control(pDX, IDC_GONGJI_EDIT, m_AnnounceEdit);
	DDX_Control(pDX, IDC_LIST1, m_StatusList);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CEbenezerDlg, CDialog)
	//{{AFX_MSG_MAP(CEbenezerDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEbenezerDlg message handlers

BOOL CEbenezerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_sZoneCount = 0;
	m_sSocketCount = 0;
	m_sErrorSocketCount = 0;
	m_KnightsManager.m_pMain = this;
	// sungyong 2002.05.23
	m_sSendSocket = 0;						
	m_bFirstServerFlag = FALSE;	
	m_bServerCheckFlag = FALSE;
	m_sReSocketCount = 0;
	m_fReConnectStart = 0.0f;
	// sungyong~ 2002.05.23

	//----------------------------------------------------------------------
	//	Logfile initialize
	//----------------------------------------------------------------------
	CTime cur = CTime::GetCurrentTime();
	char strLogFile[50];		memset(strLogFile, 0x00, 50);
	wsprintf(strLogFile, "RegionLog-%d-%d-%d.txt", cur.GetYear(), cur.GetMonth(), cur.GetDay());
	m_RegionLogFile.Open( strLogFile, CFile::modeWrite | CFile::modeCreate | CFile::modeNoTruncate | CFile::shareDenyNone );
	m_RegionLogFile.SeekToEnd();

	wsprintf(strLogFile, "PacketLog-%d-%d-%d.txt", cur.GetYear(), cur.GetMonth(), cur.GetDay());
	m_LogFile.Open( strLogFile, CFile::modeWrite | CFile::modeCreate | CFile::modeNoTruncate | CFile::shareDenyNone );
	m_LogFile.SeekToEnd();

	InitializeCriticalSection( &g_LogFile_critical );
	InitializeCriticalSection( &g_serial_critical );

	GetTimeFromIni();
	
	m_Iocport.Init( MAX_USER, CLIENT_SOCKSIZE, 4 );
	
	for(int i=0; i<MAX_USER; i++) {
		m_Iocport.m_SockArrayInActive[i] = new CUser;
	}

	_ZONE_SERVERINFO *pInfo = NULL;
	pInfo = m_ServerArray.GetData( m_nServerNo );
	if( !pInfo ) {
		AfxMessageBox("No Listen Port!!");
		AfxPostQuitMessage(0);
		return FALSE;
	}

	if (!m_Iocport.Listen(_LISTEN_PORT))
	{
		AfxMessageBox("FAIL TO CREATE LISTEN STATE", MB_OK);
		AfxPostQuitMessage(0);
		return FALSE;
	}

	if( !InitializeMMF() ) {
		AfxMessageBox("Main Shared Memory Initialize Fail");
		AfxPostQuitMessage(0);
		return FALSE;
	}

	if( !m_LoggerSendQueue.InitailizeMMF( MAX_PKTSIZE, MAX_COUNT, SMQ_LOGGERSEND ) ) {
		AfxMessageBox("SMQ Send Shared Memory Initialize Fail");
		AfxPostQuitMessage(0);
		return FALSE;
	}
	if( !m_LoggerRecvQueue.InitailizeMMF( MAX_PKTSIZE, MAX_COUNT, SMQ_LOGGERRECV ) ) {
		AfxMessageBox("SMQ Recv Shared Memory Initialize Fail");
		AfxPostQuitMessage(0);
		return FALSE;
	}

	if (!LoadTables())
	{
		AfxPostQuitMessage(-1);
		return FALSE;
	}

	LogFileWrite("before map file");
	if( !MapFileLoad() )
		AfxPostQuitMessage(0);

	LogFileWrite("after map file");

	LoadNoticeData();
	LoadBlockNameList();

	srand((unsigned int)time(NULL));

	DWORD id;
	m_hReadQueueThread = ::CreateThread( NULL, 0, ReadQueueThread, (LPVOID)this, 0, &id);

	m_pUdpSocket = new CUdpSocket(this);
	if( m_pUdpSocket->CreateSocket() == false ) {
		AfxMessageBox("Udp Socket Create Fail");
		AfxPostQuitMessage(0);
		return FALSE;
	}

	AIServerConnect();

	LogFileWrite("success");
	UserAcceptThread();

	AddToList("Game server started : %02d/%02d/%04d %d:%02d\r\n", cur.GetDay(), cur.GetMonth(), cur.GetYear(), cur.GetHour(), cur.GetMinute());
	return TRUE;  // return TRUE  unless you set the focus to a control
}

BOOL CEbenezerDlg::LoadTables()
{
	LogFileWrite("before ITEM");
	if (!LoadItemTable())
		return FALSE;

	LogFileWrite("before SERVER_RESOURCE");
	if (!LoadServerResourceTable())
		return FALSE;

	LogFileWrite("before MAGIC");
	if (!LoadMagicTable())
		return FALSE;

	LogFileWrite("before MAGIC_TYPE1");
	if (!LoadMagicType1())
		return FALSE;

	LogFileWrite("before MAGIC_TYPE2");
	if (!LoadMagicType2())
		return FALSE;

	LogFileWrite("before MAGIC_TYPE3");
	if (!LoadMagicType3())
		return FALSE;

	LogFileWrite("before MAGIC_TYPE4");
	if (!LoadMagicType4())
		return FALSE;

	LogFileWrite("before MAGIC_TYPE5");
	if (!LoadMagicType5())
		return FALSE;

	LogFileWrite("before MAGIC_TYPE8");
	if (!LoadMagicType8())
		return FALSE;

	LogFileWrite("before COEFFICIENT");
	if (!LoadCoefficientTable())
		return FALSE;

	LogFileWrite("before LEVEL_UP");
	if (!LoadLevelUpTable())
		return FALSE;

	LogFileWrite("before KNIGHTS");
	if (!LoadAllKnights())
		return FALSE;

	LogFileWrite("before KNIGHTS_USER");
	if (!LoadAllKnightsUserData())
		return FALSE;

	LogFileWrite("before HOME");
	if (!LoadHomeTable())
		return FALSE;

	LogFileWrite("before START_POSITION");
	if (!LoadStartPositionTable())
		return FALSE;

	LogFileWrite("before BATTLE");
	if (!LoadBattleTable())
		return FALSE;

	return TRUE;
}

BOOL CEbenezerDlg::ConnectToDatabase(bool reconnect /*= false*/)
{
	char dsn[128], uid[128], pwd[128];

	m_Ini.GetString("ODBC", "GAME_DSN", "KN_online", dsn, sizeof(dsn), false);
	m_Ini.GetString("ODBC", "GAME_UID", "knight", uid, sizeof(uid), false);
	m_Ini.GetString("ODBC", "GAME_PWD", "knight", pwd, sizeof(pwd), false);

	CString strConnect;
	strConnect.Format(_T("DSN=%s;UID=%s;PWD=%s"), dsn, uid, pwd);

	if (reconnect)
		m_GameDB.Close();

	try
	{
		m_GameDB.SetLoginTimeout(10);
		m_GameDB.Open(_T(""), FALSE, FALSE, (LPCTSTR )strConnect, FALSE);
	}
	catch (CDBException* e)
	{
		e->Delete();
	}
	
	if (!m_GameDB.IsOpen())
		return FALSE;

	return TRUE;
}

void CEbenezerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	CDialog::OnSysCommand(nID, lParam);
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CEbenezerDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CEbenezerDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

BOOL CEbenezerDlg::DestroyWindow() 
{
	KillTimer(GAME_TIME);
	KillTimer(SEND_TIME);
	KillTimer(ALIVE_TIME);

	KickOutAllUsers();

	if (m_hReadQueueThread != NULL)
	{
		TerminateThread(m_hReadQueueThread, 0);
		m_hReadQueueThread = 0;
	}

	if (m_bMMFCreate)
	{
		UnmapViewOfFile(m_lpMMFile);
		CloseHandle(m_hMMFile);
	}

	if (m_RegionLogFile.m_hFile != CFile::hFileNull) m_RegionLogFile.Close();
	if (m_LogFile.m_hFile != CFile::hFileNull) m_LogFile.Close();

	DeleteCriticalSection(&g_LogFile_critical);
	DeleteCriticalSection(&g_serial_critical);
	
	if (m_LevelUpArray.size())
		m_LevelUpArray.clear();

	if( m_pUdpSocket )
		delete m_pUdpSocket;

	return CDialog::DestroyWindow();
}

void CEbenezerDlg::UserAcceptThread()
{
	// User Socket Accept
	::ResumeThread( m_Iocport.m_hAcceptThread );
}

CString CEbenezerDlg::GetServerResource(int nResourceID)
{
	_SERVER_RESOURCE *pResource = m_ServerResourceArray.GetData(nResourceID);
	CString result = "";

	if (pResource == NULL)
		result.Format("%d", nResourceID);	
	else
		result = pResource->strResource;

	return result;
}

_START_POSITION *CEbenezerDlg::GetStartPosition(int nZoneID)
{
	return m_StartPositionArray.GetData(nZoneID);
}

long CEbenezerDlg::GetExpByLevel(int nLevel)
{
	LevelUpArray::iterator itr = m_LevelUpArray.find(nLevel);
	if (itr != m_LevelUpArray.end())
		return itr->second;

	return 0;
}

C3DMap * CEbenezerDlg::GetZoneByID(int zoneID)
{
	return m_ZoneArray.GetData(zoneID);
}

CUser* CEbenezerDlg::GetUserPtr(const char *userid, NameType type)
{
	CUser* pUser = NULL;
	BOOL bFind = FALSE;

	if (type == TYPE_ACCOUNT)
	{					// Account id check....
		for (int i = 0; i < MAX_USER; i++) 
		{
			pUser = GetUnsafeUserPtr(i);
			if (pUser == NULL)
				continue;

			if (!_strnicmp(pUser->m_strAccountID, userid, MAX_ID_SIZE)) 
			{
				bFind = TRUE;
				break;
			}
		}
	}
	else if (type == TYPE_CHARACTER)
	{									// character id check...
		for (int i = 0; i < MAX_USER; i++) 
		{
			pUser = GetUnsafeUserPtr(i);
			if (pUser == NULL)
				continue;

			if (!_strnicmp(pUser->m_pUserData->m_id, userid, MAX_ID_SIZE))
			{
				bFind = TRUE;
				break;
			}
		}
	}

	if (!bFind)
		return NULL;

	return pUser;
}

CUser* CEbenezerDlg::GetUserPtr(int sid)
{
	if (sid < 0 || sid >= MAX_USER)
		return NULL;

	return GetUnsafeUserPtr(sid);
}

CUser* CEbenezerDlg::GetUnsafeUserPtr(int sid)
{
	return (CUser *)m_Iocport.m_SockArray[sid];
}

_PARTY_GROUP * CEbenezerDlg::CreateParty(CUser *pLeader)
{
	pLeader->m_sPartyIndex = m_sPartyIndex++;
	if (m_sPartyIndex == SHRT_MAX)
		m_sPartyIndex = 0;

	EnterCriticalSection(&g_region_critical);
		
	_PARTY_GROUP * pParty = new _PARTY_GROUP;
	pParty->wIndex = pLeader->m_sPartyIndex;
	pParty->uid[0] = pLeader->GetSocketID();
	pParty->sMaxHp[0] = pLeader->m_iMaxHp;
	pParty->sHp[0] = pLeader->m_pUserData->m_sHp;
	pParty->bLevel[0] = pLeader->m_pUserData->m_bLevel;
	pParty->sClass[0] = pLeader->m_pUserData->m_sClass;
	if (!m_PartyArray.PutData( pParty->wIndex, pParty))
	{
		delete pParty;
		pLeader->m_sPartyIndex = -1;
		pParty = NULL;
	}
	LeaveCriticalSection(&g_region_critical);
	return pParty;
}

void CEbenezerDlg::DeleteParty(short sIndex)
{
	EnterCriticalSection(&g_region_critical);
	m_PartyArray.DeleteData(sIndex);
	LeaveCriticalSection(&g_region_critical);
}

void CEbenezerDlg::AddToList(const char * format, ...)
{
	char buffer[256];
	memset(buffer, 0x00, sizeof(buffer));

	va_list args;
	va_start(args, format);
	_vsnprintf(buffer, sizeof(buffer) - 1, format, args);
	va_end(args);

	m_StatusList.AddString((CString)buffer);

	EnterCriticalSection(&g_LogFile_critical);
	m_LogFile.Write(buffer, strlen(buffer));
	LeaveCriticalSection(&g_LogFile_critical);
}

void CEbenezerDlg::WriteLog(const char * format, ...)
{
	char buffer[256];
	memset(buffer, 0x00, sizeof(buffer));

	va_list args;
	va_start(args, format);
	_vsnprintf(buffer, sizeof(buffer) - 1, format, args);
	va_end(args);

	EnterCriticalSection(&g_LogFile_critical);
	m_LogFile.Write(buffer, strlen(buffer));
	LeaveCriticalSection(&g_LogFile_critical);
}

void CEbenezerDlg::OnTimer(UINT nIDEvent) 
{
	// sungyong 2002.05.23
	int count = 0, retval = 0;

	switch( nIDEvent ) {
	case GAME_TIME:
		UpdateGameTime();
		{	// AIServer Socket Alive Check Routine
			CAISocket* pAISock = NULL;
			for(int i=0; i<MAX_AI_SOCKET; i++) {
				pAISock = m_AISocketArray.GetData( i );
				if( pAISock && pAISock->GetState() == STATE_DISCONNECTED )
					AISocketConnect( i, 1 );
				else if( !pAISock )
					AISocketConnect( i, 1 );
				else count++;
			}

			if(count <= 0)	{	
				DeleteAllNpcList();
			}
			// sungyong~ 2002.05.23
		}
		break;
	case SEND_TIME:
		m_Iocport.m_PostOverlapped.Offset = OVL_SEND;
		retval = PostQueuedCompletionStatus( m_Iocport.m_hSendIOCPort, (DWORD)0, (DWORD)0, &(m_Iocport.m_PostOverlapped) );
		if ( !retval ) {
			int errValue;
			errValue = GetLastError();
			TRACE("Send PostQueued Error : %d\n", errValue);
		}
		break;
	case ALIVE_TIME:
		CheckAliveUser();
		break;
	}

	CDialog::OnTimer(nIDEvent);
}

int CEbenezerDlg::GetAIServerPort()
{
	int nPort = AI_KARUS_SOCKET_PORT;
	switch (m_nServerNo)
	{
	case ELMORAD:
		nPort = AI_ELMO_SOCKET_PORT;
		break;

	case BATTLE:
		nPort = AI_BATTLE_SOCKET_PORT;
		break;
	}
	return nPort;
}

// sungyong 2002.05.22
BOOL CEbenezerDlg::AIServerConnect()
{
	m_Ini.GetString("AI_SERVER", "IP", "127.0.0.1", m_AIServerIP, sizeof(m_AIServerIP));

	for (int i = 0; i < MAX_AI_SOCKET; i++)
	{
		if( !AISocketConnect( i ) ) 
		{
			foreach_stlmap (itr, m_AISocketArray)
			{
				if (itr->second == NULL)
					continue;

				itr->second->CloseProcess();
			}
			m_AISocketArray.DeleteAllData();

			AddToList("Failed to connect to AI server (%s:%d) - %d", m_AIServerIP, GetAIServerPort(), i);
			
#ifndef _DEBUG
			AfxMessageBox("AI Server Connect Fail!!");
#endif
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CEbenezerDlg::AISocketConnect(int zone, int flag)
{
	CAISocket* pAISock = NULL;
	int send_index = 0;
	char pBuf[128];
	memset( pBuf, NULL, 128 );

	//if( m_nServerNo == 3 ) return FALSE;

	pAISock = m_AISocketArray.GetData( zone );
	if( pAISock ) {
		if( pAISock->GetState() != STATE_DISCONNECTED )
			return TRUE;
		m_AISocketArray.DeleteData( zone );
	}

	pAISock = new CAISocket(zone);
	if (!pAISock->Create()
		|| !pAISock->Connect(&m_Iocport, m_AIServerIP, GetAIServerPort()))
	{
		delete pAISock;
		return FALSE;
	}

	SetByte(pBuf, AI_SERVER_CONNECT, send_index);
	SetByte(pBuf, zone, send_index);
	if(flag == 1)	SetByte(pBuf, 1, send_index);			// 재접속
	else			SetByte(pBuf, 0, send_index);			// 처음 접속..
	pAISock->Send(pBuf, send_index);

	// 해야할일 :이 부분 처리.....
	//SendAllUserInfo();
	//m_sSocketCount = zone;
	m_AISocketArray.PutData( zone, pAISock );

	TRACE("**** AISocket Connect Success!! ,, zone = %d ****\n", zone);
	return TRUE;
}
// ~sungyong 2002.05.22

void CEbenezerDlg::Send_All(char *pBuf, int len, CUser* pExceptUser, int nation )
{
	for (int i = 0; i < MAX_USER; i++)
	{
		CUser * pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL || pUser == pExceptUser || pUser->GetState() != STATE_GAMESTART || (nation != 0 && nation != pUser->getNation()))
			continue;

		pUser->Send(pBuf, len);
	}
}

void CEbenezerDlg::Send_Region(char *pBuf, int len, C3DMap *pMap, int x, int z, CUser* pExceptUser, bool bDirect)
{
	Send_UnitRegion( pBuf, len, pMap, x, z, pExceptUser, bDirect );
	Send_UnitRegion( pBuf, len, pMap, x-1, z-1, pExceptUser, bDirect );	// NW
	Send_UnitRegion( pBuf, len, pMap, x, z-1, pExceptUser, bDirect );		// N
	Send_UnitRegion( pBuf, len, pMap, x+1, z-1, pExceptUser, bDirect );	// NE
	Send_UnitRegion( pBuf, len, pMap, x-1, z, pExceptUser, bDirect );		// W
	Send_UnitRegion( pBuf, len, pMap, x+1, z, pExceptUser, bDirect );		// E
	Send_UnitRegion( pBuf, len, pMap, x-1, z+1, pExceptUser, bDirect );	// SW
	Send_UnitRegion( pBuf, len, pMap, x, z+1, pExceptUser, bDirect );		// S
	Send_UnitRegion( pBuf, len, pMap, x+1, z+1, pExceptUser, bDirect );	// SE
}

void CEbenezerDlg::Send_UnitRegion(char *pBuf, int len, C3DMap *pMap, int x, int z, CUser *pExceptUser, bool bDirect)
{
	if (pMap == NULL 
		|| x < 0 || z < 0 || x > pMap->GetXRegionMax() || z > pMap->GetZRegionMax())
		return;

	EnterCriticalSection(&g_region_critical);
	CRegion *pRegion = &pMap->m_ppRegion[x][z];

	foreach_stlmap (itr, pRegion->m_RegionUserArray)
	{
		CUser *pUser = GetUserPtr(*itr->second);
		if (pUser == NULL || pUser == pExceptUser || pUser->GetState() != STATE_GAMESTART)
			continue;

		if (bDirect)
			pUser->Send(pBuf, len);
		else
			pUser->RegionPacketAdd(pBuf, len);
	}
	LeaveCriticalSection(&g_region_critical);
}

void CEbenezerDlg::Send_NearRegion(char *pBuf, int len, C3DMap *pMap, int region_x, int region_z, float curx, float curz, CUser* pExceptUser)
{
	int left_border = region_x * VIEW_DISTANCE, top_border = region_z * VIEW_DISTANCE;
	Send_FilterUnitRegion( pBuf, len, pMap, region_x, region_z, curx, curz, pExceptUser);
	if( ((curx - left_border) > (VIEW_DISTANCE/2.0f)) ) {			// RIGHT
		if( ((curz - top_border) > (VIEW_DISTANCE/2.0f)) ) {	// BOTTOM
			Send_FilterUnitRegion( pBuf, len, pMap, region_x+1, region_z, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x, region_z+1, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x+1, region_z+1, curx, curz, pExceptUser);
		}
		else {													// TOP
			Send_FilterUnitRegion( pBuf, len, pMap, region_x+1, region_z, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x, region_z-1, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x+1, region_z-1, curx, curz, pExceptUser);
		}
	}
	else {														// LEFT
		if( ((curz - top_border) > (VIEW_DISTANCE/2.0f)) ) {	// BOTTOM
			Send_FilterUnitRegion( pBuf, len, pMap, region_x-1, region_z, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x, region_z+1, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x-1, region_z+1, curx, curz, pExceptUser);
		}
		else {													// TOP
			Send_FilterUnitRegion( pBuf, len, pMap, region_x-1, region_z, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x, region_z-1, curx, curz, pExceptUser);
			Send_FilterUnitRegion( pBuf, len, pMap, region_x-1, region_z-1, curx, curz, pExceptUser);
		}
	}
}

void CEbenezerDlg::Send_FilterUnitRegion(char *pBuf, int len, C3DMap *pMap, int x, int z, float ref_x, float ref_z, CUser *pExceptUser)
{
	if (pMap == NULL
		|| x < 0 || z < 0 || x > pMap->GetXRegionMax() || z>pMap->GetZRegionMax())
		return;

//	EnterCriticalSection(&g_region_critical);
	CRegion *pRegion = &pMap->m_ppRegion[x][z];

	foreach_stlmap (itr, pRegion->m_RegionUserArray)
	{
		CUser *pUser = GetUserPtr(*itr->second);
		if (pUser == NULL || pUser == pExceptUser || pUser->GetState() != STATE_GAMESTART)
			continue;

		if (sqrt(pow((pUser->m_pUserData->m_curx - ref_x), 2) + pow((pUser->m_pUserData->m_curz - ref_z), 2)) < 32)
			pUser->RegionPacketAdd(pBuf, len);
	}

//	LeaveCriticalSection( &g_region_critical );
}

void CEbenezerDlg::Send_PartyMember(int party, char *pBuf, int len)
{
	_PARTY_GROUP* pParty = m_PartyArray.GetData(party);
	if (pParty == NULL)
		return;

	for (int i = 0; i < 8; i++)
	{
		CUser *pUser = GetUserPtr(pParty->uid[i]);
		if (pUser == NULL)
			continue;

		pUser->Send(pBuf, len);
	}
}

void CEbenezerDlg::Send_KnightsMember( int index, char* pBuf, int len, int zone )
{
	CKnights* pKnights = m_KnightsArray.GetData(index);
	if (pKnights == NULL)
		return;

	for (int i = 0; i < MAX_USER; i++)
	{
		CUser *pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL
			|| pUser->m_pUserData->m_bKnights != index) 
			continue;

		if (zone == 100 || pUser->getZoneID() == zone)
			pUser->Send( pBuf, len );
	}
}

// sungyong 2002.05.22
void CEbenezerDlg::Send_AIServer(char* pBuf, int len)
{
	CAISocket* pSocket = NULL;
	int send_size = 0, old_send_socket = 0;

	for(int i=0; i<MAX_AI_SOCKET; i++) {
		pSocket = m_AISocketArray.GetData( i );
		if( pSocket == NULL)	{
			m_sSendSocket++;
			if(m_sSendSocket >= MAX_AI_SOCKET)	m_sSendSocket = 0;
			continue;
		}
		if( i == m_sSendSocket )	{
			send_size = pSocket->Send( pBuf, len);
			old_send_socket = m_sSendSocket;
			m_sSendSocket++;
			if(m_sSendSocket >= MAX_AI_SOCKET)	m_sSendSocket = 0;
			if(send_size == 0)	continue;
			else	{
				//TRACE(" <--- Send_AIServer : length = %d, socket = %d \n", send_size, old_send_socket);
				return;
			}
		}
		else	{
			continue;
		}
	}
}
// ~sungyong 2002.05.22

BOOL CEbenezerDlg::InitializeMMF()
{
	BOOL bCreate = TRUE;
	DWORD filesize = MAX_USER * sizeof(_USER_DATA);
	m_hMMFile = CreateFileMapping ((HANDLE)-1, NULL, PAGE_READWRITE, 0, filesize, "KNIGHT_DB");
	
	if (m_hMMFile != NULL && GetLastError() == ERROR_ALREADY_EXISTS) 
	{
		m_hMMFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, "KNIGHT_DB");
		if (m_hMMFile == NULL)
		{
			DEBUG_LOG("Shared Memory Load Fail!!");
			m_hMMFile = INVALID_HANDLE_VALUE; 
			return FALSE;
		}
		bCreate = FALSE;
	}
	
	DEBUG_LOG("Shared Memory Create Success!!");

    m_lpMMFile = (char *)MapViewOfFile(m_hMMFile, FILE_MAP_WRITE, 0, 0, 0);
	if (!m_lpMMFile)
		return FALSE;

	if (bCreate)
		memset(m_lpMMFile, NULL, filesize);

	m_bMMFCreate = bCreate;

	for (int i = 0; i < MAX_USER; i++)
	{
		CUser* pUser = (CUser*)m_Iocport.m_SockArrayInActive[i];
		if (pUser)
			pUser->m_pUserData = (_USER_DATA*)(m_lpMMFile + i * sizeof(_USER_DATA));
	}

	return TRUE;
}

BOOL CEbenezerDlg::MapFileLoad()
{
	CFile file;
	CString szFullPath, errormsg, sZoneName;
	C3DMap* pMap = NULL;
	EVENT*	pEvent = NULL;

	CZoneInfoSet	ZoneInfoSet(&m_GameDB);

	if( !ZoneInfoSet.Open() ) {
		AfxMessageBox(_T("ZoneInfoTable Open Fail!"));
		return FALSE;
	}
	if(ZoneInfoSet.IsBOF() || ZoneInfoSet.IsEOF()) {
		AfxMessageBox(_T("ZoneInfoTable Empty!"));
		return FALSE;
	}

	ZoneInfoSet.MoveFirst();

	while( !ZoneInfoSet.IsEOF() )
	{
		sZoneName = ZoneInfoSet.m_strZoneName;
		szFullPath.Format(".\\MAP\\%s", sZoneName);
		
		LogFileWrite("mapfile load\r\n");
		if (!file.Open(szFullPath, CFile::modeRead)) {
			errormsg.Format( "File Open Fail - %s\n", szFullPath );
			AfxMessageBox(errormsg);
			return FALSE;
		}

		pMap = new C3DMap;

		pMap->m_nServerNo = ZoneInfoSet.m_ServerNo;
		pMap->m_nZoneNumber = ZoneInfoSet.m_ZoneNo;
		strcpy( pMap->m_MapName, (char*)(LPCTSTR)sZoneName );
		pMap->m_fInitX = (float)(ZoneInfoSet.m_InitX/100.0);
		pMap->m_fInitZ = (float)(ZoneInfoSet.m_InitZ/100.0);
		pMap->m_fInitY = (float)(ZoneInfoSet.m_InitY/100.0);
		pMap->m_bType = ZoneInfoSet.m_Type;
		
		if( !pMap->LoadMap( (HANDLE)file.m_hFile ) ) {
			errormsg.Format( "Map Load Fail - %s\n", szFullPath );
			AfxMessageBox(errormsg);
			delete pMap;
			return FALSE;
		}
		m_ZoneArray.PutData(ZoneInfoSet.m_ZoneNo, pMap);

		LogFileWrite("before script\r\n");

		pEvent = new EVENT;
		if( !pEvent->LoadEvent(ZoneInfoSet.m_ZoneNo) )
		{
			delete pEvent;
			pEvent = NULL;
		}
		if( pEvent ) {
			if( !m_Event.PutData(pEvent->m_Zone, pEvent) ) {
				delete pEvent;
				pEvent = NULL;
			}
		}

		ZoneInfoSet.MoveNext();

		file.Close();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadItemTable()
{
	CItemTableSet	ItemTableSet(&m_GameDB);

	if( !ItemTableSet.Open() ) {
		AfxMessageBox(_T("ItemTable Open Fail!"));
		return FALSE;
	}
	if(ItemTableSet.IsBOF() || ItemTableSet.IsEOF()) {
		AfxMessageBox(_T("ItemTable Empty!"));
		return FALSE;
	}

	ItemTableSet.MoveFirst();

	while( !ItemTableSet.IsEOF() )
	{
		_ITEM_TABLE* pTableItem = new _ITEM_TABLE;
				
		pTableItem->m_iNum = ItemTableSet.m_Num;
		strcpy(pTableItem->m_strName, ItemTableSet.m_strName);
		pTableItem->m_bKind = ItemTableSet.m_Kind;
		pTableItem->m_bSlot = ItemTableSet.m_Slot;
		pTableItem->m_bRace = ItemTableSet.m_Race;
		pTableItem->m_bClass = ItemTableSet.m_Class;
		pTableItem->m_sDamage = ItemTableSet.m_Damage;
		pTableItem->m_sDelay = ItemTableSet.m_Delay;
		pTableItem->m_sRange = ItemTableSet.m_Range;
		pTableItem->m_sWeight = ItemTableSet.m_Weight;
		pTableItem->m_sDuration = ItemTableSet.m_Duration;
		pTableItem->m_iBuyPrice = ItemTableSet.m_BuyPrice;
		pTableItem->m_iSellPrice = ItemTableSet.m_SellPrice;
		pTableItem->m_sAc = ItemTableSet.m_Ac;
		pTableItem->m_bCountable = ItemTableSet.m_Countable;
		pTableItem->m_iEffect1 = ItemTableSet.m_Effect1;
		pTableItem->m_iEffect2 = ItemTableSet.m_Effect2;
		pTableItem->m_bReqLevel = ItemTableSet.m_ReqLevel;
		pTableItem->m_bReqRank = ItemTableSet.m_ReqRank;
		pTableItem->m_bReqTitle = ItemTableSet.m_ReqTitle;
		pTableItem->m_bReqStr = ItemTableSet.m_ReqStr;
		pTableItem->m_bReqSta = ItemTableSet.m_ReqSta;
		pTableItem->m_bReqDex = ItemTableSet.m_ReqDex;
		pTableItem->m_bReqIntel = ItemTableSet.m_ReqIntel;
		pTableItem->m_bReqCha = ItemTableSet.m_ReqCha;
		pTableItem->m_bSellingGroup = ItemTableSet.m_SellingGroup;
		pTableItem->m_ItemType = ItemTableSet.m_ItemType;
		pTableItem->m_sHitrate = ItemTableSet.m_Hitrate;
		pTableItem->m_sEvarate = ItemTableSet.m_Evasionrate;
		pTableItem->m_sDaggerAc = ItemTableSet.m_DaggerAc;
		pTableItem->m_sSwordAc = ItemTableSet.m_SwordAc;
		pTableItem->m_sMaceAc = ItemTableSet.m_MaceAc;
		pTableItem->m_sAxeAc = ItemTableSet.m_AxeAc;
		pTableItem->m_sSpearAc = ItemTableSet.m_SpearAc;
		pTableItem->m_sBowAc = ItemTableSet.m_BowAc;
		pTableItem->m_bFireDamage = ItemTableSet.m_FireDamage;
		pTableItem->m_bIceDamage = ItemTableSet.m_IceDamage;
		pTableItem->m_bLightningDamage = ItemTableSet.m_LightningDamage;
		pTableItem->m_bPoisonDamage = ItemTableSet.m_PoisonDamage;
		pTableItem->m_bHPDrain = ItemTableSet.m_HPDrain;
		pTableItem->m_bMPDamage = ItemTableSet.m_MPDamage;
		pTableItem->m_bMPDrain = ItemTableSet.m_MPDrain;
		pTableItem->m_bMirrorDamage = ItemTableSet.m_MirrorDamage;
		pTableItem->m_bDroprate = ItemTableSet.m_Droprate;
		pTableItem->m_bStrB = ItemTableSet.m_StrB;
		pTableItem->m_bStaB = ItemTableSet.m_StaB;
		pTableItem->m_bDexB = ItemTableSet.m_DexB;
		pTableItem->m_bIntelB = ItemTableSet.m_IntelB;
		pTableItem->m_bChaB = ItemTableSet.m_ChaB;
		pTableItem->m_MaxHpB = ItemTableSet.m_MaxHpB;
		pTableItem->m_MaxMpB = ItemTableSet.m_MaxMpB;
		pTableItem->m_bFireR = ItemTableSet.m_FireR;
		pTableItem->m_bColdR = ItemTableSet.m_ColdR;
		pTableItem->m_bLightningR = ItemTableSet.m_LightningR;
		pTableItem->m_bMagicR = ItemTableSet.m_MagicR;
		pTableItem->m_bPoisonR = ItemTableSet.m_PoisonR;
		pTableItem->m_bCurseR = ItemTableSet.m_CurseR;

		if( !m_ItemtableArray.PutData(pTableItem->m_iNum, pTableItem) ) {
			TRACE("ItemTable PutData Fail - %d\n", pTableItem->m_iNum );
			delete pTableItem;
			pTableItem = NULL;
		}
		
		ItemTableSet.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadServerResourceTable()
{
	CServerResourceSet ServerResourceSet(&m_GameDB);

	if (!ServerResourceSet.Open())
	{
		AfxMessageBox(_T("Failed to open SERVER_RESOURCE table!"));
		return FALSE;
	}

	if (ServerResourceSet.IsBOF() || ServerResourceSet.IsEOF()) 
	{
		AfxMessageBox(_T("SERVER_RESOURCE table is empty!"));
		return FALSE;
	}

	ServerResourceSet.MoveFirst();

	while (!ServerResourceSet.IsEOF())
	{
		_SERVER_RESOURCE *pResource = new _SERVER_RESOURCE;
		pResource->nResourceID = ServerResourceSet.m_nResourceID;
		strcpy(pResource->strResource, ServerResourceSet.m_strResource.TrimRight());
		
		if (!m_ServerResourceArray.PutData(pResource->nResourceID, pResource))
			delete pResource;

		ServerResourceSet.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadMagicTable()
{
	CMagicTableSet	MagicTableSet(&m_GameDB);

	if( !MagicTableSet.Open() ) {
		AfxMessageBox(_T("MagicTable Open Fail!"));
		return FALSE;
	}
	if(MagicTableSet.IsBOF() || MagicTableSet.IsEOF()) {
		AfxMessageBox(_T("MagicTable Empty!"));
		return FALSE;
	}

	MagicTableSet.MoveFirst();

	while( !MagicTableSet.IsEOF() )
	{
		_MAGIC_TABLE* pTableMagic = new _MAGIC_TABLE;
				
		pTableMagic->iNum = MagicTableSet.m_MagicNum;
		pTableMagic->sFlyingEffect = MagicTableSet.m_FlyingEffect;
		pTableMagic->bMoral = MagicTableSet.m_Moral;
		pTableMagic->bSkillLevel = MagicTableSet.m_SkillLevel;
		pTableMagic->sSkill = MagicTableSet.m_Skill;
		pTableMagic->sMsp = MagicTableSet.m_Msp;
		pTableMagic->sHP = MagicTableSet.m_HP;
		pTableMagic->bItemGroup = MagicTableSet.m_ItemGroup;
		pTableMagic->iUseItem = MagicTableSet.m_UseItem;
		pTableMagic->bCastTime = MagicTableSet.m_CastTime;
		pTableMagic->bReCastTime = MagicTableSet.m_ReCastTime;
		pTableMagic->bSuccessRate = MagicTableSet.m_SuccessRate;
		pTableMagic->bType1 = MagicTableSet.m_Type1;
		pTableMagic->bType2 = MagicTableSet.m_Type2;
		pTableMagic->sRange = MagicTableSet.m_Range;
		pTableMagic->bEtc = MagicTableSet.m_Etc;

		if( !m_MagictableArray.PutData(pTableMagic->iNum, pTableMagic) ) {
			TRACE("MagicTable PutData Fail - %d\n", pTableMagic->iNum );
			delete pTableMagic;
			pTableMagic = NULL;
		}
		
		MagicTableSet.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadMagicType1()
{
	CMagicType1Set	MagicType1Set(&m_GameDB);

	if( !MagicType1Set.Open() ) {
		AfxMessageBox(_T("MagicType1 Open Fail!"));
		return FALSE;
	}
	if(MagicType1Set.IsBOF() || MagicType1Set.IsEOF()) {
		AfxMessageBox(_T("MagicType1 Empty!"));
		return FALSE;
	}

	MagicType1Set.MoveFirst();

	while( !MagicType1Set.IsEOF() )
	{
		_MAGIC_TYPE1* pType1Magic = new _MAGIC_TYPE1;
				
		pType1Magic->iNum = MagicType1Set.m_iNum;
		pType1Magic->bHitType = MagicType1Set.m_Type;
		pType1Magic->bDelay = MagicType1Set.m_Delay;
		pType1Magic->bComboCount = MagicType1Set.m_ComboCount;
		pType1Magic->bComboType = MagicType1Set.m_ComboType;
		pType1Magic->sComboDamage = MagicType1Set.m_ComboDamage;
		pType1Magic->sHit = MagicType1Set.m_Hit;
		pType1Magic->sHitRate = MagicType1Set.m_HitRate;
		pType1Magic->sRange = MagicType1Set.m_Range;

		if( !m_Magictype1Array.PutData(pType1Magic->iNum, pType1Magic) ) {
			TRACE("MagicType1 PutData Fail - %d\n", pType1Magic->iNum );
			delete pType1Magic;
			pType1Magic = NULL;
		}

		MagicType1Set.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadMagicType2()
{
	CMagicType2Set	MagicType2Set(&m_GameDB);

	if( !MagicType2Set.Open() ) {
		AfxMessageBox(_T("MagicType2 Open Fail!"));
		return FALSE;
	}
	if(MagicType2Set.IsBOF() || MagicType2Set.IsEOF()) {
		AfxMessageBox(_T("MagicType2 Empty!"));
		return FALSE;
	}

	MagicType2Set.MoveFirst();

	while( !MagicType2Set.IsEOF() )
	{
		_MAGIC_TYPE2* pType2Magic = new _MAGIC_TYPE2;
				
		pType2Magic->iNum = MagicType2Set.m_iNum;
		pType2Magic->bHitType = MagicType2Set.m_HitType;
		pType2Magic->sHitRate = MagicType2Set.m_HitRate;
		pType2Magic->sAddDamage = MagicType2Set.m_AddDamage;
		pType2Magic->sAddRange = MagicType2Set.m_AddRange;
		pType2Magic->bNeedArrow = MagicType2Set.m_NeedArrow;

		if( !m_Magictype2Array.PutData(pType2Magic->iNum, pType2Magic) ) {
			TRACE("MagicType2 PutData Fail - %d\n", pType2Magic->iNum );
			delete pType2Magic;
			pType2Magic = NULL;
		}
		MagicType2Set.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadMagicType3()
{
	CMagicType3Set	MagicType3Set(&m_GameDB);

	if( !MagicType3Set.Open() ) {
		AfxMessageBox(_T("MagicType3 Open Fail!"));
		return FALSE;
	}
	if(MagicType3Set.IsBOF() || MagicType3Set.IsEOF()) {
		AfxMessageBox(_T("MagicType3 Empty!"));
		return FALSE;
	}

	MagicType3Set.MoveFirst();

	while( !MagicType3Set.IsEOF() )
	{
		_MAGIC_TYPE3* pType3Magic = new _MAGIC_TYPE3;
				
		pType3Magic->iNum = MagicType3Set.m_iNum;
		pType3Magic->bAttribute = MagicType3Set.m_Attribute;
		pType3Magic->bDirectType = MagicType3Set.m_DirectType;
		pType3Magic->bRadius = MagicType3Set.m_Radius;
		pType3Magic->sAngle = MagicType3Set.m_Angle;
		pType3Magic->sDuration = MagicType3Set.m_Duration;
		pType3Magic->sEndDamage = MagicType3Set.m_EndDamage;
		pType3Magic->sFirstDamage = MagicType3Set.m_FirstDamage;
		pType3Magic->sTimeDamage = MagicType3Set.m_TimeDamage;

		if( !m_Magictype3Array.PutData(pType3Magic->iNum, pType3Magic) ) {
			TRACE("MagicType3 PutData Fail - %d\n", pType3Magic->iNum );
			delete pType3Magic;
			pType3Magic = NULL;
		}	
		MagicType3Set.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadMagicType4()
{
	CMagicType4Set	MagicType4Set(&m_GameDB);

	if( !MagicType4Set.Open() ) {
		AfxMessageBox(_T("MagicType4 Open Fail!"));
		return FALSE;
	}
	if(MagicType4Set.IsBOF() || MagicType4Set.IsEOF()) {
		AfxMessageBox(_T("MagicType4 Empty!"));
		return FALSE;
	}

	MagicType4Set.MoveFirst();

	while( !MagicType4Set.IsEOF() )
	{
		_MAGIC_TYPE4* pType4Magic = new _MAGIC_TYPE4;
						
		pType4Magic->iNum = MagicType4Set.m_iNum;
		pType4Magic->bBuffType = MagicType4Set.m_BuffType;
		pType4Magic->bRadius = MagicType4Set.m_Radius;
		pType4Magic->sDuration = MagicType4Set.m_Duration;
		pType4Magic->bAttackSpeed = MagicType4Set.m_AttackSpeed;
		pType4Magic->bSpeed = MagicType4Set.m_Speed;
		pType4Magic->sAC = MagicType4Set.m_AC;
		pType4Magic->bAttack = MagicType4Set.m_Attack;
		pType4Magic->sMaxHP = MagicType4Set.m_MaxHP;
		pType4Magic->bHitRate = MagicType4Set.m_HitRate;
		pType4Magic->sAvoidRate = MagicType4Set.m_AvoidRate;
		pType4Magic->bStr = MagicType4Set.m_Str;
		pType4Magic->bSta = MagicType4Set.m_Sta;
		pType4Magic->bDex = MagicType4Set.m_Dex;
		pType4Magic->bIntel = MagicType4Set.m_Intel;
		pType4Magic->bCha = MagicType4Set.m_Cha;
		pType4Magic->bFireR = MagicType4Set.m_FireR;
		pType4Magic->bColdR = MagicType4Set.m_ColdR;
		pType4Magic->bLightningR = MagicType4Set.m_LightningR;
		pType4Magic->bMagicR = MagicType4Set.m_MagicR;
		pType4Magic->bDiseaseR = MagicType4Set.m_DiseaseR;
		pType4Magic->bPoisonR = MagicType4Set.m_PoisonR;

		if( !m_Magictype4Array.PutData(pType4Magic->iNum, pType4Magic) ) 
		{
			TRACE("MagicType4 PutData Fail - %d\n", pType4Magic->iNum );
			delete pType4Magic;
			pType4Magic = NULL;
		}	
		MagicType4Set.MoveNext();
	}
	return TRUE;
}

BOOL CEbenezerDlg::LoadMagicType5()
{
	CMagicType5Set	MagicType5Set(&m_GameDB);

	if( !MagicType5Set.Open() ) {
		AfxMessageBox(_T("MagicType5 Open Fail!"));
		return FALSE;
	}

	if(MagicType5Set.IsBOF() || MagicType5Set.IsEOF()) {
		AfxMessageBox(_T("MagicType5 Empty!"));
		return FALSE;
	}

	MagicType5Set.MoveFirst();

	while( !MagicType5Set.IsEOF() )
	{
		_MAGIC_TYPE5* pType5Magic = new _MAGIC_TYPE5;

		pType5Magic->iNum = MagicType5Set.m_iNum;
		pType5Magic->bType = MagicType5Set.m_Type;
		pType5Magic->bExpRecover = MagicType5Set.m_ExpRecover;
		pType5Magic->sNeedStone = MagicType5Set.m_NeedStone; 

		if( !m_Magictype5Array.PutData(pType5Magic->iNum, pType5Magic) ) {
			TRACE("MagicType5 PutData Fail - %d\n", pType5Magic->iNum );
			delete pType5Magic;
			pType5Magic = NULL;
		}	
		MagicType5Set.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadMagicType8()
{
	CMagicType8Set	MagicType8Set(&m_GameDB);

	if( !MagicType8Set.Open() ) {
		AfxMessageBox(_T("MagicType8 Open Fail!"));
		return FALSE;
	}
	if(MagicType8Set.IsBOF() || MagicType8Set.IsEOF()) {
		AfxMessageBox(_T("MagicType8 Empty!"));
		return FALSE;
	}

	MagicType8Set.MoveFirst();

	while( !MagicType8Set.IsEOF() )
	{
		_MAGIC_TYPE8* pType8Magic = new _MAGIC_TYPE8;
				
		pType8Magic->iNum = MagicType8Set.m_iNum;
		pType8Magic->bTarget = MagicType8Set.m_Target;
		pType8Magic->sRadius = MagicType8Set.m_Radius;
		pType8Magic->bWarpType = MagicType8Set.m_WarpType;
		pType8Magic->sExpRecover = MagicType8Set.m_ExpRecover;

		if( !m_Magictype8Array.PutData(pType8Magic->iNum, pType8Magic) ) {
			TRACE("MagicType8 PutData Fail - %d\n", pType8Magic->iNum );
			delete pType8Magic;
			pType8Magic = NULL;
		}

		MagicType8Set.MoveNext();
	}

	return TRUE; 
}

BOOL CEbenezerDlg::LoadCoefficientTable()
{
	CCoefficientSet	CoefficientSet(&m_GameDB);

	if( !CoefficientSet.Open() ) {
		AfxMessageBox(_T("CharacterDataTable Open Fail!"));
		return FALSE;
	}
	if(CoefficientSet.IsBOF() || CoefficientSet.IsEOF()) {
		AfxMessageBox(_T("CharaterDataTable Empty!"));
		return FALSE;
	}

	CoefficientSet.MoveFirst();

	while( !CoefficientSet.IsEOF() )
	{
		_CLASS_COEFFICIENT* p_TableCoefficient = new _CLASS_COEFFICIENT;

		p_TableCoefficient->sClassNum = (short)CoefficientSet.m_sClass;
		p_TableCoefficient->ShortSword = CoefficientSet.m_ShortSword;
		p_TableCoefficient->Sword = CoefficientSet.m_Sword;
		p_TableCoefficient->Axe = CoefficientSet.m_Axe;
		p_TableCoefficient->Club = CoefficientSet.m_Club;
		p_TableCoefficient->Spear = CoefficientSet.m_Spear;
		p_TableCoefficient->Pole = CoefficientSet.m_Pole;
		p_TableCoefficient->Staff = CoefficientSet.m_Staff;
		p_TableCoefficient->Bow = CoefficientSet.m_Bow;
		p_TableCoefficient->HP = CoefficientSet.m_Hp;
		p_TableCoefficient->MP = CoefficientSet.m_Mp;
		p_TableCoefficient->SP = CoefficientSet.m_Sp;
		p_TableCoefficient->AC = CoefficientSet.m_Ac;
		p_TableCoefficient->Hitrate = CoefficientSet.m_Hitrate;
		p_TableCoefficient->Evasionrate = CoefficientSet.m_Evasionrate;

		if( !m_CoefficientArray.PutData(p_TableCoefficient->sClassNum, p_TableCoefficient) ) {
			TRACE("Coefficient PutData Fail - %d\n", p_TableCoefficient->sClassNum );
			delete p_TableCoefficient;
			p_TableCoefficient = NULL;
		}
		
		CoefficientSet.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadLevelUpTable()
{
	CLevelUpTableSet	LevelUpTableSet(&m_GameDB);

	if( !LevelUpTableSet.Open() ) {
		AfxMessageBox(_T("LevelUpTable Open Fail!"));
		return FALSE;
	}
	if(LevelUpTableSet.IsBOF() || LevelUpTableSet.IsEOF()) {
		AfxMessageBox(_T("LevelUpTable Empty!"));
		return FALSE;
	}

	LevelUpTableSet.MoveFirst();

	while (!LevelUpTableSet.IsEOF())
	{
		m_LevelUpArray.insert(make_pair(LevelUpTableSet.m_level, LevelUpTableSet.m_Exp));
		LevelUpTableSet.MoveNext();
	}

	return TRUE;
}

void CEbenezerDlg::GetTimeFromIni()
{
	int year=0, month=0, date=0, hour=0, server_count=0, sgroup_count = 0, i=0;
	char ipkey[20]; memset( ipkey, 0x00, 20 );

	if (!ConnectToDatabase())
	{
		AfxMessageBox(_T("Couldn't connect to game database."));
		return;
	}

	m_nYear = m_Ini.GetInt("TIMER", "YEAR", 1);
	m_nMonth = m_Ini.GetInt("TIMER", "MONTH", 1);
	m_nDate = m_Ini.GetInt("TIMER", "DATE", 1);
	m_nHour = m_Ini.GetInt("TIMER", "HOUR", 1);
	m_nWeather = m_Ini.GetInt("TIMER", "WEATHER", 1);

	m_nBattleZoneOpenWeek  = m_Ini.GetInt("BATTLE", "WEEK", 5);
	m_nBattleZoneOpenHourStart  = m_Ini.GetInt("BATTLE", "START_TIME", 20);
	m_nBattleZoneOpenHourEnd  = m_Ini.GetInt("BATTLE", "END_TIME", 0);

	m_nCastleCapture = m_Ini.GetInt("CASTLE", "NATION", 1);
	m_nServerNo = m_Ini.GetInt("ZONE_INFO", "MY_INFO", 1);
	m_nServerGroup = m_Ini.GetInt("ZONE_INFO", "SERVER_NUM", 0);
	server_count = m_Ini.GetInt("ZONE_INFO", "SERVER_COUNT", 1);
	if( server_count < 1 ) {
		AfxMessageBox("ServerCount Error!!");
		return;
	}

	for( i=0; i<server_count; i++ ) {
		_ZONE_SERVERINFO *pInfo = new _ZONE_SERVERINFO;
		sprintf( ipkey, "SERVER_%02d", i );
		pInfo->sServerNo = m_Ini.GetInt("ZONE_INFO", ipkey, 1);
		sprintf( ipkey, "SERVER_IP_%02d", i );
		m_Ini.GetString("ZONE_INFO", ipkey, "127.0.0.1", pInfo->strServerIP, sizeof(pInfo->strServerIP));
		m_ServerArray.PutData(pInfo->sServerNo, pInfo);
	}

	if( m_nServerGroup != 0 )	{
		m_nServerGroupNo = m_Ini.GetInt("SG_INFO", "GMY_INFO", 1);
		sgroup_count = m_Ini.GetInt("SG_INFO", "GSERVER_COUNT", 1);
		if( server_count < 1 ) {
			AfxMessageBox("ServerCount Error!!");
			return;
		}
		for( i=0; i<sgroup_count; i++ ) {
			_ZONE_SERVERINFO *pInfo = new _ZONE_SERVERINFO;
			sprintf( ipkey, "GSERVER_%02d", i );
			pInfo->sServerNo = m_Ini.GetInt("SG_INFO", ipkey, 1);
			sprintf( ipkey, "GSERVER_IP_%02d", i );
			m_Ini.GetString("SG_INFO", ipkey, "127.0.0.1", pInfo->strServerIP, sizeof(pInfo->strServerIP));

			m_ServerGroupArray.PutData(pInfo->sServerNo, pInfo);
		}
	}

	SetTimer( GAME_TIME, 6000, NULL );
	SetTimer( SEND_TIME, 200, NULL );
	SetTimer( ALIVE_TIME, 34000, NULL );
}

void CEbenezerDlg::UpdateGameTime()
{
	CUser* pUser = NULL;
	BOOL bKnights = FALSE;

	m_nMin++;

	BattleZoneOpenTimer();	// Check if it's time for the BattleZone to open or end.

	if( m_nMin == 60 ) {
		m_nHour++;
		m_nMin = 0;
		UpdateWeather();
		SetGameTime();
//  갓댐 산타!! >.<
		if (m_bSanta) {
			FlySanta();
		}
//
	}
	if( m_nHour == 24 ) {
		m_nDate++;
		m_nHour = 0;
		bKnights = TRUE;
	}
	if( m_nDate == 31 ) {
		m_nMonth++;
		m_nDate = 1;
	}
	if( m_nMonth == 13 ) {
		m_nYear++;
		m_nMonth = 1;
	}

	// ai status check packet...
	m_sErrorSocketCount++;
	int send_index = 0;
	char pSendBuf[256];		::ZeroMemory(pSendBuf, sizeof(pSendBuf));
	//SetByte(pSendBuf, AG_CHECK_ALIVE_REQ, send_index);
	//Send_AIServer(pSendBuf, send_index);

	// 시간과 날씨 정보를 보낸다..
	::ZeroMemory(pSendBuf, sizeof(pSendBuf));   send_index = 0;
	SetByte(pSendBuf, AG_TIME_WEATHER, send_index);
	SetShort( pSendBuf, m_nYear, send_index );				// time info
	SetShort( pSendBuf, m_nMonth, send_index );
	SetShort( pSendBuf, m_nDate, send_index );
	SetShort( pSendBuf, m_nHour, send_index );
	SetShort( pSendBuf, m_nMin, send_index );
	SetByte( pSendBuf, (BYTE)m_nWeather, send_index );		// weather info
	SetShort( pSendBuf, m_nAmount, send_index );
	Send_AIServer(pSendBuf, send_index);

	if( bKnights )	{
		::ZeroMemory(pSendBuf, sizeof(pSendBuf));   send_index = 0;
		SetByte( pSendBuf, WIZ_KNIGHTS_PROCESS, send_index );
		SetByte( pSendBuf, KNIGHTS_ALLLIST_REQ+0x10, send_index );
		SetByte( pSendBuf, m_nServerNo, send_index );
		m_LoggerSendQueue.PutData( pSendBuf, send_index );
	}
}

void CEbenezerDlg::UpdateWeather()
{
	int weather = 0, result = 0, send_index = 0;
	char send_buff[256];
	memset( send_buff, NULL, 256 );

	result = myrand( 0, 100 );

//	if( result < 5 )
	if( result < 2 )
		weather = WEATHER_SNOW;
//	else if( result < 15 )
	else if( result < 7 )
		weather = WEATHER_RAIN;
	else
		weather = WEATHER_FINE;

	m_nAmount = myrand( 0, 100 );
	if( weather == WEATHER_FINE ) {		// WEATHER_FINE 일때 m_nAmount 는 안개 정도 표시
		if( m_nAmount > 70 )
			m_nAmount = m_nAmount/2;
		else
			m_nAmount = 0;
	}
	m_nWeather = weather;
	SetByte( send_buff, WIZ_WEATHER, send_index );
	SetByte( send_buff, (BYTE)m_nWeather, send_index );
	SetShort( send_buff, m_nAmount, send_index );
	Send_All( send_buff, send_index );
}

void CEbenezerDlg::SetGameTime()
{
	m_Ini.SetInt( "TIMER", "YEAR", m_nYear );
	m_Ini.SetInt( "TIMER", "MONTH", m_nMonth );
	m_Ini.SetInt( "TIMER", "DATE", m_nDate );
	m_Ini.SetInt( "TIMER", "HOUR", m_nHour );
	m_Ini.SetInt( "TIMER", "WEATHER", m_nWeather );
}

void CEbenezerDlg::UserInOutForMe(CUser *pSendUser)
{
	int send_index = 0, buff_index = 0, i=0, j=0, t_count = 0, prev_index = 0;
	C3DMap* pMap = pSendUser->GetMap();
	ASSERT(pMap != NULL);

	int region_x = -1, region_z = -1, user_count = 0, uid = -1;
	char buff[16384], send_buff[49152];
	memset( buff, NULL, 16384 );
	memset( send_buff, NULL, 49152 );

	if( !pSendUser ) return;

	send_index = 3;		// packet command 와 user_count 를 나중에 셋팅한다...
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ;			// CENTER
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH WEST
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ - 1;		// NORTH
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	prev_index = buff_index + send_index;
	if( prev_index >=  49152) {
		TRACE("#### UserInOutForMe - buffer overflow = %d ####\n", prev_index);
		return;
	}
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH EAST
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	prev_index = buff_index + send_index;
	if( prev_index >=  49152) {
		TRACE("#### UserInOutForMe - buffer overflow = %d ####\n", prev_index);
		return;
	}
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ;		// WEST
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	prev_index = buff_index + send_index;
	if( prev_index >=  49152) {
		TRACE("#### UserInOutForMe - buffer overflow = %d ####\n", prev_index);
		return;
	}
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ;		// EAST
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	prev_index = buff_index + send_index;
	if( prev_index >=  49152) {
		TRACE("#### UserInOutForMe - buffer overflow = %d ####\n", prev_index);
		return;
	}
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH WEST
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	prev_index = buff_index + send_index;
	if( prev_index >=  49152) {
		TRACE("#### UserInOutForMe - buffer overflow = %d ####\n", prev_index);
		return;
	}
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ + 1;		// SOUTH
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	prev_index = buff_index + send_index;
	if( prev_index >=  49152) {
		TRACE("#### UserInOutForMe - buffer overflow = %d ####\n", prev_index);
		return;
	}
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 16384 );
	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH EAST
	buff_index = GetRegionUserIn( pMap, region_x, region_z, buff, t_count );
	prev_index = buff_index + send_index;
	if( prev_index >=  49152) {
		TRACE("#### UserInOutForMe - buffer overflow = %d ####\n", prev_index);
		return;
	}
	SetString( send_buff, buff, buff_index, send_index );

	int temp_index = 0;
	SetByte( send_buff, WIZ_REQ_USERIN, temp_index );
	SetShort( send_buff, t_count, temp_index );
	
	pSendUser->SendCompressingPacket( send_buff, send_index );
}

void CEbenezerDlg::RegionUserInOutForMe(CUser *pSendUser)
{
	int send_index = 0, buff_index = 0, i=0, j=0, t_count = 0;
	C3DMap* pMap = pSendUser->GetMap();
	ASSERT(pMap != NULL);
	int region_x = -1, region_z = -1, user_count = 0, uid_sendindex = 0;
	char uid_buff[2048], send_buff[16384];
	memset( uid_buff, NULL, 2048 );
	memset( send_buff, NULL, 16384 );

	if( !pSendUser ) return;

	uid_sendindex = 3;	// packet command 와 user_count 는 나중에 셋팅한다...

	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ;			// CENTER
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );

	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH WEST
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );
	
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ - 1;		// NORTH
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );

	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH EAST
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );

	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ;		// WEST
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );

	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ;		// EAST
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );

	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH WEST
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );

	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ + 1;		// SOUTH
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );
	memset( uid_buff, NULL, 2048 );

	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH EAST
	buff_index = GetRegionUserList( pMap, region_x, region_z, uid_buff, user_count );
	SetString( send_buff, uid_buff, buff_index, uid_sendindex );

	int temp_index = 0;
	SetByte( send_buff, WIZ_REGIONCHANGE, temp_index );
	SetShort( send_buff, user_count, temp_index );

	pSendUser->Send( send_buff, uid_sendindex );
	
	if( user_count > 500 )
		TRACE("Req UserIn: %d \n", user_count);
}

int CEbenezerDlg::GetRegionUserIn( C3DMap *pMap, int region_x, int region_z, char *buff, int &t_count)
{
	int buff_index = 0;

	if (pMap == NULL || region_x < 0 || region_z < 0 || region_x > pMap->GetXRegionMax() || region_z > pMap->GetZRegionMax())
		return 0;

	EnterCriticalSection(&g_region_critical);
	CRegion *pRegion = &pMap->m_ppRegion[region_x][region_z];

	foreach_stlmap (itr, pRegion->m_RegionUserArray)
	{
		CUser *pUser = GetUserPtr(*itr->second);
		if (pUser == NULL || 
			pUser->m_RegionX != region_x || pUser->m_RegionZ != region_z ||
			pUser->GetState() != STATE_GAMESTART)
			continue;

		SetShort(buff, pUser->GetSocketID(), buff_index);
		pUser->GetUserInfo(buff, buff_index);

		t_count++;
	}

	LeaveCriticalSection(&g_region_critical);
	return buff_index;
}

int CEbenezerDlg::GetRegionUserList( C3DMap* pMap, int region_x, int region_z, char *buff, int &t_count)
{
	int buff_index = 0;

	if (pMap == NULL || region_x < 0 || region_z < 0 || region_x > pMap->GetXRegionMax() || region_z > pMap->GetZRegionMax())
		return 0;

	EnterCriticalSection(&g_region_critical);
	CRegion *pRegion = &pMap->m_ppRegion[region_x][region_z];

	foreach_stlmap (itr, pRegion->m_RegionUserArray)
	{
		CUser *pUser = GetUserPtr(*itr->second);
		if (pUser == NULL || 
			pUser->m_RegionX != region_x || pUser->m_RegionZ != region_z ||
			pUser->GetState() != STATE_GAMESTART)
			continue;

		SetShort(buff, pUser->GetSocketID(), buff_index);

		t_count++;
	}

	LeaveCriticalSection(&g_region_critical);
	return buff_index;
}

void CEbenezerDlg::NpcInOutForMe( CUser* pSendUser )
{
	int send_index = 0, buff_index = 0, i=0, j=0, t_count = 0;
	C3DMap* pMap = pSendUser->GetMap();
	ASSERT(pMap != NULL);
	int region_x = -1, region_z = -1, npc_count = 0, nid = -1;
	char buff[8192], send_buff[32768];
	memset( buff, NULL, 8192 );
	memset( send_buff, NULL, 32768 );

	if( !pSendUser ) return;

	send_index = 3;		// packet command 와 user_count 를 나중에 셋팅한다...
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ;			// CENTER
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH WEST
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ - 1;		// NORTH
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH EAST
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ;		// WEST
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ;		// EAST
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH WEST
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ + 1;		// SOUTH
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );
	memset( buff, NULL, 8192 );
	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH EAST
	buff_index = GetRegionNpcIn( pMap, region_x, region_z, buff, t_count );
	SetString( send_buff, buff, buff_index, send_index );

	int temp_index = 0;
	SetByte( send_buff, WIZ_REQ_NPCIN, temp_index );
	SetShort( send_buff, t_count, temp_index );
	
	pSendUser->SendCompressingPacket( send_buff, send_index );
}

int CEbenezerDlg::GetRegionNpcIn(C3DMap *pMap, int region_x, int region_z, char *buff, int &t_count)
{
	if( m_bPointCheckFlag == FALSE)	return 0;	// 포인터 참조하면 안됨
	int buff_index = 0;
	
	if (pMap == NULL
		|| region_x < 0 || region_z < 0 || region_x > pMap->GetXRegionMax() || region_z > pMap->GetZRegionMax())
		return 0;

	EnterCriticalSection( &g_region_critical );

	foreach_stlmap (itr, pMap->m_ppRegion[region_x][region_z].m_RegionNpcArray)
	{
		CNpc *pNpc = m_arNpcArray.GetData(*itr->second);
		if (pNpc == NULL
			|| pNpc->m_sRegion_X != region_x || pNpc->m_sRegion_Z != region_z)
			continue;

		SetShort( buff, pNpc->m_sNid, buff_index );
		pNpc->GetNpcInfo(buff, buff_index);
		t_count++;
	}

	LeaveCriticalSection( &g_region_critical );

	return buff_index;
}

void CEbenezerDlg::RegionNpcInfoForMe(CUser *pSendUser)
{
	int send_index = 0, buff_index = 0, i=0, j=0, t_count = 0;
	C3DMap* pMap = pSendUser->GetMap();
	ASSERT(pMap != NULL);
	int region_x = -1, region_z = -1, npc_count = 0, nid_sendindex = 0;
	char nid_buff[1024], send_buff[8192];
	memset( nid_buff, NULL, 1024 );
	memset( send_buff, NULL, 8192 );
	CString string;

	if( !pSendUser ) return;

	nid_sendindex = 3;	// packet command 와 user_count 는 나중에 셋팅한다...

	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ;			// CENTER
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );

	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH WEST
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );
	
	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ - 1;		// NORTH
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );

	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ - 1;	// NORTH EAST
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );

	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ;		// WEST
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );

	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ;		// EAST
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );

	region_x = pSendUser->m_RegionX - 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH WEST
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );

	region_x = pSendUser->m_RegionX;	region_z = pSendUser->m_RegionZ + 1;		// SOUTH
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );
	memset( nid_buff, NULL, 1024 );

	region_x = pSendUser->m_RegionX + 1;	region_z = pSendUser->m_RegionZ + 1;	// SOUTH EAST
	buff_index = GetRegionNpcList( pMap, region_x, region_z, nid_buff, npc_count );
	SetString( send_buff, nid_buff, buff_index, nid_sendindex );

	int temp_index = 0;
	SetByte( send_buff, WIZ_NPC_REGION, temp_index );
	SetShort( send_buff, npc_count, temp_index );
	pSendUser->Send( send_buff, nid_sendindex );
	
	if( npc_count > 500 )
		TRACE("Req Npc In: %d \n", npc_count);
}

int CEbenezerDlg::GetRegionNpcList(C3DMap *pMap, int region_x, int region_z, char *nid_buff, int &t_count)
{
	if( m_bPointCheckFlag == FALSE)	return 0;

	int buff_index = 0;
	if (pMap == NULL
		|| region_x < 0 || region_z < 0 || region_x > pMap->GetXRegionMax() || region_z > pMap->GetZRegionMax())
		return 0;

	EnterCriticalSection( &g_region_critical );
	foreach_stlmap (itr, pMap->m_ppRegion[region_x][region_z].m_RegionNpcArray)
	{
		CNpc *pNpc = m_arNpcArray.GetData(*itr->second);
		if (pNpc == NULL)
			continue;

		SetShort(nid_buff, pNpc->m_sNid, buff_index);
		t_count++;
	}

	LeaveCriticalSection( &g_region_critical );
	return buff_index;
}

BOOL CEbenezerDlg::PreTranslateMessage(MSG* pMsg) 
{
	char buff[1024];
	memset( buff, 0x00, 1024 );
	char chatstr[256], killstr[256];
	memset( chatstr, 0x00, 256 ); memset( killstr, 0x00, 256 );
	int chatlen = 0, buffindex = 0;
	_ZONE_SERVERINFO *pInfo	= NULL;

	std::string buff2;
//
	BOOL permanent_off = FALSE;
//
	if( pMsg->message == WM_KEYDOWN ) {
		if( pMsg->wParam == VK_RETURN ) {
			m_AnnounceEdit.GetWindowText( chatstr, 256 );
			UpdateData(TRUE);
			chatlen = strlen(chatstr);
			if( chatlen == 0 )
				return TRUE;

			m_AnnounceEdit.SetWindowText("");
			UpdateData(FALSE);

			if( _strnicmp( "/kill", chatstr, 5 ) == 0 ) {
				strcpy( killstr, chatstr+6);
				KillUser( killstr );
				return TRUE;
			}
			if( _strnicmp( "/Open", chatstr, 5 ) == 0 ) {
				BattleZoneOpen( BATTLEZONE_OPEN );
				return TRUE;
			}
			if( _strnicmp( "/snowopen", chatstr, 9 ) == 0 ) {
				BattleZoneOpen( SNOW_BATTLEZONE_OPEN );
				return TRUE;
			}
			if( _strnicmp( "/Close", chatstr, 6 ) == 0 ) {
				m_byBanishFlag = 1;
				//WithdrawUserOut();
				return TRUE;
			}
			if( _strnicmp( "/down", chatstr, 5 ) == 0 ) {
				g_serverdown_flag = TRUE;
				::SuspendThread( m_Iocport.m_hAcceptThread );
				int users = KickOutAllUsers();
				char output[128];
				sprintf_s(output, 128, "Server shutdown, %d users kicked out.", users);
				m_StatusList.AddString(output);
				return TRUE;
			}
			if( _strnicmp( "/pause", chatstr, 6 ) == 0 ) {
				g_serverdown_flag = TRUE;
				::SuspendThread( m_Iocport.m_hAcceptThread );
				m_StatusList.AddString("Server no longer accepting connections");
				return TRUE;
			}
			if( _strnicmp( "/resume", chatstr, 7 ) == 0 ) {
				g_serverdown_flag = FALSE;
				::ResumeThread( m_Iocport.m_hAcceptThread );
				m_StatusList.AddString("Server accepting connections");
				return TRUE;
			}
			if( _strnicmp( "/discount", chatstr, 9 ) == 0 ) {
				m_sDiscount = 1;
				return TRUE;
			}
			if( _strnicmp( "/alldiscount", chatstr, 12 ) == 0 ) {
				m_sDiscount = 2;
				return TRUE;
			}
			if( _strnicmp( "/undiscount", chatstr, 11 ) == 0 ) {
				m_sDiscount = 0;
				return TRUE;
			}
// 비러머글 남는 공지 --;
			if( _strnicmp( "/permanent", chatstr, 10 ) == 0 ) {
				m_bPermanentChatMode = TRUE;
				m_bPermanentChatFlag = TRUE;
				return TRUE;
			}
			if( _strnicmp( "/captain", chatstr, 8 ) == 0 ) {
				LoadKnightsRankTable();				// captain 
				return TRUE;
			}
			
			if( _strnicmp( "/offpermanent", chatstr, 13 ) == 0 ) {
				m_bPermanentChatMode = FALSE;
				m_bPermanentChatFlag = FALSE;
				permanent_off = TRUE;
//				return TRUE;	//이것은 고의적으로 TRUE를 뺐었음
			}			
			if( _strnicmp( "/santa", chatstr, 6 ) == 0 ) {
				m_bSanta = TRUE;			// Make Motherfucking Santa Claus FLY!!!
				return TRUE;
			}

			if( _strnicmp( "/offsanta", chatstr, 9 ) == 0 ) {
				m_bSanta = FALSE;			// SHOOT DOWN Motherfucking Santa Claus!!!
				return TRUE;
			}			

			char finalstr[512]; memset(finalstr, NULL, 512);
			if (m_bPermanentChatFlag)
				_snprintf(finalstr, sizeof(finalstr), "- %s -", chatstr);
			else
				_snprintf(finalstr, sizeof(finalstr), GetServerResource(IDP_ANNOUNCEMENT), chatstr);

			SetByte( buff, WIZ_CHAT, buffindex );

			if (permanent_off)
				SetByte( buff, END_PERMANENT_CHAT, buffindex );
			else if (!m_bPermanentChatFlag)
				SetByte( buff, PUBLIC_CHAT, buffindex );
			else 
			{
				SetByte( buff, PERMANENT_CHAT, buffindex );
				strcpy(m_strPermanentChat, finalstr);
				m_bPermanentChatFlag = FALSE;
			}
//
			SetByte( buff, 0x01, buffindex );		// nation
			SetShort( buff, -1, buffindex );		// sid
			SetKOString( buff, finalstr, buffindex );
			Send_All( buff, buffindex );

			buffindex = 0;
			memset( buff, 0x00, 1024 );
			SetByte( buff, STS_CHAT, buffindex );
			SetKOString( buff, finalstr, buffindex );

			foreach_stlmap (itr, m_ServerArray)
				if (itr->second && itr->second->sServerNo == m_nServerNo)
					m_pUdpSocket->SendUDPPacket(itr->second->strServerIP, buff, buffindex);

			return TRUE;
		}

		if( pMsg->wParam == VK_ESCAPE )
			return TRUE;
	}
	
	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CEbenezerDlg::LoadNoticeData()
{
	CString ProgPath = GetProgPath();
	CString NoticePath = ProgPath + "Notice.txt";
	CString buff;
	CStdioFile txt_file;
	int count = 0;

	if (!txt_file.Open(NoticePath, CFile::modeRead)) {
		DEBUG_LOG("cannot open Notice.txt!!");
		return FALSE;
	}

	while( txt_file.ReadString(buff) ) {
		if( count > 19 )
		{
			AfxMessageBox("Too many lines in Notice.txt");
			txt_file.Close();
			return FALSE;
		}
		strcpy( m_ppNotice[count], (char*)(LPCTSTR)buff );
		count++;
	}

	txt_file.Close();

	return TRUE;
}

BOOL CEbenezerDlg::LoadBlockNameList()
{
	CString NoticePath = GetProgPath() + "BlockWord.txt"; // we should rename this probably, but let's stick with their name for now
	CString buff;
	CStdioFile file;

	if (!file.Open(NoticePath, CFile::modeRead))
	{
		DEBUG_LOG("Cannot open BlockWord.txt!");
		return FALSE;
	}

	while (file.ReadString(buff))
	{
		buff.MakeUpper();
		m_BlockNameArray.push_back(buff);
	}

	file.Close();

	return TRUE;
}

void CEbenezerDlg::SendAllUserInfo()
{
	int send_index = 0;
	char send_buff[2048];		::ZeroMemory(send_buff, sizeof(send_buff));

	SetByte(send_buff, AG_SERVER_INFO, send_index );
	SetByte(send_buff, SERVER_INFO_START, send_index );
	Send_AIServer(send_buff, send_index );

	int count = 0;
	send_index = 2;
	::ZeroMemory(send_buff, sizeof(send_buff));
	int send_count = 0;
	int send_tot = 0;
	int tot = 20;

	for (int i = 0; i < MAX_USER; i++)
	{
		CUser * pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL)
			continue;

		pUser->SendUserInfo(send_buff, send_index);
		count++;
		if(count == tot)	{
			SetByte(send_buff, AG_USER_INFO_ALL, send_count );
			SetByte(send_buff, (BYTE)count, send_count );
			Send_AIServer(send_buff, send_index);
			send_index = 2;
			send_count = 0;
			count = 0;
			send_tot++;
			::ZeroMemory(send_buff, sizeof(send_buff));
		}
	}	

	if(count != 0 && count < (tot-1) )	{
		send_count = 0;
		SetByte(send_buff, AG_USER_INFO_ALL, send_count );
		SetByte(send_buff, (BYTE)count, send_count );
		Send_AIServer(send_buff, send_index );
		send_tot++;
		//TRACE("AllNpcInfo - send_count=%d, count=%d\n", send_tot, count);
		//Sleep(1);
	}

	// 파티에 대한 정보도 보내도록 한다....
	_PARTY_GROUP* pParty = NULL;
	
	EnterCriticalSection( &g_region_critical );

	for(int i=0; i<m_PartyArray.GetSize(); i++)	{
		pParty = m_PartyArray.GetData( i );
		if( !pParty ) return;
		send_index = 0;
		::ZeroMemory(send_buff, sizeof(send_buff));
		SetByte(send_buff, AG_PARTY_INFO_ALL, send_index );
		SetShort(send_buff, i, send_index );					// 파티 번호
		//if( i == pParty->wIndex )
		for( int j=0; j<8; j++ ) {
			SetShort(send_buff, pParty->uid[j], send_index );				// 유저 번호
			//SetShort(send_buff, pParty->sHp[j], send_index );				// HP
			//SetByte(send_buff, pParty->bLevel[j], send_index );				// Level
			//SetShort(send_buff, pParty->sClass[j], send_index );			// Class
		}

		Send_AIServer(send_buff, send_index );
	}

	LeaveCriticalSection( &g_region_critical );

	send_index = 0;
	::ZeroMemory(send_buff, sizeof(send_buff));
	SetByte(send_buff, AG_SERVER_INFO, send_index );
	SetByte(send_buff, SERVER_INFO_END, send_index );
	Send_AIServer(send_buff, send_index );

	TRACE("** SendAllUserInfo() **\n");
}

// sungyong 2002. 05. 23
void CEbenezerDlg::DeleteAllNpcList(int flag)
{
	if(m_bServerCheckFlag == FALSE)	return;
	if(m_bPointCheckFlag == TRUE)	{
		m_bPointCheckFlag = FALSE;
		TRACE("*** Point 참조 하면 안되여 *** \n");
		return;
	}

	DEBUG_LOG("[Monster Point Delete]");
	TRACE("*** DeleteAllNpcList - Start *** \n");

	// region Npc Array Delete
	foreach_stlmap (itr, m_ZoneArray)
	{
		C3DMap *pMap = itr->second;
		if (pMap == NULL)
			continue;

		for (int i = 0; i < pMap->GetXRegionMax(); i++)
		{
			for (int j = 0; j<pMap->GetZRegionMax(); j++)
			{
				if (!pMap->m_ppRegion[i][j].m_RegionNpcArray.IsEmpty())
					pMap->m_ppRegion[i][j].m_RegionNpcArray.DeleteAllData();
			}
		}
	}

	// Npc Array Delete
	if (!m_arNpcArray.IsEmpty())
		m_arNpcArray.DeleteAllData();

	m_bServerCheckFlag = FALSE;

	TRACE("*** DeleteAllNpcList - End *** \n");
}
// ~sungyong 2002. 05. 23

void CEbenezerDlg::KillUser(const char *strbuff)
{
	if (strbuff[0] == 0 || strlen(strbuff) > MAX_ID_SIZE )
		return;

	CUser* pUser = GetUserPtr(strbuff, TYPE_CHARACTER);
	if (pUser != NULL)
		pUser->Close();
}

CNpc*  CEbenezerDlg::GetNpcPtr( int sid, int cur_zone )
{
	if( m_bPointCheckFlag == FALSE)	return NULL;

	CNpc* pNpc = NULL;

	int nSize = m_arNpcArray.GetSize();

	for( int i = 0; i < nSize; i++)	{
		pNpc = m_arNpcArray.GetData( i+NPC_BAND );
		if (pNpc == NULL || pNpc->getZoneID() != cur_zone
			|| pNpc->m_sPid != sid) // this isn't a typo (unless it's mgame's typo).
			continue;

		return pNpc;
	}

	return NULL;
}

void CEbenezerDlg::WithdrawUserOut()
{
	for (int i = 0; i < MAX_USER; i++)
	{
		CUser *pUser = GetUnsafeUserPtr(i);
		if (pUser != NULL && pUser->GetState() == STATE_GAMESTART
			&& pUser->m_pUserData->m_bZone == pUser->m_pUserData->m_bNation
			&& pUser->GetMap() != NULL)
			pUser->ZoneChange(pUser->GetMap()->m_nZoneNumber, pUser->GetMap()->m_fInitX, pUser->GetMap()->m_fInitZ);
	}
}

void CEbenezerDlg::AliveUserCheck()
{
	float currenttime = TimeGet();

	for (int i = 0; i < MAX_USER; i++)
	{
		CUser * pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL || pUser->GetState() != STATE_GAMESTART) 
			continue;

		for ( int k = 0 ; k < MAX_TYPE3_REPEAT ; k++ ) {
			if( (currenttime - pUser->m_fHPLastTime[k]) > 300 ) {
				pUser->Close();
				break;
			}
		}
	}
}
/////// BATTLEZONE RELATED by Yookozuna 2002.6.18 /////////////////
void CEbenezerDlg::BattleZoneOpenTimer()
{
	CTime cur = CTime::GetCurrentTime();

	// sungyong modify
	int nWeek = cur.GetDayOfWeek();
	int nTime = cur.GetHour();
	char send_buff[128];	memset(send_buff, 0x00, 128);
	int send_index = 0, loser_nation = 0, snow_battle = 0;
	CUser* pKarusUser = NULL;
	CUser* pElmoUser = NULL;

/*	if( m_byBattleOpen == NO_BATTLE )	{	// When Battlezone is closed, open it!
		if( nWeek == m_nBattleZoneOpenWeek && nTime == m_nBattleZoneOpenHourStart )	{	// 수요일, 20시에 전쟁존 open
			TRACE("전쟁 자동 시작 - week=%d, time=%d\n", nWeek, nTime);
			BattleZoneOpen(BATTLEZONE_OPEN);
//			KickOutZoneUsers(ZONE_FRONTIER);	// Kick out users in frontier zone.
		}
	}
	else {	  // When Battlezone is open, close it!
		if( nWeek == (m_nBattleZoneOpenWeek+1) && nTime == m_nBattleZoneOpenHourEnd )	{	// 목요일, 0시에 전쟁존 close
			TRACE("전쟁 자동 종료 - week=%d, time=%d\n", nWeek, nTime);
			m_byBanishFlag = 1;
		}
	}	*/

	if( m_byBattleOpen == NATION_BATTLE )		BattleZoneCurrentUsers();

	if( m_byBanishFlag == 1 )	{		
		if( m_sBanishDelay == 0 )	{
			m_byBattleOpen = NO_BATTLE;
			m_byKarusOpenFlag = 0;		// 카루스 땅으로 넘어갈 수 없도록
			m_byElmoradOpenFlag = 0;	// 엘모 땅으로 넘어갈 수 없도록
			memset( m_strKarusCaptain, 0x00, MAX_ID_SIZE+1 );
			memset( m_strElmoradCaptain, 0x00, MAX_ID_SIZE+1 );
			TRACE("전쟁 종료 0단계\n");
			if( m_nServerNo == KARUS )	{
				memset(send_buff, 0x00, 128);		send_index = 0;
				SetByte( send_buff, UDP_BATTLE_EVENT_PACKET, send_index );
				SetByte( send_buff, BATTLE_EVENT_KILL_USER, send_index );
				SetByte( send_buff, 1, send_index );						// karus의 정보 전송
				SetShort( send_buff, m_sKarusDead, send_index );
				SetShort( send_buff, m_sElmoradDead, send_index );
				Send_UDP_All( send_buff, send_index );
			}
		}

		m_sBanishDelay++;

		if( m_sBanishDelay == 3 )	{
			if( m_byOldBattleOpen == SNOW_BATTLE )	{		// 눈싸움 전쟁
				if( m_sKarusDead > m_sElmoradDead )	{
					m_bVictory = ELMORAD;
					loser_nation = KARUS;
				}
				else if( m_sKarusDead < m_sElmoradDead )	{
					m_bVictory = KARUS;
					loser_nation = ELMORAD;
				}
				else if( m_sKarusDead == m_sElmoradDead )	{
					m_bVictory = 0;
				}
			}

			if( m_bVictory == 0 )	BattleZoneOpen( BATTLEZONE_CLOSE );
			else if( m_bVictory )	{
				if( m_bVictory == KARUS )		 loser_nation = ELMORAD;
				else if( m_bVictory == ELMORAD ) loser_nation = KARUS;
				Announcement( DECLARE_WINNER, m_bVictory );
				Announcement( DECLARE_LOSER, loser_nation );
			}
			TRACE("전쟁 종료 1단계, m_bVictory=%d\n", m_bVictory);
		}
		else if( m_sBanishDelay == 8 )	{
			Announcement(DECLARE_BAN);
		}
		else if( m_sBanishDelay == 10 )	{
			TRACE("전쟁 종료 2단계 - 모든 유저 자기 국가로 가 \n");
			BanishLosers();
		}
		else if( m_sBanishDelay == 20 )	{
			TRACE("전쟁 종료 3단계 - 초기화 해주세여 \n");
			SetByte( send_buff, AG_BATTLE_EVENT, send_index );
			SetByte( send_buff, BATTLE_EVENT_OPEN, send_index );
			SetByte( send_buff, BATTLEZONE_CLOSE, send_index );
			Send_AIServer(send_buff, send_index );
			ResetBattleZone();
		}
	}

	// ~
}

void CEbenezerDlg::BattleZoneOpen( int nType )
{
	int send_index = 0;
	char send_buff[1024]; memset( send_buff, NULL, 1024 );
	char strLogFile[100]; memset( strLogFile, NULL, 100 );
	CTime time = CTime::GetCurrentTime();

	if( nType == BATTLEZONE_OPEN ) {				// Open battlezone.
		m_byBattleOpen = NATION_BATTLE;	
		m_byOldBattleOpen = NATION_BATTLE;
	}
	else if( nType == SNOW_BATTLEZONE_OPEN ) {		// Open snow battlezone.
		m_byBattleOpen = SNOW_BATTLE;	
		m_byOldBattleOpen = SNOW_BATTLE;
		wsprintf(strLogFile, "EventLog-%d-%d-%d.txt", time.GetYear(), time.GetMonth(), time.GetDay());
		m_EvnetLogFile.Open( strLogFile, CFile::modeWrite | CFile::modeCreate | CFile::modeNoTruncate | CFile::shareDenyNone );
		m_EvnetLogFile.SeekToEnd();
	}
	else if( nType == BATTLEZONE_CLOSE )	{		// battle close
		m_byBattleOpen = NO_BATTLE;
		Announcement(BATTLEZONE_CLOSE);
	}
	else return;

	Announcement(nType);	// Send an announcement out that the battlezone is open/closed.
//
	KickOutZoneUsers(ZONE_FRONTIER);
//
	memset( send_buff, NULL, 1024 );
	SetByte( send_buff, AG_BATTLE_EVENT, send_index );		// Send packet to AI server.
	SetByte( send_buff, BATTLE_EVENT_OPEN, send_index );
	SetByte( send_buff, nType, send_index );
	Send_AIServer(send_buff, send_index );
}

void CEbenezerDlg::BattleZoneVictoryCheck()
{	
	if (m_bKarusFlag >= NUM_FLAG_VICTORY) {		// WINNER DECLARATION PROCEDURE !!!
		m_bVictory = KARUS ;
	}
	else if (m_bElmoradFlag >= NUM_FLAG_VICTORY) {
		m_bVictory = ELMORAD ;
	}
	else return;

	Announcement(DECLARE_WINNER);

	for (int i = 0 ; i < MAX_USER ; i++) {		// GOLD DISTRIBUTION PROCEDURE FOR WINNERS !!!
		CUser* pTUser = GetUnsafeUserPtr(i);
		if (pTUser == NULL) continue;
		
		if (pTUser->m_pUserData->m_bNation == m_bVictory) {
			if ( pTUser->m_pUserData->m_bZone == pTUser->m_pUserData->m_bNation ) {		// Zone Check!
				pTUser->m_pUserData->m_iGold += AWARD_GOLD;	// Target is in the area.
			}
		}
	}		
}

void CEbenezerDlg::BanishLosers()
{
	int zoneindex = -1, send_index = 0;;
	C3DMap* pMap = NULL;
	char send_buff[256];	memset( send_buff, 0x00, 256 );


	for (int i = 0 ; i < MAX_USER ; i++) {				// EVACUATION PROCEDURE FOR LOSERS !!!		
		CUser *pTUser = GetUnsafeUserPtr(i); 
		if (pTUser == NULL) continue;	
		if ( pTUser->m_pUserData->m_bFame == COMMAND_CAPTAIN )	{
			pTUser->m_pUserData->m_bFame = CHIEF;
			send_index = 0;		memset( send_buff, 0x00, 256 );
			SetByte( send_buff, WIZ_AUTHORITY_CHANGE, send_index );
			SetByte( send_buff, COMMAND_AUTHORITY, send_index );
			SetShort( send_buff, pTUser->GetSocketID(), send_index );
			SetByte( send_buff, pTUser->m_pUserData->m_bFame, send_index );
			pTUser->Send( send_buff, send_index );
		}
		if (pTUser->m_pUserData->m_bZone != pTUser->m_pUserData->m_bNation ) {
			pTUser->KickOutZoneUser(TRUE);
		}
	}
}

void CEbenezerDlg::ResetBattleZone()
{
	if( m_byOldBattleOpen == SNOW_BATTLE )	{
		if(m_EvnetLogFile.m_hFile != CFile::hFileNull) m_EvnetLogFile.Close();
		TRACE("Event Log close\n");
	}

	m_bVictory = 0;
	m_byBanishFlag = 0;
	m_sBanishDelay = 0;
	m_bKarusFlag = 0,
	m_bElmoradFlag = 0;
	m_byKarusOpenFlag = m_byElmoradOpenFlag = 0;
	m_byBattleOpen = NO_BATTLE;
	m_byOldBattleOpen = NO_BATTLE;
	m_sKarusDead = m_sElmoradDead = 0;
	m_byBattleSave = 0;
	m_sKarusCount = 0;
	m_sElmoradCount = 0;
	// REMEMBER TO MAKE ALL FLAGS AND LEVERS NEUTRAL AGAIN!!!!!!!!!!
}

void CEbenezerDlg::Announcement(BYTE type, int nation, int chat_type)
{
	int send_index = 0;

	char chatstr[1024]; memset( chatstr, NULL, 1024 );
	char finalstr[1024]; memset( finalstr, NULL, 1024 );
	char send_buff[1024]; memset( send_buff, NULL, 1024 );
	
	switch(type) {
		case BATTLEZONE_OPEN:
		case SNOW_BATTLEZONE_OPEN:
			_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDP_BATTLEZONE_OPEN));
			break;

		case DECLARE_WINNER:
			if (m_bVictory == KARUS)
				_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDP_KARUS_VICTORY), m_sElmoradDead, m_sKarusDead);
			else if (m_bVictory == ELMORAD)
				_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDP_ELMORAD_VICTORY), m_sKarusDead, m_sElmoradDead);
			else 
				return;
			break;
		case DECLARE_LOSER:
			if (m_bVictory == KARUS)
				_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_ELMORAD_LOSER), m_sKarusDead, m_sElmoradDead);
			else if (m_bVictory == ELMORAD)
				_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_KARUS_LOSER), m_sElmoradDead, m_sKarusDead);
			else 
				return;
			break;

		case DECLARE_BAN:
			_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_BANISH_USER));
			break;
		case BATTLEZONE_CLOSE:
			_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_BATTLE_CLOSE));
			break;
		case KARUS_CAPTAIN_NOTIFY:
			_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_KARUS_CAPTAIN), m_strKarusCaptain);
			break;
		case ELMORAD_CAPTAIN_NOTIFY:
			_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_ELMO_CAPTAIN), m_strElmoradCaptain);
			break;
		case KARUS_CAPTAIN_DEPRIVE_NOTIFY:
			_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_KARUS_CAPTAIN_DEPRIVE), m_strKarusCaptain);
			break;
		case ELMORAD_CAPTAIN_DEPRIVE_NOTIFY:
			_snprintf(chatstr, sizeof(chatstr), GetServerResource(IDS_ELMO_CAPTAIN_DEPRIVE), m_strElmoradCaptain);
			break;
	}

	_snprintf(finalstr, sizeof(finalstr), GetServerResource(IDP_ANNOUNCEMENT), chatstr);
	SetByte( send_buff, WIZ_CHAT, send_index );
	SetByte( send_buff, chat_type, send_index );
	SetByte( send_buff, 1, send_index );
	SetShort( send_buff, -1, send_index );
	SetKOString(send_buff, finalstr, send_index);

	Send_All(send_buff, send_index, NULL, nation);
}

BOOL CEbenezerDlg::LoadHomeTable()
{
	CHomeSet	HomeSet(&m_GameDB);

	if( !HomeSet.Open() ) {
		AfxMessageBox(_T("Home Data Open Fail!"));
		return FALSE;
	}
	if(HomeSet.IsBOF() || HomeSet.IsEOF()) {
		AfxMessageBox(_T("Home Data Empty!"));
		return FALSE;
	}

	HomeSet.MoveFirst();

	while( !HomeSet.IsEOF() )
	{
		_HOME_INFO* pHomeInfo = new _HOME_INFO;
				
		pHomeInfo->bNation = HomeSet.m_Nation ;

		pHomeInfo->KarusZoneX = HomeSet.m_KarusZoneX;
		pHomeInfo->KarusZoneZ = HomeSet.m_KarusZoneZ;
		pHomeInfo->KarusZoneLX = HomeSet.m_KarusZoneLX;
		pHomeInfo->KarusZoneLZ = HomeSet.m_KarusZoneLZ;

		pHomeInfo->ElmoZoneX = HomeSet.m_ElmoZoneX;
		pHomeInfo->ElmoZoneZ = HomeSet.m_ElmoZoneZ;
		pHomeInfo->ElmoZoneLX = HomeSet.m_ElmoZoneLX;
		pHomeInfo->ElmoZoneLZ = HomeSet.m_ElmoZoneLZ;

		pHomeInfo->FreeZoneX = HomeSet.m_FreeZoneX;
		pHomeInfo->FreeZoneZ = HomeSet.m_FreeZoneZ;
		pHomeInfo->FreeZoneLX = HomeSet.m_FreeZoneLX;
		pHomeInfo->FreeZoneLZ = HomeSet.m_FreeZoneLZ;
//
		pHomeInfo->BattleZoneX = HomeSet.m_BattleZoneX;
		pHomeInfo->BattleZoneZ = HomeSet.m_BattleZoneZ;
		pHomeInfo->BattleZoneLX = HomeSet.m_BattleZoneLX;
		pHomeInfo->BattleZoneLZ = HomeSet.m_BattleZoneLZ;
//
		if( !m_HomeArray.PutData(pHomeInfo->bNation, pHomeInfo) ) {
			TRACE("Home Info PutData Fail - %d\n", pHomeInfo->bNation );
			delete pHomeInfo;
			pHomeInfo = NULL;
		}

		HomeSet.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadStartPositionTable()
{
	CStartPositionSet StartPositionSet(&m_GameDB);

	if (!StartPositionSet.Open())
	{
		AfxMessageBox(_T("Could not open START_POSITION table."));
		return FALSE;
	}

	if (StartPositionSet.IsBOF() || StartPositionSet.IsEOF())
	{
		AfxMessageBox(_T("_START_POSITION table empty!"));
		return FALSE;
	}

	StartPositionSet.MoveFirst();
	while (!StartPositionSet.IsEOF())
	{
		_START_POSITION* pData = new _START_POSITION;
		pData->ZoneID			= StartPositionSet.m_sZoneID;
		pData->sKarusX			= StartPositionSet.m_sKarusX;
		pData->sKarusZ			= StartPositionSet.m_sKarusZ;
		pData->sElmoradX		= StartPositionSet.m_sElmoradX;
		pData->sElmoradZ		= StartPositionSet.m_sElmoradZ;
		pData->sKarusGateX		= StartPositionSet.m_sKarusGateX;
		pData->sKarusGateZ		= StartPositionSet.m_sKarusGateZ;
		pData->sElmoradGateX	= StartPositionSet.m_sElmoradGateX;
		pData->sElmoradGateZ	= StartPositionSet.m_sElmoradGateZ;
		pData->bRangeX			= StartPositionSet.m_bRangeX;
		pData->bRangeZ			= StartPositionSet.m_bRangeZ;

		if (!m_StartPositionArray.PutData(pData->ZoneID, pData))
		{
			TRACE("Could not add zone %d to the starting position array\n", pData->ZoneID);
			delete pData;
		}

		StartPositionSet.MoveNext();
	}

	return TRUE;
}


BOOL CEbenezerDlg::LoadAllKnights()
{
	CKnightsSet	KnightsSet(&m_GameDB);
	CString strKnightsName, strChief, strViceChief_1, strViceChief_2, strViceChief_3;
	int i=0;

	if( !KnightsSet.Open() ) {
		AfxMessageBox(_T("Knights Open Fail!"));
		return FALSE;
	}
	if(KnightsSet.IsBOF() || KnightsSet.IsEOF()) {
	//	AfxMessageBox(_T("Knights Data Empty!"));
		return TRUE;
	}

	KnightsSet.MoveFirst();

	while( !KnightsSet.IsEOF() )
	{
		// sungyong ,, zone server : 카루스와 전쟁존을 합치므로 인해서,,
	/*	if( m_nServerNo == KARUS )	{
			if( KnightsSet.m_IDNum < 15000 )	{
				CKnights* pKnights = new _KNIGHTS;
				pKnights->sIndex = KnightsSet.m_IDNum;
				pKnights->bFlag = KnightsSet.m_Flag;
				pKnights->bNation = KnightsSet.m_Nation;
				strKnightsName = KnightsSet.m_IDName;
				strKnightsName.TrimRight();
				strChief = KnightsSet.m_Chief;
				strChief.TrimRight();
				strViceChief_1 = KnightsSet.m_ViceChief_1;
				strViceChief_1.TrimRight();
				strViceChief_2 = KnightsSet.m_ViceChief_2;
				strViceChief_2.TrimRight();
				strViceChief_3 = KnightsSet.m_ViceChief_3;
				strViceChief_3.TrimRight();

				strcpy( pKnights->strName, (char*)(LPCTSTR)strKnightsName );
				pKnights->sMembers = KnightsSet.m_Members;
				strcpy( pKnights->strChief, (char*)(LPCTSTR)strChief );
				strcpy( pKnights->strViceChief_1, (char*)(LPCTSTR)strViceChief_1 );
				strcpy( pKnights->strViceChief_2, (char*)(LPCTSTR)strViceChief_2 );
				strcpy( pKnights->strViceChief_3, (char*)(LPCTSTR)strViceChief_3 );
				pKnights->nMoney = atoi((const char*)(LPCTSTR)KnightsSet.m_Gold);
				pKnights->sDomination = KnightsSet.m_Domination;
				pKnights->nPoints = KnightsSet.m_Points;
				pKnights->bGrade = GetKnightsGrade( KnightsSet.m_Points );
				pKnights->bRanking = KnightsSet.m_Ranking;

				for(i=0; i<MAX_CLAN; i++)	{
					pKnights->arKnightsUser[i].byUsed = 0;
					strcpy(pKnights->arKnightsUser[i].strUserName, "");
				}	

				if( !m_KnightsArray.PutData(pKnights->sIndex, pKnights) ) {
					TRACE("Knights PutData Fail - %d\n", pKnights->sIndex);
					delete pKnights;
					pKnights = NULL;
				}
			}
		}
		else if( m_nServerNo == ELMORAD )	{	*/
	/*	if( m_nServerNo == ELMORAD )	{
			if( KnightsSet.m_IDNum >= 15000 && KnightsSet.m_IDNum < 30000 )	{
				CKnights* pKnights = new CKnights;
				pKnights->InitializeValue();

				pKnights->m_sIndex = KnightsSet.m_IDNum;
				pKnights->m_byFlag = KnightsSet.m_Flag;
				pKnights->m_byNation = KnightsSet.m_Nation;
				//strcpy( pKnights->strName, (char*)(LPCTSTR)KnightsSet.m_IDName );
				strKnightsName = KnightsSet.m_IDName;
				strKnightsName.TrimRight();
				strChief = KnightsSet.m_Chief;
				strChief.TrimRight();
				strViceChief_1 = KnightsSet.m_ViceChief_1;
				strViceChief_1.TrimRight();
				strViceChief_2 = KnightsSet.m_ViceChief_2;
				strViceChief_2.TrimRight();
				strViceChief_3 = KnightsSet.m_ViceChief_3;
				strViceChief_3.TrimRight();

				strcpy( pKnights->m_strName, (char*)(LPCTSTR)strKnightsName );
				pKnights->m_sMembers = KnightsSet.m_Members;
				strcpy( pKnights->m_strChief, (char*)(LPCTSTR)strChief );
				strcpy( pKnights->m_strViceChief_1, (char*)(LPCTSTR)strViceChief_1 );
				strcpy( pKnights->m_strViceChief_2, (char*)(LPCTSTR)strViceChief_2 );
				strcpy( pKnights->m_strViceChief_3, (char*)(LPCTSTR)strViceChief_3 );
				pKnights->m_nMoney = atoi((const char*)(LPCTSTR)KnightsSet.m_Gold);
				pKnights->m_sDomination = KnightsSet.m_Domination;
				pKnights->m_nPoints = KnightsSet.m_Points;
				pKnights->m_byGrade = GetKnightsGrade( KnightsSet.m_Points );
				pKnights->m_byRanking = KnightsSet.m_Ranking;

				for(i=0; i<MAX_CLAN; i++)	{
					pKnights->m_arKnightsUser[i].byUsed = 0;
					strcpy(pKnights->m_arKnightsUser[i].strUserName, "");
				}	

				if( !m_KnightsArray.PutData(pKnights->m_sIndex, pKnights) ) {
					TRACE("Knights PutData Fail - %d\n", pKnights->m_sIndex);
					delete pKnights;
					pKnights = NULL;
				}

				//TRACE("knightindex = %d\n", IDNum);

			}
		}
		else	*/
		{
			CKnights* pKnights = new CKnights;
			pKnights->InitializeValue();

			pKnights->m_sIndex = KnightsSet.m_IDNum;
			pKnights->m_byFlag = KnightsSet.m_Flag;
			pKnights->m_byNation = KnightsSet.m_Nation;
			strKnightsName = KnightsSet.m_IDName;
			strKnightsName.TrimRight();
			strChief = KnightsSet.m_Chief;
			strChief.TrimRight();
			strViceChief_1 = KnightsSet.m_ViceChief_1;
			strViceChief_1.TrimRight();
			strViceChief_2 = KnightsSet.m_ViceChief_2;
			strViceChief_2.TrimRight();
			strViceChief_3 = KnightsSet.m_ViceChief_3;
			strViceChief_3.TrimRight();

			strcpy( pKnights->m_strName, (char*)(LPCTSTR)strKnightsName );
			pKnights->m_sMembers = KnightsSet.m_Members;
			strcpy( pKnights->m_strChief, (char*)(LPCTSTR)strChief );
			strcpy( pKnights->m_strViceChief_1, (char*)(LPCTSTR)strViceChief_1 );
			strcpy( pKnights->m_strViceChief_2, (char*)(LPCTSTR)strViceChief_2 );
			strcpy( pKnights->m_strViceChief_3, (char*)(LPCTSTR)strViceChief_3 );
			pKnights->m_nMoney = atoi((const char*)(LPCTSTR)KnightsSet.m_Gold);
			pKnights->m_sDomination = KnightsSet.m_Domination;
			pKnights->m_nPoints = KnightsSet.m_Points;
			pKnights->m_byGrade = GetKnightsGrade( KnightsSet.m_Points );
			pKnights->m_byRanking = KnightsSet.m_Ranking;

			for(i=0; i<MAX_CLAN; i++)	{
				pKnights->m_arKnightsUser[i].byUsed = 0;
				strcpy(pKnights->m_arKnightsUser[i].strUserName, "");
			}	

			if( !m_KnightsArray.PutData(pKnights->m_sIndex, pKnights) ) {
				TRACE("Knights PutData Fail - %d\n", pKnights->m_sIndex);
				delete pKnights;
				pKnights = NULL;
			}
		}

		KnightsSet.MoveNext();
	}

	return TRUE;
}

BOOL CEbenezerDlg::LoadAllKnightsUserData()
{
	CKnightsUserSet	KnightsSet(&m_GameDB);
	CString strUserName;
	int iFame=0, iLevel=0, iClass=0;

	if( !KnightsSet.Open() ) {
		AfxMessageBox(_T("KnightsUser Open Fail!"));
		return FALSE;
	}
	if(KnightsSet.IsBOF() || KnightsSet.IsEOF()) {
	//	AfxMessageBox(_T("KnightsUser Data Empty!"));
		return TRUE;
	}

	KnightsSet.MoveFirst();

	while( !KnightsSet.IsEOF() )
	{
		// sungyong ,, zone server : 카루스와 전쟁존을 합치므로 인해서,,
	/*	if( m_nServerNo == KARUS )	{
			if( KnightsSet.m_sIDNum < 15000 )	{
				strUserName = KnightsSet.m_strUserID;
				strUserName.TrimRight();
				m_KnightsManager.AddKnightsUser( KnightsSet.m_sIDNum, (char*)(LPCTSTR) strUserName );
			}
		}
		else if( m_nServerNo == ELMORAD )	{	*/
	/*	if( m_nServerNo == ELMORAD )	{
			if( KnightsSet.m_sIDNum >= 15000 && KnightsSet.m_sIDNum < 30000 )	{
				strUserName = KnightsSet.m_strUserID;
				strUserName.TrimRight();
				m_KnightsManager.AddKnightsUser( KnightsSet.m_sIDNum, (char*)(LPCTSTR) strUserName );
			}
		}
		else	*/
		{
			strUserName = KnightsSet.m_strUserID;
			strUserName.TrimRight();
			m_KnightsManager.AddKnightsUser( KnightsSet.m_sIDNum, (char*)(LPCTSTR) strUserName );
		}

		KnightsSet.MoveNext();
	}	

	return TRUE;
}

int  CEbenezerDlg::GetKnightsAllMembers(int knightsindex, char *temp_buff, int& buff_index, int type )
{
	if( knightsindex <= 0 ) return 0;			

	CUser* pUser = NULL;
	CKnights* pKnights = NULL;
	int count = 0, i=0;

	if( type == 0 )	{
		for( i=0; i<MAX_USER; i++ ) {
			pUser = GetUnsafeUserPtr(i);
			if (pUser == NULL || pUser->m_pUserData->m_bKnights != knightsindex )
				continue;

			SetKOString( temp_buff, pUser->m_pUserData->m_id, buff_index );
			SetByte( temp_buff, pUser->m_pUserData->m_bFame, buff_index);
			SetByte( temp_buff, pUser->m_pUserData->m_bLevel, buff_index);
			SetShort( temp_buff, pUser->m_pUserData->m_sClass, buff_index);
			SetByte( temp_buff, 1, buff_index);
			count++;
		}	
	}
	else if( type == 1)	{
		pKnights = m_KnightsArray.GetData( knightsindex );
		if( !pKnights ) return 0;

		for( i=0; i<MAX_CLAN; i++ )	{
			if( pKnights->m_arKnightsUser[i].byUsed == 1 )	{	// 
				pUser = GetUserPtr(pKnights->m_arKnightsUser[i].strUserName, TYPE_CHARACTER);
				if( pUser )	{
					if( pUser->m_pUserData->m_bKnights == knightsindex )	{
						SetKOString( temp_buff, pUser->m_pUserData->m_id, buff_index );
						SetByte( temp_buff, pUser->m_pUserData->m_bFame, buff_index);
						SetByte( temp_buff, pUser->m_pUserData->m_bLevel, buff_index);
						SetShort( temp_buff, pUser->m_pUserData->m_sClass, buff_index);
						SetByte( temp_buff, 1, buff_index);
						count++;
					}
					else {
						m_KnightsManager.RemoveKnightsUser( knightsindex, pUser->m_pUserData->m_id );
					}
				}
				else	{
					SetShort( temp_buff, 0, buff_index );
					SetByte( temp_buff, 0, buff_index);
					SetByte( temp_buff, 0, buff_index);
					SetShort( temp_buff, 0, buff_index);
					SetByte( temp_buff, 0, buff_index);
					count++;	
				}
				
			}
		}	
	}

	return count;
}

int CEbenezerDlg::GetKnightsGrade(int nPoints)
{
	int nGrade = 5;
	int nClanPoints = nPoints / 24;		// 클랜등급 = 클랜원 국가 기여도의 총합 / 24

	if( nClanPoints >= 0 && nClanPoints < 2000 )	{
		nGrade = 5;
	}
	else if( nClanPoints >= 2000 && nClanPoints < 5000 )	{
		nGrade = 4;
	}
	else if( nClanPoints >= 5000 && nClanPoints < 10000 )	{
		nGrade = 3;
	}
	else if( nClanPoints >= 10000 && nClanPoints < 20000 )	{
		nGrade = 2;
	}
	else if( nClanPoints >= 20000 )	{
		nGrade = 1;
	}
	else nGrade = 5;

	return nGrade;
}

void CEbenezerDlg::CheckAliveUser()
{
	for (int i = 0; i < MAX_USER; i++)
	{
		CUser *pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL || pUser->GetState() != STATE_GAMESTART)
			continue;

		if (pUser->m_sAliveCount++ > 3)
		{
			pUser->Close();
			DEBUG_LOG_FILE("User dropped due to inactivity - char=%s", pUser->m_pUserData->m_id);
		}
	}
}

int CEbenezerDlg::KickOutAllUsers()
{
	CUser* pUser = NULL;
	int count = 0;

	for (int i = 0; i < MAX_USER; i++)
	{
		pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL || pUser->GetState() == STATE_DISCONNECTED)
			continue;

		BYTE state = pUser->GetState();
		pUser->CloseProcess();

		// Only delay (for saving)if they're logged in, this is awful... 
		// but until we do away with the shared memory system, it'll overflow the queue...
		if (state == STATE_GAMESTART)
		{
			count++;
			Sleep(50);
		}
	}
	return count;
}

__int64 CEbenezerDlg::GenerateItemSerial()
{
	MYINT64 serial;
	MYSHORT	increase;
	serial.i = 0;
	increase.w = 0;

	CTime t = CTime::GetCurrentTime();

	EnterCriticalSection( &g_serial_critical );

	increase.w = g_increase_serial++;

	serial.b[7] = (BYTE)m_nServerNo;
	serial.b[6] = (BYTE)(t.GetYear()%100);
	serial.b[5] = (BYTE)t.GetMonth();
	serial.b[4] = (BYTE)t.GetDay();
	serial.b[3] = (BYTE)t.GetHour();
	serial.b[2] = (BYTE)t.GetMinute();
	serial.b[1] = increase.b[1];
	serial.b[0] = increase.b[0];

	LeaveCriticalSection( &g_serial_critical );
	
//	TRACE("Generate Item Serial : %I64d\n", serial.i);
	return serial.i;
}

void CEbenezerDlg::KickOutZoneUsers(short zone)
{
	for (int i = 0; i < MAX_USER; i++)
	{
		CUser * pTUser = GetUnsafeUserPtr(i);     
		if (pTUser == NULL || pTUser->GetState() != STATE_GAMESTART) 
			continue;

		if (pTUser->m_pUserData->m_bZone == zone) 	// Only kick out users in requested zone.
		{
			C3DMap * pMap = GetZoneByID(pTUser->m_pUserData->m_bNation);
			if (pMap == NULL)
				continue;

			pTUser->ZoneChange(pMap->m_nZoneNumber, pMap->m_fInitX, pMap->m_fInitZ); // Move user to native zone.
		}
	}
}

void CEbenezerDlg::Send_UDP_All( char* pBuf, int len, int group_type )
{
	int server_number = (group_type == 0 ? m_nServerNo : m_nServerGroupNo);
	foreach_stlmap (itr, (group_type == 0 ? m_ServerArray : m_ServerGroupArray))
	{
		if (itr->second && itr->second->sServerNo == server_number)
			m_pUdpSocket->SendUDPPacket(itr->second->strServerIP, pBuf, len);
	}
}

BOOL CEbenezerDlg::LoadBattleTable()
{
	CBattleSet	BattleSet(&m_GameDB);

	if( !BattleSet.Open() ) {
		AfxMessageBox(_T("BattleSet Data Open Fail!"));
		return FALSE;
	}
	if(BattleSet.IsBOF() || BattleSet.IsEOF()) {
		AfxMessageBox(_T("BattleSet Data Empty!"));
		return FALSE;
	}

	BattleSet.MoveFirst();

	while( !BattleSet.IsEOF() )	{
		m_byOldVictory = BattleSet.m_byNation;		
		BattleSet.MoveNext();
	}

	return TRUE;
}

void CEbenezerDlg::Send_CommandChat( char* pBuf, int len, int nation, CUser* pExceptUser )
{
	for (int i = 0; i < MAX_USER; i++)
	{
		CUser * pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL || pUser->GetState() != STATE_GAMESTART || pUser == pExceptUser || (nation != 0 && nation != pUser->m_pUserData->m_bNation))
			continue;
		pUser->Send(pBuf, len);
	}
}

void CEbenezerDlg::GetCaptainUserPtr()
{
	foreach_stlmap (itr, m_KnightsArray)
	{
		CKnights *pKnights = itr->second;
		if (pKnights == NULL
			|| pKnights->m_byRanking != 1)
			continue;

		// do something cool here
	}
}

BOOL CEbenezerDlg::LoadKnightsRankTable()
{
	CKnightsRankSet	KRankSet(&m_GameDB);
	int nRank = 0, nKnightsIndex = 0, nKaursRank = 0, nElmoRank = 0, nFindKarus = 0, nFindElmo = 0, send_index = 0, temp_index = 0;
	CUser *pUser = NULL;
	CKnights* pKnights = NULL;
	CString strKnightsName;

	char send_buff[1024];		memset( send_buff, 0x00, 1024 );
	char temp_buff[1024];		memset( temp_buff, 0x00, 1024 );
	char strKarusCaptainName[1024];		memset( strKarusCaptainName, 0x00, 1024 );
	char strElmoCaptainName[1024];		memset( strElmoCaptainName, 0x00, 1024 );
	char strKarusCaptain[5][50];	
	char strElmoCaptain[5][50];	
	for( int i=0; i<5; i++)	{
		memset( strKarusCaptain[i], 0x00, 50 );
		memset( strElmoCaptain[i], 0x00, 50 );
	}

	if( !KRankSet.Open() ) {
		ConnectToDatabase(true);
		if (!KRankSet.Open())
		{
			TRACE("### KnightsRankTable Open Fail! ###\n");
			return TRUE;
		}
	}
	if(KRankSet.IsBOF() || KRankSet.IsEOF()) {
		TRACE("### KnightsRankTable Empty! ###\n");
		return TRUE;
	}

	KRankSet.MoveFirst();

	while( !KRankSet.IsEOF() )	{
		nRank = KRankSet.m_nRank;
		nKnightsIndex = KRankSet.m_shIndex;
		pKnights = m_KnightsArray.GetData( nKnightsIndex );
		strKnightsName = KRankSet.m_strName;
		strKnightsName.TrimRight();
		if( !pKnights )	{
			KRankSet.MoveNext();
			continue;
		}
		if( pKnights->m_byNation == KARUS )	{
			if( nKaursRank == 5 )	{
				KRankSet.MoveNext();
				continue;			
			}	
			pUser = GetUserPtr(pKnights->m_strChief, TYPE_CHARACTER);
			if( !pUser )	{
				KRankSet.MoveNext();
				continue;
			}
			if( pUser->m_pUserData->m_bZone != ZONE_BATTLE )	{
				KRankSet.MoveNext();
				continue;
			}
			if( pUser->m_pUserData->m_bKnights == nKnightsIndex	)	{
				pUser->m_pUserData->m_bFame = COMMAND_CAPTAIN;
				sprintf( strKarusCaptain[nKaursRank], "[%s][%s]", strKnightsName, pUser->m_pUserData->m_id);
				nKaursRank++;
				nFindKarus = 1;
				memset( send_buff, NULL, 1024 );	send_index = 0;
				SetByte( send_buff, WIZ_AUTHORITY_CHANGE, send_index );
				SetByte( send_buff, COMMAND_AUTHORITY, send_index );
				SetShort( send_buff, pUser->GetSocketID(), send_index );
				SetByte( send_buff, pUser->m_pUserData->m_bFame, send_index );
				Send_Region( send_buff, send_index, pUser->GetMap(), pUser->m_RegionX, pUser->m_RegionZ );

				memset( send_buff, NULL, 1024 );	send_index = 0;
			}
		}
		else if( pKnights->m_byNation == ELMORAD )	{
			if( nElmoRank == 5 )	{
				KRankSet.MoveNext();
				continue;
			}
			pUser = GetUserPtr(pKnights->m_strChief, TYPE_CHARACTER);
			if( !pUser )	{
				KRankSet.MoveNext();
				continue;
			}
			if( pUser->m_pUserData->m_bZone != ZONE_BATTLE )	{
				KRankSet.MoveNext();
				continue;
			}
			if( pUser->m_pUserData->m_bKnights == nKnightsIndex	)	{
				pUser->m_pUserData->m_bFame = COMMAND_CAPTAIN;
				sprintf( strElmoCaptain[nElmoRank], "[%s][%s]", strKnightsName, pUser->m_pUserData->m_id);
				nFindElmo = 1;
				nElmoRank++;
				memset( send_buff, NULL, 1024 );	send_index = 0;
				SetByte( send_buff, WIZ_AUTHORITY_CHANGE, send_index );
				SetByte( send_buff, COMMAND_AUTHORITY, send_index );
				SetShort( send_buff, pUser->GetSocketID(), send_index );
				SetByte( send_buff, pUser->m_pUserData->m_bFame, send_index );
				Send_Region( send_buff, send_index, pUser->GetMap(), pUser->m_RegionX, pUser->m_RegionZ );
			}
		}
		
		KRankSet.MoveNext();
	}

	_snprintf(strKarusCaptainName, sizeof(strKarusCaptainName), GetServerResource(IDS_KARUS_CAPTAIN), strKarusCaptain[0], strKarusCaptain[1], strKarusCaptain[2], strKarusCaptain[3], strKarusCaptain[4]);
	_snprintf(strElmoCaptainName, sizeof(strElmoCaptainName), GetServerResource(IDS_ELMO_CAPTAIN), strElmoCaptain[0], strElmoCaptain[1], strElmoCaptain[2], strElmoCaptain[3], strElmoCaptain[4]);

	TRACE("LoadKnightsRankTable Success\n");
	
	SetByte( send_buff, WIZ_CHAT, send_index );
	SetByte( send_buff, WAR_SYSTEM_CHAT, send_index );
	SetByte( send_buff, 1, send_index );
	SetShort( send_buff, -1, send_index );
	SetKOString(temp_buff, strKarusCaptainName, temp_index);

	SetByte( temp_buff, WIZ_CHAT, temp_index );
	SetByte( temp_buff, WAR_SYSTEM_CHAT, temp_index );
	SetByte( temp_buff, 1, temp_index );
	SetShort( temp_buff, -1, temp_index );
	SetKOString(temp_buff, strElmoCaptainName, temp_index);

	for (int i = 0; i < MAX_USER; i++)
	{
		CUser *pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL || pUser->GetState() != STATE_GAMESTART)
			continue;

		if (pUser->m_pUserData->m_bNation == KARUS)
			pUser->Send(send_buff, send_index);
		else if (pUser->m_pUserData->m_bNation == ELMORAD)
			pUser->Send(temp_buff, temp_index);
	}

	return TRUE;
}

void CEbenezerDlg::BattleZoneCurrentUsers()
{
	C3DMap* pMap = GetZoneByID(ZONE_BATTLE);
	if (pMap == NULL || m_nServerNo != pMap->m_nServerNo)
		return;

	char send_buff[128];	memset( send_buff, 0x00, 128 );
	int nKarusMan = 0, nElmoradMan = 0, send_index = 0;

	for (int i = 0; i < MAX_USER; i++)
	{
		CUser * pUser = GetUnsafeUserPtr(i);
		if (pUser == NULL || pUser->GetState() != STATE_GAMESTART || pUser->getZoneID() != ZONE_BATTLE)
			continue;

		if (pUser->getNation() == KARUS)
			nKarusMan++;
		else
			nElmoradMan++;
	}

	m_sKarusCount = nKarusMan;
	m_sElmoradCount = nElmoradMan;

	//TRACE("---> BattleZoneCurrentUsers - karus=%d, elmorad=%d\n", m_sKarusCount, m_sElmoradCount);

	SetByte( send_buff, UDP_BATTLEZONE_CURRENT_USERS, send_index );
	SetShort( send_buff, m_sKarusCount, send_index );
	SetShort( send_buff, m_sElmoradCount, send_index );
	Send_UDP_All( send_buff, send_index );

}

void CEbenezerDlg::FlySanta()
{
	int send_index = 0;
	char send_buff[128];	memset( send_buff, 0x00, 128 );	

	SetByte( send_buff, WIZ_SANTA, send_index );
	Send_All( send_buff, send_index );
} 

void CEbenezerDlg::WriteEventLog( char* pBuf )
{
	char strLog[256];	memset(strLog, 0x00, 256);
	CTime t = CTime::GetCurrentTime();
	wsprintf(strLog, "%d:%d-%d : %s \r\n", t.GetHour(), t.GetMinute(), t.GetSecond(), pBuf);
	EnterCriticalSection( &g_LogFile_critical );
	m_EvnetLogFile.Write( strLog, strlen(strLog) );
	LeaveCriticalSection( &g_LogFile_critical );
}