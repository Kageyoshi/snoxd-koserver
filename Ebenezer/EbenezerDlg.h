// EbenezerDlg.h : header file
//

#if !defined(AFX_EBENEZERDLG_H__655A21EF_E029_42C0_890A_68DA7F542428__INCLUDED_)
#define AFX_EBENEZERDLG_H__655A21EF_E029_42C0_890A_68DA7F542428__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma warning(disable : 4786)

#include "Iocport.h"
#include "Map.h"
#include "Define.h"
#include "GameDefine.h"
#include "AISocket.h"
#include "Npc.h"
#include "../shared/SharedMem.h"
#include "../shared/ini.h"
#include "Knights.h"
#include "KnightsManager.h"
#include "EVENT.h"
#include "UdpSocket.h"

#include "../shared/STLMap.h"
#include <vector>

/////////////////////////////////////////////////////////////////////////////
// CEbenezerDlg dialog

typedef CSTLMap <C3DMap>					ZoneArray;
typedef map <int, long>						LevelUpArray;
typedef CSTLMap <_CLASS_COEFFICIENT>		CoefficientArray;
typedef CSTLMap <_ITEM_TABLE>				ItemtableArray;
typedef CSTLMap <_MAGIC_TABLE>				MagictableArray;
typedef CSTLMap <_MAGIC_TYPE1>				Magictype1Array;
typedef CSTLMap <_MAGIC_TYPE2>				Magictype2Array;
typedef CSTLMap <_MAGIC_TYPE3>				Magictype3Array;
typedef CSTLMap	<_MAGIC_TYPE4>				Magictype4Array;
typedef CSTLMap <_MAGIC_TYPE5>				Magictype5Array;
typedef CSTLMap <_MAGIC_TYPE8>				Magictype8Array; 
typedef CSTLMap <CNpc>						NpcArray;
typedef CSTLMap <CAISocket>					AISocketArray;
typedef CSTLMap <_PARTY_GROUP>				PartyArray;
typedef CSTLMap <CKnights>					KnightsArray;
typedef CSTLMap <_ZONE_SERVERINFO>			ServerArray;
typedef CSTLMap <_HOME_INFO>				HomeArray;
typedef CSTLMap <_START_POSITION>			StartPositionArray;
typedef	CSTLMap	<EVENT>						QuestArray;
typedef	CSTLMap	<_SERVER_RESOURCE>			ServerResourceArray;
typedef vector <CString>					BlockNameArray;

class CUser;
class CEbenezerDlg : public CDialog
{
// Construction
public:	
	void WriteEventLog( char* pBuf );
	void FlySanta();
	void BattleZoneCurrentUsers();
	BOOL LoadKnightsRankTable();
	void GetCaptainUserPtr();
	void Send_CommandChat( char* pBuf, int len, int nation, CUser* pExceptUser = NULL );
	BOOL LoadBattleTable();
	void Send_UDP_All( char* pBuf, int len, int group_type = 0 );
	void KickOutZoneUsers(short zone);
	__int64 GenerateItemSerial();
	int KickOutAllUsers();
	void CheckAliveUser();
	int GetKnightsGrade(int nPoints);
	void WritePacketLog();
	int  GetKnightsAllMembers( int knightsindex, char *temp_buff, int& buff_index, int type=0 );
	BOOL LoadAllKnightsUserData();
	BOOL LoadAllKnights();
	BOOL LoadHomeTable();
	BOOL LoadStartPositionTable();
	void Announcement(BYTE type, int nation=0, int chat_type=8);
	void ResetBattleZone();
	void BanishLosers();
	void BattleZoneVictoryCheck();
	void BattleZoneOpenTimer();
	void BattleZoneOpen( int nType );	// 0:open 1:close
	void AliveUserCheck();
	void WithdrawUserOut();
	BOOL LoadMagicType8();
	BOOL LoadMagicType4();
	BOOL LoadMagicType5();
	BOOL LoadMagicType3();
	BOOL LoadMagicType2();
	BOOL LoadMagicType1();
	void KillUser( const char* strbuff );
	void Send_PartyMember( int party, char* pBuf, int len );
	void Send_KnightsMember( int index, char* pBuf, int len, int zone=100 );
	int GetAIServerPort();
	BOOL AISocketConnect( int zone, int flag = 0 );
	int GetRegionNpcIn( C3DMap* pMap, int region_x, int region_z, char* buff, int & t_count );
	BOOL LoadNoticeData();
	BOOL LoadBlockNameList();
	int GetRegionNpcList( C3DMap* pMap, int region_x, int region_z, char* nid_buff, int& t_count ); // Region All Npcs nid Packaging Function
	void RegionNpcInfoForMe( CUser* pSendUser );	// 9 Regions All Npcs nid Packaging Function
	int GetRegionUserList( C3DMap* pMap, int region_x, int region_z, char* buff, int &t_count ); // Region All Users uid Packaging Function
	int GetRegionUserIn( C3DMap* pMap, int region_x, int region_z, char* buff, int &t_count );	// Region All Users USERINOUT Packet Packaging Function
	void RegionUserInOutForMe( CUser* pSendUser );	// 9 Regions All Users uid Packaging Function
	BOOL LoadLevelUpTable();
	void SetGameTime();
	void UpdateWeather();
	void UpdateGameTime();
	void GetTimeFromIni();
	void Send_NearRegion( char* pBuf, int len, C3DMap *pMap, int region_x, int region_z, float curx, float curz, CUser* pExceptUser=NULL );
	void Send_FilterUnitRegion( char* pBuf, int len, C3DMap *pMap, int x, int z, float ref_x, float ref_z, CUser* pExceptUser=NULL );
	void Send_UnitRegion( char *pBuf, int len, C3DMap *pMap, int x, int z, CUser* pExceptUser=NULL, bool bDirect=true );
	BOOL LoadCoefficientTable();
	BOOL LoadMagicTable();
	BOOL LoadItemTable();
	BOOL LoadServerResourceTable();
	BOOL MapFileLoad();
	void UserAcceptThread();
	// sungyong 2001.11.06
	BOOL AIServerConnect();
	void SendAllUserInfo();
	void DeleteAllNpcList(int flag = 0);
	CNpc*  GetNpcPtr( int sid, int cur_zone );
	// ~sungyong 2001.11.06
	BOOL InitializeMMF();
	void UserInOutForMe( CUser* pSendUser );	// 9 Regions All Users USERINOUT Packet Packaging Function
	void NpcInOutForMe( CUser* pSendUser );	// 9 Regions All Npcs NPCINOUT Packet Packaging Function
	void Send_Region( char* pBuf, int len, C3DMap *pMap, int x, int z, CUser* pExceptUser = NULL, bool bDirect=true );	// zone == real zone number
	void Send_All( char* pBuf, int len, CUser* pExceptUser = NULL, int nation=0 );	// pointer != NULL don`t send to that user pointer
	void Send_AIServer( char* pBuf, int len );

	CString GetServerResource(int nResourceID);
	_START_POSITION *GetStartPosition(int nZoneID);

	long GetExpByLevel(int nLevel);
	C3DMap * GetZoneByID(int zoneID);

	CUser* GetUserPtr(const char* userid, NameType type);
	CUser * GetUserPtr(int sid);
	__forceinline CUser * GetUnsafeUserPtr(int sid);

	_PARTY_GROUP * CreateParty(CUser *pLeader);
	void DeleteParty(short sIndex);

	void AddToList(const char * format, ...);
	void WriteLog(const char * format, ...);

	CEbenezerDlg(CWnd* pParent = NULL);	// standard constructor

	static CIOCPort	m_Iocport;

	CSharedMemQueue	m_LoggerSendQueue, m_LoggerRecvQueue;

	HANDLE	m_hReadQueueThread;
	HANDLE	m_hMMFile;
	char*	m_lpMMFile;
	BOOL	m_bMMFCreate;
	DWORD	m_ServerOffset;

	char	m_ppNotice[20][128];
	char	m_AIServerIP[20];

	AISocketArray			m_AISocketArray;
	NpcArray				m_arNpcArray;
	ZoneArray				m_ZoneArray;
	ItemtableArray			m_ItemtableArray;
	MagictableArray			m_MagictableArray;
	Magictype1Array			m_Magictype1Array;
	Magictype2Array         m_Magictype2Array;
	Magictype3Array			m_Magictype3Array;
	Magictype4Array			m_Magictype4Array;
	Magictype5Array         m_Magictype5Array;
	Magictype8Array         m_Magictype8Array;
	CoefficientArray		m_CoefficientArray;
	LevelUpArray			m_LevelUpArray;
	PartyArray				m_PartyArray;
	KnightsArray			m_KnightsArray;
	HomeArray				m_HomeArray;
	StartPositionArray		m_StartPositionArray;
	QuestArray				m_Event;
	ServerResourceArray		m_ServerResourceArray;
	BlockNameArray			m_BlockNameArray;

	CKnightsManager			m_KnightsManager;

	short	m_sPartyIndex;
	short	m_sZoneCount;							// AI Server 재접속시 사용
	short	m_sSocketCount;							// AI Server 재접속시 사용
	// sungyong 2002.05.23
	short   m_sSendSocket;			
	BOOL	m_bFirstServerFlag;		// 서버가 처음시작한 후 게임서버가 붙은 경우에는 1, 붙지 않은 경우 0
	BOOL	m_bServerCheckFlag;
	BOOL	m_bPointCheckFlag;		// AI서버와 재접전에 NPC포인터 참조막기 (TRUE:포인터 참조, FALSE:포인터 참조 못함)
	short   m_sReSocketCount;		// GameServer와 재접시 필요
	float   m_fReConnectStart;	// 처음 소켓이 도착한 시간
	short   m_sErrorSocketCount;  // 이상소켓 감시용
	// ~sungyong 2002.05.23

	int m_nYear, m_nMonth, m_nDate, m_nHour, m_nMin, m_nWeather, m_nAmount;
	int m_nCastleCapture;

	// ~Yookozuna 2002.06.12
	BYTE    m_byBattleOpen, m_byOldBattleOpen;					// 0:전쟁중이 아님, 1:전쟁중(국가간전쟁), 2:눈싸움전쟁
	BYTE	m_bVictory, m_byOldVictory;
	BYTE	m_bKarusFlag, m_bElmoradFlag;
	BYTE    m_byKarusOpenFlag, m_byElmoradOpenFlag, m_byBanishFlag, m_byBattleSave;
	short   m_sDiscount;	// 능력치와 포인트 초기화 할인 (0:할인없음, 1:할인(50%) )
	short	m_sKarusDead, m_sElmoradDead, m_sBanishDelay, m_sKarusCount, m_sElmoradCount;
	int m_nBattleZoneOpenWeek, m_nBattleZoneOpenHourStart, m_nBattleZoneOpenHourEnd;
	char m_strKarusCaptain[MAX_ID_SIZE+1];
	char m_strElmoradCaptain[MAX_ID_SIZE+1];

	// ~Yookozuna 2002.07.17
	BYTE	m_bMaxRegenePoint;

	// ~Yookozuna 2002.09.21 - Today is Chusok :( 
	short m_sBuyID[MAX_BBS_POST];
	char  m_strBuyTitle[MAX_BBS_POST][MAX_BBS_TITLE];
	char  m_strBuyMessage[MAX_BBS_POST][MAX_BBS_MESSAGE];
	int   m_iBuyPrice[MAX_BBS_POST];
	float m_fBuyStartTime[MAX_BBS_POST];

	short m_sSellID[MAX_BBS_POST];
	char  m_strSellTitle[MAX_BBS_POST][MAX_BBS_TITLE];
	char  m_strSellMessage[MAX_BBS_POST][MAX_BBS_MESSAGE];
	int   m_iSellPrice[MAX_BBS_POST];
	float m_fSellStartTime[MAX_BBS_POST];

	// ~Yookozuna 2002.11.26 - 비러머글 남는 공지 --;
	BOOL	m_bPermanentChatMode;
	BOOL	m_bPermanentChatFlag;
	char	m_strPermanentChat[1024];

	// ~Yookozuna 2002.12.11 - 갓댐 산타 클로스 --;
	BOOL	m_bSanta;

	// zone server info
	int					m_nServerNo, m_nServerGroupNo;
	int					m_nServerGroup;	// server의 번호(0:서버군이 없다, 1:서버1군, 2:서버2군)
	ServerArray			m_ServerArray;
	ServerArray			m_ServerGroupArray;
	CUdpSocket*			m_pUdpSocket;
	CFile m_RegionLogFile;
	CFile m_LogFile;
	CFile m_EvnetLogFile;
// Dialog Data
	//{{AFX_DATA(CEbenezerDlg)
	enum { IDD = IDD_EBENEZER_DIALOG };
	CEdit	m_AnnounceEdit;
	CListBox	m_StatusList;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEbenezerDlg)
	public:
	virtual BOOL DestroyWindow();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

private:
	CIni	m_Ini;
	CDatabase m_GameDB;

	BOOL LoadTables();
	BOOL ConnectToDatabase(bool reconnect = false);

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CEbenezerDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EBENEZERDLG_H__655A21EF_E029_42C0_890A_68DA7F542428__INCLUDED_)
