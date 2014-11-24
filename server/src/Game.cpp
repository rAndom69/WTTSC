
#include "stdafx.h"
#include "Game.h"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/ParseHandler.h>
#include <Poco/UUIDGenerator.h>
#include <assert.h>
#include "Resources/SoundResourceCollection.h"

//	Represent single game data.
//	Game is played between two playes and they must switch sides of table based on number of sets played
class CGameData
{
protected:
	enum MatchVictoryCondition
	{
		kVictoryStandard,
		kVictoryLast
	};

	struct CGameMode
	{
		int			BallsToWin;
		int			BallsToServe;
		std::string	ModeText;
		MatchVictoryCondition VictoryCondition;
	};

	struct CPlayerMatchData
	{
		std::string Name;
		std::string Id;
		int			Points;
		int			Sets;
	};

	static const CGameMode	m_Modes[3];
	int						m_Mode;
	bool					m_ServeButtonSet;
	SideIndex				m_ServeSide;
	std::string				m_MatchUuid;
	CPlayerMatchData		m_Players[2];
	CSoundResourceCollection&				m_SoundResourceCollection;
	std::shared_ptr<const CSoundResource>	m_SoundResource;

	int GetSetsCompleted() const;	

public:
	CGameData(CSoundResourceCollection& soundResourceCollection);
	void ToggleGameMode();
	int  BallsToWin() const;
	int  BallsToServe() const;
	bool PlayerServes(SideIndex side) const;
	int SideToPlayer(SideIndex button) const;
	SideIndex PlayerToSide(int player) const;

	void SerializeState(Poco::JSON::Object& object) const;
	std::string RandomName() const;
	std::string PlayerId(int player) const;
	int PlayerResult(int player) const;
	//	NOTE:
	//	This functions expect set ended, but new one did not start.
	int PlayerSetResult(int player) const;
	//	NOTE:
	//	This functions expect set ended, but new one did not start.
	bool IsPlayerVictorious(int player) const;

	std::string MatchUuid() const;
	int  GameMode() const;
	void SetPlayerIdAndName(SideIndex button, const std::string& Uuid, const std::string& Name);
	void AddPlayerPoint(SideIndex button);
	void SubtractPlayerPoint(SideIndex button);
	void SetServerSide(SideIndex button);
	bool IsSetFinished() const;
	void StartSetContinueMatch();
	void ResetGame();
	void ResetConfiguration();

	void SelectSoundResource();
	void ToggleSoundResource();

	std::shared_ptr<const CSoundResource> GetSoundResource();
};

//	Common partial implementation of game state.
class ICommonState : public IGameState
{
protected:
	std::string m_Screen;
	std::string m_Info;

	void invalidCommand();
	void SetInfoText(const std::string& Info);
public:
	ICommonState(const std::string& screen, const std::string& info);
	virtual bool OnInputId(SideIndex button, const std::string& Id, IGameCallback* Callback, CGameData* Data) override;
	virtual bool OnTick(IGameCallback* Callback, CGameData* Data) override;
	virtual Poco::JSON::Object GetInterfaceState(const IGameCallback* Callback, const CGameData* Data) override;
};

//	Temporary (state could timeout) common implementation of game state.
class ITemporaryCommonState : public ICommonState
{
protected:
	Poco::Timestamp m_Timeout;

public:
	ITemporaryCommonState(const std::string& screen, const std::string& info = "Press any button to return back.", int TimeoutMilisec = 5000);
	virtual bool OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data) override;
	virtual bool OnTick(IGameCallback* Callback, CGameData* Data);
	void ResetTimeout(int TimeoutMilisec);
	virtual bool Next(IGameCallback* Callback, CGameData* Data) abstract;
	virtual Poco::JSON::Object GetInterfaceState(const IGameCallback* Callback, const CGameData* Data) override;
};

//	After one of players won, this state is shortly displayed to show message
class CGameSwitchSideState : public ITemporaryCommonState
{
protected:
	enum
	{
		TimeoutMs = 15 * 1000
	};

public:
	CGameSwitchSideState();
	virtual bool Next(IGameCallback* Callback, CGameData* Data);
	virtual bool OnInputPress(SideIndex side, ButtonPressLength press, IGameCallback* Callback, CGameData* Data) override;
};

//	Game implementation. Doh pretty short for compared to the remainder of this crap
//	Abandoned game will timeout in 10 minutes
class CGameState : public ITemporaryCommonState
{
protected:
	enum
	{
		TimeoutMs = 10 * 60 * 1000,
	};
public:
	CGameState();
	virtual bool OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data);
	virtual bool Next(IGameCallback* Callback, CGameData* Data);
};

//	Before game is run we have to detemine which player will serve
class CDetemineServerSideState : public ICommonState
{
public:
	CDetemineServerSideState();
	virtual bool OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data);
};

class CScreenSaverState : public ICommonState
{
	Poco::Logger&	m_Log;
public:

	CScreenSaverState();
	virtual bool OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data) override;
};

class CGameConfigurationState : public ITemporaryCommonState
{
protected:
	enum
	{
		TimeoutMs			= 120*1000,
	};

	enum ConfigurationType
	{
		kGameMode,
		kSoundSet,
		kDone,
	};

	ConfigurationType	m_ConfigurationType;

public:

	CGameConfigurationState();
	virtual bool Next(IGameCallback* Callback, CGameData* Data);
	virtual Poco::JSON::Object GetInterfaceState(const IGameCallback* Callback, const CGameData* Data) override
	{
		Poco::JSON::Object Result = ITemporaryCommonState::GetInterfaceState(Callback, Data);
		Poco::JSON::Array::Ptr	Configurations(new Poco::JSON::Array);
		for (int idx = 0; idx <= kDone; ++idx)
		{
			std::string Name, Replace;
			switch (idx)
			{
			case kGameMode:
				Name = "Rules";
				Replace = "gameMode";
				break;
			case kSoundSet:
				Name = "Sound set";
				Replace = "soundSetName";
				break;
			case kDone:
				Name = "Done";
				break;
			};
			Poco::JSON::Object::Ptr Node(new Poco::JSON::Object);
			Node->set("name", Name);
			if (m_ConfigurationType == idx)
			{
				Node->set("active", true);
			}
			Node->set("replace", Replace);
			Configurations->add(Node);
		}
		Result.set("configuration", Configurations);
		return Result;
	}
	virtual bool OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data);
};

class CIdleState : public ITemporaryCommonState
{
protected:
	Poco::Logger&	m_Log;
	int				m_ReaderSet;
	std::vector<IDatabaseResource::MatchResults> 
					m_ScoreTable;
	std::vector<IDatabaseResource::UserResults>
					m_UserScoreTable;
	std::string		m_ScoreTableText;
	int				m_ResetsToScreenSaver;
	int				m_CurrentStatistics;
	
	enum
	{
		MaxMatchForStatistics	= 18,							//!<	Number of matches displayed in statistics
#ifdef _DEBUG
		TimeoutMs				= 20 * 1000,
		ResetsToScreenSaver		= (60 * 1000) / TimeoutMs,
#else
		TimeoutMs				= 60 * 1000,					//!<	After this timeout screen is reset/other statistics displayed.
																//!<	Timeout is reset on any input
		ResetsToScreenSaver		= (10 * 60 * 1000) / TimeoutMs,	//!<	Number of resets before screen saver is run
#endif
	};
	void DisplayStatistics(const std::vector<IDatabaseResource::UserResults>& Statistics);
	void DisplayStatistics(const std::vector<IDatabaseResource::MatchResults>& Statistics);
	void DisplayStatisticsForPlayers(const IGameCallback* Callback, const std::string& playerId1, const std::string& playerId2);
	void DisplayUserRankings(IDatabaseResource& Database);
	void DisplayNextStatistics(const IGameCallback* Callback);
	void ResetTimeout();


public:
	CIdleState(const IGameCallback* Callback);
	virtual bool OnInputId(SideIndex button, const std::string& Id, IGameCallback* Callback, CGameData* Data);
	virtual bool OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data);
	virtual bool Next(IGameCallback* Callback, CGameData* Data);
	virtual Poco::JSON::Object GetInterfaceState(const IGameCallback* Callback, const CGameData* Data) override;
};

/////////////////////////////////////////////////////////////////////////////////

bool CIdleState::Next(IGameCallback* Callback, CGameData* Data)
{	//	Reset only game (only due to logout) and change highscore table
	ITemporaryCommonState::ResetTimeout(TimeoutMs);
	if (!m_ResetsToScreenSaver--)
	{
		Data->ResetConfiguration();
		Callback->SetCurrentState(new CScreenSaverState);
	}
	else
	{
		m_ReaderSet = 0;
		Data->ResetGame();
		DisplayNextStatistics(Callback);
	}
	return true;
}

bool CIdleState::OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	ResetTimeout();
	if (press == kButtonPressLong)
	{
		Callback->SetCurrentState(new CGameConfigurationState);
	}
	else
	if (press == kButtonPressShort)
	{
		Callback->SetCurrentState(new CDetemineServerSideState);
	}
	else
	{
		invalidCommand();
	}
	return true;
}

bool CIdleState::OnInputId(SideIndex button, const std::string& Id, IGameCallback* Callback, CGameData* Data)
{
	ResetTimeout();
	try
	{	//	support logon with single chip reader
		//	if there is already someone logged on, we'll simply logon other player
		//	if both players are logged on, we'll restart logon
		Callback->LoginPlayer(Id);
		std::string Name = Callback->GetPlayerNameById(Id);
		if (Name.empty())
		{	
			throw std::runtime_error("Unknown player: " + Id);
		}
		//	For now ignore reader index - we only have one
		//	We'll just cycle logons
		switch (m_ReaderSet)
		{
		case 3:
		case 0:
			if (Data->PlayerId(kSideTwo) != Id)
			{
				Data->SetPlayerIdAndName(kSideOne, Id, Name);
				SetInfoText("Second player logon or press button to start (Statistics are not stored if at least one of players is guest).");
				m_ReaderSet = 1;
			}
			break;
		case 1:
			if (Data->PlayerId(kSideOne) != Id)
			{
				Data->SetPlayerIdAndName(kSideTwo, Id, Name);
				SetInfoText("Press any button to start.");
				m_ReaderSet = 3;
			}
			break;
		}

		//	If at least one player is logged on display his/her statistics
		if (Data->PlayerId(0).length() || Data->PlayerId(1).length())
		{
			DisplayStatisticsForPlayers(Callback, Data->PlayerId(0), Data->PlayerId(1));
		}
	}
	catch (std::exception& e)
	{	//	errors during id init are ignored, does not reset game
		m_Log.error(e.what());
	}
	return true;
}

Poco::JSON::Object CIdleState::GetInterfaceState(const IGameCallback* Callback, const CGameData* Data)
{
	Poco::JSON::Object object = ITemporaryCommonState::GetInterfaceState(Callback, Data);
	if (m_ScoreTable.size())
	{
		Poco::JSON::Array::Ptr Matches(new Poco::JSON::Array);
		for (auto It = m_ScoreTable.begin(); It != m_ScoreTable.end(); ++It)
		{
			Poco::JSON::Object::Ptr	Match(new Poco::JSON::Object);
			Poco::JSON::Array::Ptr Names(new Poco::JSON::Array);
			Poco::JSON::Array::Ptr Win(new Poco::JSON::Array);
			for (int idx = 0; idx < 2; ++idx)
			{
				Names->add(It->PlayerNames[idx]);
				Win->add(It->Win[idx]);
			}
			Match->set("names", Names);
			Match->set("win", Win);

			Poco::JSON::Array::Ptr Sets(new Poco::JSON::Array);
			for (auto ItPoints = It->SetResults.begin(); ItPoints != It->SetResults.end(); ++ItPoints)
			{
				Poco::JSON::Array::Ptr Points(new Poco::JSON::Array);
				for (int idx = 0; idx < 2; ++idx)
				{
					Points->add((*ItPoints)[idx]);
				}
				Sets->add(Points);
			}

			Match->set("sets",  Sets);
			Match->set("datetime", It->DateTime);
			Matches->add(Match);
		}
		object.set("scoreTable", Matches);
		object.set("scoreText", m_ScoreTableText);
	}
	else
	if (m_UserScoreTable.size())
	{
		Poco::JSON::Array::Ptr Users(new Poco::JSON::Array);
		for (auto It = m_UserScoreTable.begin(); It != m_UserScoreTable.end(); ++It)
		{
			Poco::JSON::Object::Ptr	User(new Poco::JSON::Object);
			User->set("name", It->Name);
			User->set("setsWon", It->SetsWon);
			User->set("setsTotal", It->SetsTotal);
			User->set("matchesWon", It->MatchesWon);
			User->set("matchesTotal", It->MatchesTotal);
			Users->add(User);
		}
		object.set("userRankTable", Users);
		object.set("scoreText", m_ScoreTableText);
	}
	return object;
}


CIdleState::CIdleState(const IGameCallback* Callback) 
	:	ITemporaryCommonState("idle", "Login with reader or short press to start game as guests. Long press to change configuration.", TimeoutMs)
	,	m_Log(Poco::Logger::get("Game.IdleState"))
	,	m_ReaderSet(0)
	,	m_ResetsToScreenSaver(ResetsToScreenSaver)
	,	m_CurrentStatistics(rand())
{
	DisplayNextStatistics(Callback);
}

void CIdleState::DisplayNextStatistics(const IGameCallback* Callback)
{
	switch (m_CurrentStatistics++ % 2)
	{
	default:
	case 0:
		{
			m_ScoreTableText = "Last matches";
			return DisplayStatistics(Callback->GetLastResults(MaxMatchForStatistics));
		}
	case 1:
		{
			m_ScoreTableText = "User Rankings";
			return DisplayUserRankings(Callback->GetDatabase());
		}
	};
}

void CIdleState::DisplayStatisticsForPlayers(const IGameCallback* Callback, const std::string& playerId1, const std::string& playerId2)
{
	if (playerId1.length() && playerId2.length())
	{
		m_ScoreTableText = Callback->GetPlayerNameById(playerId1) + " vs " + Callback->GetPlayerNameById(playerId2);
	}
	else
	if (playerId1.length() || playerId2.length())
	{
		m_ScoreTableText = playerId1.length() ? Callback->GetPlayerNameById(playerId1) : Callback->GetPlayerNameById(playerId2);
	}
	DisplayStatistics(Callback->GetLastResultsForPlayers(playerId1, playerId2, MaxMatchForStatistics));
}

void CIdleState::DisplayStatistics(const std::vector<IDatabaseResource::UserResults>& Statistics)
{
	m_UserScoreTable = Statistics;
	m_ScoreTable.clear();
}

void CIdleState::DisplayStatistics(const std::vector<IDatabaseResource::MatchResults>& Statistics)
{
	m_ScoreTable = Statistics;
	m_UserScoreTable.clear();
}

void CIdleState::ResetTimeout()
{
	m_ResetsToScreenSaver = ResetsToScreenSaver;
	ITemporaryCommonState::ResetTimeout(TimeoutMs);
}

void CIdleState::DisplayUserRankings(IDatabaseResource& Database)
{	//	Last two months
	DisplayStatistics(Database.GetUserResults((Poco::Timestamp() - (2LL * 30 * 24 * 60 * 60 * 1000 * 1000)).epochTime()));
}

/////////////////////////////////////////////////////////////////////////////////

CGameSwitchSideState::CGameSwitchSideState() 
	: ITemporaryCommonState("game", "Switch sides! Short press to cancel last input and return to game, Long press to begin game.", TimeoutMs)
{
}

bool CGameSwitchSideState::Next(IGameCallback* Callback, CGameData* Data)
{
	Callback->StoreCurrentGameResult();
	Data->StartSetContinueMatch();
	Callback->SetCurrentState(new CGameState);
	return true;
}

bool CGameSwitchSideState::OnInputPress(SideIndex side, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	if (press == kButtonPressShort)
	{
		side = Data->PlayerToSide(0);
		Data->SubtractPlayerPoint(static_cast<SideIndex>(Data->PlayerResult(0) > Data->PlayerResult(1) ? side : (side ^ 1)));
		Callback->SetCurrentState(new CGameState);
		return true;
	}
	else
	if (press == kButtonPressLong)
	{
		return Next(Callback, Data);
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////

bool CGameState::Next(IGameCallback* Callback, CGameData* Data)
{
	Data->ResetGame();
	Callback->SetCurrentState(new CIdleState(Callback));
	return true;
}

CGameState::CGameState() 
	:	ITemporaryCommonState("game", "Short press to add point. Long press to subtract point.", TimeoutMs)
{}

bool CGameState::OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	ResetTimeout(TimeoutMs);
	if (press == kButtonPressShort)
	{
		Data->AddPlayerPoint(button);
		Callback->PlaySoundPointResult();
	}
	else
	if (press == kButtonPressLong)
	{
		Data->SubtractPlayerPoint(button);
		Callback->PlaySoundPointResult();
	}
	else
	{
		invalidCommand();
	}
	if (Data->IsSetFinished())
	{
		Callback->SetCurrentState(new CGameSwitchSideState);
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////

const CGameData::CGameMode CGameData::m_Modes[3] =
{
	{	11, 2, "Short set (11 balls to win)", kVictoryStandard },
	{	21, 5, "Long set (21 balls to win)", kVictoryStandard },
	{	11, 2, "All in one (11 balls to win)", kVictoryLast }
};

int CGameData::GetSetsCompleted() const
{
	return m_Players[0].Sets + m_Players[1].Sets;
}

int CGameData::SideToPlayer(SideIndex button) const
{
	return (GetSetsCompleted() & 1) ^ button;
}

void CGameData::ResetGame()
{
	srand(static_cast<unsigned>(time(NULL)));
	for (int player = 0; player < 2; ++player)
	{
		m_Players[player].Id = std::string();
		m_Players[player].Name = RandomName();
		while (player && m_Players[1].Name == m_Players[0].Name)
			m_Players[player].Name = RandomName();
		m_Players[player].Points = 0;
		m_Players[player].Sets = 0;
	}
	m_ServeSide = kSideOne;
	m_ServeButtonSet = false;
	m_MatchUuid = Poco::UUIDGenerator().createRandom().toString();
}

void CGameData::ResetConfiguration()
{
	m_Mode = 0;
	SelectSoundResource();
}

void CGameData::StartSetContinueMatch()
{
	if (m_Players[0].Points > m_Players[1].Points)
		m_Players[0].Sets++;
	else
		m_Players[1].Sets++;
	m_Players[0].Points = 0;
	m_Players[1].Points = 0;
}

bool CGameData::IsSetFinished() const
{
	if (m_Players[0].Points >= BallsToWin() || 
		m_Players[1].Points >= BallsToWin())
	{
		if (std::abs(m_Players[0].Points - m_Players[1].Points) >= 2)
		{
			return true;
		}
	}
	return false;
}

void CGameData::SetServerSide(SideIndex button)
{
	m_ServeSide = button;
	m_ServeButtonSet = true;
}

void CGameData::SubtractPlayerPoint(SideIndex button)
{
	if (m_Players[SideToPlayer(button)].Points > 0)
	{
		m_Players[SideToPlayer(button)].Points--;
	}
}

void CGameData::AddPlayerPoint(SideIndex button)
{
	m_Players[SideToPlayer(button)].Points++;
}

void CGameData::SetPlayerIdAndName(SideIndex button, const std::string& Uuid, const std::string& Name)
{
	if (m_Players[SideToPlayer(static_cast<SideIndex>(button ^ 1))].Id == Uuid)
	{
		throw std::runtime_error("Unable to use same player id twice.");
	}
	m_Players[SideToPlayer(button)].Id = Uuid;
	m_Players[SideToPlayer(button)].Name = Name;
}

int CGameData::GameMode() const
{
	return m_Mode;
}

std::string CGameData::MatchUuid() const
{
	return m_MatchUuid;
}

bool CGameData::IsPlayerVictorious(int player) const
{
	assert(IsSetFinished());
	switch (m_Modes[m_Mode].VictoryCondition)
	{
	case kVictoryStandard:
		if (PlayerSetResult(player) == PlayerSetResult(player ^ 1))
		{
			return false;
		}
		return PlayerSetResult(player) > PlayerSetResult(player ^ 1);

	case kVictoryLast:
		return m_Players[player].Points > m_Players[player ^ 1].Points;
	};
	return false;
}

int CGameData::PlayerSetResult(int player) const
{
	assert(IsSetFinished());
	return m_Players[player].Sets + ((m_Players[player].Points > m_Players[player^1].Points) ? 1 : 0);
}

int CGameData::PlayerResult(int player) const
{
	return m_Players[player].Points;
}

std::string CGameData::PlayerId(int player) const
{
	return m_Players[player].Id;
}

std::string CGameData::RandomName() const
{
	switch (rand() % 11)
	{
	default:
	case 0:		return "Miyamoto Musashi";
	case 1:		return "John Fitzgerald Kennedy";
	case 2:		return "Iejasu Tokugawa";
	case 3:		return "Sabina Spielrein";
	case 4:		return "Carl Gustav Jung";
	case 5:		return "Isaac Newton";
	case 6:		return "Isaac Asimov";
	case 7:		return "Dick Francis";
	case 8:		return "Dale Carnegie";
	case 9:		return "Friedrich Nietzsche";
	case 10:	return "Albert Einstein";
	};
}

void CGameData::SerializeState(Poco::JSON::Object& object) const
{
	object.set("gameMode", m_Modes[m_Mode].ModeText);
	if (m_SoundResource != nullptr) 
	{
		object.set("soundSetName", m_SoundResource->GetName() + " (" + m_SoundResource->GetLanguage() + ")");
	}
	else 
	{
		object.set("soundSetName", "None");
	}

	Poco::JSON::Array::Ptr Players(new Poco::JSON::Array);
	for (int side = 0; side < 2; ++side)
	{
		SideIndex	sideIndex = static_cast<SideIndex>(side);
		int			playerIndex = SideToPlayer(sideIndex);
		Poco::JSON::Object::Ptr	Player(new Poco::JSON::Object);
		Player->set("name",		m_Players[playerIndex].Name);
		Player->set("points",	m_Players[playerIndex].Points);
		Player->set("sets",		m_Players[playerIndex].Sets);
		Player->set("serves",	PlayerServes(sideIndex));
		Players->add(Player);

	}		
	object.set("players", Players);
}

bool CGameData::PlayerServes(SideIndex sideIndex) const
{
	//	Determine player which serves
	if ((m_Players[0].Points >= (BallsToWin() - 1)) && (m_Players[1].Points >= (BallsToWin() - 1)))
	{
		if (((((m_Players[0].Points + m_Players[1].Points) - ((BallsToWin() - 1) * 2)) + m_ServeSide + sideIndex) % 2) == 0)
			return m_ServeButtonSet;
	}
	else
	{
		if (((((m_Players[0].Points + m_Players[1].Points) / BallsToServe()) + m_ServeSide + sideIndex) % 2) == 0)
			return m_ServeButtonSet;
	}
	return false;
}

int CGameData::BallsToServe() const
{
	return m_Modes[m_Mode].BallsToServe;
}

int CGameData::BallsToWin() const
{
	return m_Modes[m_Mode].BallsToWin;
}

void CGameData::ToggleGameMode()
{
	m_Mode++;
	if (m_Mode >= _countof(m_Modes))
	{
		m_Mode = 0;
	}
}

CGameData::CGameData(CSoundResourceCollection& soundResourceCollection)
	:	m_SoundResourceCollection(soundResourceCollection)
{
	ResetGame();
}

SideIndex CGameData::PlayerToSide(int player) const
{
	return static_cast<SideIndex>((GetSetsCompleted() & 1) ^ player);
}

std::shared_ptr<const CSoundResource> CGameData::GetSoundResource()
{
	return m_SoundResource;
}

void CGameData::SelectSoundResource()
{
	//	I expect this will be based on who plays...
	//	For now just random
	auto Begin = m_SoundResourceCollection.begin(), End = m_SoundResourceCollection.end();
	size_t Size = End - Begin;
	if (Size) 
	{
		m_SoundResource = *(Begin + (rand() % static_cast<int>(Size)));
	}
}

void CGameData::ToggleSoundResource()
{
	auto Begin = m_SoundResourceCollection.begin(), End = m_SoundResourceCollection.end();
	size_t Size = End - Begin;
	if (Size) 
	{
		if (m_SoundResource == nullptr) 
		{
			m_SoundResource = *Begin;
		}
		else
		{
			for (auto It = Begin; It != End; ++It)
			{
				if ((*It)->GetUnique() == m_SoundResource->GetUnique())
				{
					if (It + 1 == End)
					{
						m_SoundResource = *Begin;
					}
					else
					{
						m_SoundResource = *(It + 1);
					}
					break;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////

ICommonState::ICommonState(const std::string& screen, const std::string& info) 
	:	m_Screen(screen)
	,	m_Info(info)
{}

bool ICommonState::OnInputId(SideIndex button, const std::string& Id, IGameCallback* Callback, CGameData* Data)
{
	invalidCommand();
	return false;
}

Poco::JSON::Object ICommonState::GetInterfaceState(const IGameCallback* Callback, const CGameData* Data)
{
	Poco::JSON::Object object;
	object.set("screen", m_Screen);
	object.set("info", m_Info);
	Data->SerializeState(object);
	return object;
}

bool ICommonState::OnTick(IGameCallback* Callback, CGameData* Data)
{
	return false;
}

void ICommonState::invalidCommand()
{
	throw std::runtime_error("Invalid command.");
}

void ICommonState::SetInfoText(const std::string& Info)
{
	m_Info = Info;
}

/////////////////////////////////////////////////////////////////////////////////

ITemporaryCommonState::ITemporaryCommonState(const std::string& screen, const std::string& info /*= "Press any button to return back."*/, int TimeoutMilisec /*= 5000*/) 
	:	ICommonState(screen, info)
	,	m_Timeout(Poco::Timestamp() + (TimeoutMilisec * 1000))
{}

void ITemporaryCommonState::ResetTimeout(int TimeoutMilisec)
{
	m_Timeout = (Poco::Timestamp() + (TimeoutMilisec * 1000));
}

bool ITemporaryCommonState::OnTick(IGameCallback* Callback, CGameData* Data)
{
	if (m_Timeout < Poco::Timestamp())
	{
		return Next(Callback, Data);
	}
	return false;
}

bool ITemporaryCommonState::OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	return Next(Callback, Data);
}

Poco::JSON::Object ITemporaryCommonState::GetInterfaceState(const IGameCallback* Callback, const CGameData* Data)
{
	Poco::JSON::Object Result = ICommonState::GetInterfaceState(Callback, Data);
	int Seconds = 0;
	Poco::Timestamp Now;
	if (m_Timeout > Now) 
	{
		Seconds = static_cast<int>(((m_Timeout - Now) + 500000) / 1000000);
	}
	Result.set("TimeoutInSeconds", Seconds);
	return Result;
}

/////////////////////////////////////////////////////////////////////////////////

CGameConfigurationState::CGameConfigurationState() 
	:	ITemporaryCommonState("configuration", "Short press to toggle value. Long press to change option.", TimeoutMs)
	,	m_ConfigurationType(kGameMode)
{
}

bool CGameConfigurationState::OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	ResetTimeout(TimeoutMs);
	if (press == kButtonPressLong)
	{
		m_ConfigurationType = static_cast<ConfigurationType>(m_ConfigurationType + 1);
		if (m_ConfigurationType > kDone) {
			m_ConfigurationType = kGameMode;
		}
		return true;
	}
	else
	if (press == kButtonPressShort)
	{
		switch (m_ConfigurationType)
		{
		case kGameMode:
			Data->ToggleGameMode();
			break;
		case kSoundSet:
			Data->ToggleSoundResource();
			Callback->PlayRandomSound();
			break;
		case kDone:
			Callback->SetCurrentState(new CIdleState(Callback));
			break;
		default:
			return false;
		};
		return true;
	}
	return false;
}

bool CGameConfigurationState::Next(IGameCallback* Callback, CGameData* Data)
{
	Callback->SetCurrentState(new CIdleState(Callback));
	return true;
}

/////////////////////////////////////////////////////////////////////////////////

bool CDetemineServerSideState::OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	Data->SetServerSide(button);
	Callback->SetCurrentState(new CGameState);
	return true;
}

CDetemineServerSideState::CDetemineServerSideState() 
	:	ICommonState("game", "Player who will serve press button.")
{}

/////////////////////////////////////////////////////////////////////////////////

bool CScreenSaverState::OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	Data->ResetGame();
	Callback->SetCurrentState(new CIdleState(Callback));
	return true;
}

CScreenSaverState::CScreenSaverState() 
	:	ICommonState("screensaver", "")
	,	m_Log(Poco::Logger::get("Game.ScreenSaverState"))
{

}

/////////////////////////////////////////////////////////////////////////////////

void CGameController::OnInputId(SideIndex button, const std::string& Id)
{
	try
	{
		Poco::ScopedLock<Poco::Mutex> Lock(m_Mutex);

		if (m_State->OnInputId(button, Id, this, m_Data.get()))
		{
			UpdateGui();
		}
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		ResetToIdleState();
	}
}

void CGameController::OnInputPress(SideIndex button, int miliseconds)
{
	try
	{
		Poco::ScopedLock<Poco::Mutex> Lock(m_Mutex);

		if (miliseconds >= kVeryLongPressTimeMilisecMin)
		{
			ResetToIdleState();
		}
		else
		{
			if (m_State->OnInputPress(button, miliseconds >= kLongPressTimeMilisecMin ? kButtonPressLong : kButtonPressShort, this, m_Data.get()))
			{
				UpdateGui();
			}
		}
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		ResetToIdleState();
	}
}

void CGameController::ResetToIdleState()
{
	m_Data->ResetGame();
	m_Data->ResetConfiguration();
	SetCurrentState(new CIdleState(this));
}

std::string CGameController::GetInterfaceState(const std::string& message)
{
	try
	{
		if (message.length())
		{
			Poco::JSON::Parser parser;
			Poco::JSON::ParseHandler::Ptr handler = new Poco::JSON::ParseHandler;
			parser.setHandler(handler);
			parser.parse(message);
			Poco::Dynamic::Var result = handler->asVar();
			Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();
			if (object.isNull())
			{
				throw std::runtime_error("Invalid RPC input.");
			}
			WaitForUpdate(object->getValue<unsigned int>("UpdateId"));
			{
				Poco::ScopedLock<Poco::Mutex> Lock(m_Mutex);
				Poco::JSON::Object result = m_State->GetInterfaceState(this,m_Data.get());
				result.set("UpdateId", static_cast<int>(m_UpdateId));	//	Get rid of volatile which breaks serialization... Doh
				if (m_ReloadClient) 
				{
					result.set("Reload", true);
					m_ReloadClient = false;
				}
				if (m_SoundsPlay == m_UpdateId)
				{
					result.set("Sounds", m_Sounds);
				}
				std::ostringstream str;
				result.stringify(str);
				return str.str();
			}
		}
		else
		{
			throw std::runtime_error("Invalid RPC input.");
		}
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		return std::string();
	}
}


std::string CGameController::RpcCall(const std::string& command)
{
	try
	{
		Poco::JSON::Parser parser;
		Poco::JSON::ParseHandler::Ptr handler = new Poco::JSON::ParseHandler;
		parser.setHandler(handler);
		parser.parse(command);
		Poco::Dynamic::Var result = handler->asVar();
		Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();
		if (object.isNull())
		{
			throw std::runtime_error("Invalid RPC command.");
		}
		std::stringstream str;
		HandleRpcCalls(*object).stringify(str);
		return str.str();
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		return std::string();
	}
}


void CGameController::SetCurrentState(IGameState* newState)
{
	m_State.reset(newState);
	UpdateGui();
}

CGameController::CGameController(std::unique_ptr<IDatabaseResource> DBResource, Poco::Path FSClientRoot)
	:	m_Log(Poco::Logger::get("Game"))
	,	m_DBResource(std::move(DBResource))
	,	m_StateChanged(false)
	,	m_UpdateId(0)
	,	m_ReloadClient(false)
	,	m_SoundResourceCollection(FSClientRoot)
{
	m_Quit = false;
	m_Data.reset(new CGameData(m_SoundResourceCollection));
	m_Timer.start(*this);
	ResetToIdleState();
	ResetSounds();
}

Poco::JSON::Object CGameController::HandleRpcCalls(Poco::JSON::Object& object)
{
	Poco::JSON::Object result;

	if (object.has("command"))
	{
		try
		{
			Poco::JSON::Object::Ptr commands = object.get("command").extract<Poco::JSON::Object::Ptr>();
			if (commands->has("Press"))
			{
				auto Params = commands->getObject("Press");
				int Side = Params->getValue<int>("Side");
				int Time = Params->getValue<int>("Time");
				OnInputPress(static_cast<SideIndex>(Side), Time);
			}
			if (commands->has("ID"))
			{
				auto Params = commands->getObject("ID");
				int Side = Params->getValue<int>("Side");
				std::string ID = Params->getValue<std::string>("ID");
				OnInputId(static_cast<SideIndex>(Side), ID);
			}
			if (commands->has("Name"))
			{
				auto Params = commands->getObject("Name");
				int Side = Params->getValue<int>("Side");
				std::string Name = Params->getValue<std::string>("Name");
				OnInputId(static_cast<SideIndex>(Side), GetPlayerIdByName(Name));
			}
			if (commands->has("LastLogons"))
			{
				result.set("LastLogons", GetLastLogons(commands->getValue<int>("LastLogons")));
			}
			if (commands->has("Users"))
			{
				result.set("Users", GetUsers());
			}
			if (commands->has("RenameUser"))
			{
				auto Params = commands->getObject("RenameUser");
				m_DBResource->SetUserNameById(Params->getValue<std::string>("ID"), Params->getValue<std::string>("Name"));
			}
		}
		catch (std::exception& e)
		{
			result.set("error", e.what());
			m_Log.error(e.what());
		}
	}

	return result;
}

void CGameController::run()
{
	while (!m_Quit)
	{
		try
		{
			Poco::Thread::sleep(200);
			{
				Poco::ScopedLock<Poco::Mutex> Lock(m_Mutex);
				if (m_State->OnTick(this, m_Data.get()))
				{
					UpdateGui();
				}
			}
		}
		catch (std::exception& e)
		{
			m_Log.error(e.what());
			ResetToIdleState();
		}
	}
}

void CGameController::Initialize()
{
	m_DBResource->Initialize();
}

void CGameController::StoreCurrentGameResult()
{
	if (!m_Data->PlayerId(0).length() || !m_Data->PlayerId(1).length())
	{
		return;
	}
	try
	{
		//	Results are stored before new set begins (which means we have to add winning set to corresponding player)
		//	Shitty design
		m_DBResource->StoreMatchResult(
			m_Data->MatchUuid(),
			std::make_pair(m_Data->PlayerId(0), m_Data->PlayerId(1)),
			std::make_pair(m_Data->PlayerResult(0), m_Data->PlayerResult(1)),
			std::make_pair(m_Data->PlayerSetResult(0), m_Data->PlayerSetResult(1)),
			std::make_pair(m_Data->IsPlayerVictorious(0), m_Data->IsPlayerVictorious(1)),
			m_Data->GameMode());
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
	}
}

CGameController::~CGameController()
{
	m_Quit = true;
	m_Timer.join();
}

std::string CGameController::GetPlayerNameById(const std::string& Uid) const
{
	try
	{
		return m_DBResource->GetUserNameById(Uid);
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
	}
	return std::string();
}

std::vector<IDatabaseResource::MatchResults> 
CGameController::GetLastResultsForPlayers(const std::string& playerId1, const std::string& playerId2, int Limit) const
{
	std::vector<IDatabaseResource::MatchResults>	Results;
	try
	{
		if (playerId1.length() && playerId2.length())
		{
			Results = m_DBResource->GetLatestResultsForUsers(std::make_pair(playerId1, playerId2), Limit);
		}
		else
		if (playerId1.length() || playerId2.length())
		{
			Results = m_DBResource->GetLatestResultsForUsers(playerId1.length() ? playerId1 : playerId2, Limit);
		}
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
	}
	return Results;
}

std::vector<IDatabaseResource::MatchResults> 
CGameController::GetLastResults(int Limit) const
{
	std::vector<IDatabaseResource::MatchResults>	Results;
	try
	{
		Results = m_DBResource->GetLatestResults(Limit);
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
	}
	return Results;
}

std::pair<std::string, std::string> CGameController::GetRandomPlayers() const
{
	std::pair<std::string, std::string>	Result;
	try
	{
		Result = m_DBResource->GetRandomUsers();
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
	}
	return Result;
}

void CGameController::UpdateGui()
{
	{
		Poco::ScopedLock<Poco::Mutex>	Lock(m_Mutex);
		if (!++m_UpdateId)
		{	//	skip zero which is uninitialized state on client
			m_UpdateId = 1;
		}
	}
	m_StateChanged.set();
}

void CGameController::WaitForUpdate(unsigned int LastClientUpdateId)
{
	{
		Poco::ScopedLock<Poco::Mutex>	Lock(m_Mutex);
		if (LastClientUpdateId != m_UpdateId)
			return;
	}
	//	return current state at least every 2 seconds
	if (m_StateChanged.tryWait(kConnectionTimeout))
	{	//	reset event, all threads waiting should be free now.
		m_StateChanged.reset();
	}
}

void CGameController::LoginPlayer(const std::string& Uid)
{
	if (m_DBResource->UserExists(Uid))
	{
		m_DBResource->UpdateUserLogonTime(Uid);
	}
	else
	{
		m_DBResource->CreateUser(Uid, Uid);
	}
}

std::string CGameController::GetPlayerIdByName(const std::string& Name)
{
	return m_DBResource->GetUserIdByName(Name);
}

Poco::JSON::Array::Ptr CGameController::ConvertLogonsToJson(const std::vector<IDatabaseResource::User>& Players)
{
	Poco::JSON::Array::Ptr	Logons(new Poco::JSON::Array);
	for (auto It = Players.begin(); It != Players.end(); ++It)
	{
		Poco::JSON::Object::Ptr	User(new Poco::JSON::Object);
		User->set("ID", It->Id);
		User->set("Name", It->Name);
		User->set("LastLogon", It->LastLogon);
		Logons->add(User);
	}
	return Logons;
}

Poco::JSON::Array::Ptr CGameController::GetLastLogons(size_t Count)
{
	return ConvertLogonsToJson(m_DBResource->GetLatestLogons(Count));
}

Poco::JSON::Array::Ptr CGameController::GetUsers()
{
	return ConvertLogonsToJson(m_DBResource->GetUsers());
}

void CGameController::ResetSounds()
{
	m_Sounds = new Poco::JSON::Array;
	m_SoundsPlay = m_UpdateId;
}

void CGameController::PlaySoundPointResult()
{
	ResetSounds();
	//	skip zero - zero
	if (m_Data->PlayerResult(0) == m_Data->PlayerResult(1) &&
		m_Data->PlayerResult(0) == 0) {
		return;
	}
	int Player = 0;
	if (!m_Data->PlayerServes(m_Data->PlayerToSide(Player))) {
		Player ^= 1;
	}
	try
	{
		auto SoundResource = m_Data->GetSoundResource();
		m_Sounds->add(Poco::Dynamic::Var(SoundResource->GetNumber(m_Data->PlayerResult(Player), false)));
		m_Sounds->add(Poco::Dynamic::Var(100));
		m_Sounds->add(Poco::Dynamic::Var(SoundResource->GetNumber(m_Data->PlayerResult(Player ^ 1), true)));
		m_SoundsPlay = m_UpdateId + 1;
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		ResetSounds();
	}
}

void CGameController::PlayRandomSound()
{
	ResetSounds();
	try
	{
		auto SoundResource = m_Data->GetSoundResource();
		m_Sounds->add(Poco::Dynamic::Var(SoundResource->GetNumber(rand() % 22, false)));
		m_Sounds->add(Poco::Dynamic::Var(100));
		m_Sounds->add(Poco::Dynamic::Var(SoundResource->GetNumber(rand() % 22, true)));
		m_SoundsPlay = m_UpdateId + 1;
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		ResetSounds();
	}
}

IDatabaseResource& CGameController::GetDatabase() const 
{
	return *m_DBResource;
}
