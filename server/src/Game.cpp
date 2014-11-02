
#include "stdafx.h"
#include "Game.h"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/ParseHandler.h>
#include <Poco/UUIDGenerator.h>
#include "../ThirdParty/Statement.h"
#include "../ThirdParty/Transaction.h"
#include <assert.h>

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
	std::string				m_SoundSet;

	int GetSetsCompleted() const;	

public:
	CGameData();
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
	int IsPlayerVictorious(int player) const;


	std::string MatchUuid() const;
	int  GameMode() const;
	void SetPlayerIdAndName(SideIndex button, const std::string& Uuid, const std::string& Name);
	void AddPlayerPoint(SideIndex button);
	void SubtractPlayerPoint(SideIndex button);
	void SetServerSide(SideIndex button);
	bool IsSetFinished() const;
	void StartSetContinueMatch();
	void Reset();

	std::string GetSoundSet() const;
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
};

//	After one of players won, this state is shortly displayed to show message
class CGameSwitchSideState : public ITemporaryCommonState
{
public:
	CGameSwitchSideState();
	virtual bool Next(IGameCallback* Callback, CGameData* Data);
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


class CIdleState : public ITemporaryCommonState
{
protected:
	int				m_ReaderSet;
	std::vector<IGameCallback::MatchResults> 
					m_ScoreTable;
	std::string		m_ScoreTableText;
	std::string		m_ScoreTableId;
	
	enum
	{
		MaxMatchForStatistics = 18,		//!<	Number of matches displayed in statistics
		TimeoutMs = 20 * 1000,	//!<	After this timeout screen is reset/other statistics displayed.
								//!<	Timeout is reset on any input
	};
	void DisplayStatistics(const std::vector<IGameCallback::MatchResults>& Statistics);
	void DisplayStatisticsForPlayers(const IGameCallback* Callback, const std::string& playerId1, const std::string& playerId2);
	void DisplayRandomStatistics(const IGameCallback* Callback);

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
	ResetTimeout(TimeoutMs);
	m_ReaderSet = 0;
	Data->Reset();
	DisplayRandomStatistics(Callback);
	return true;
}

bool CIdleState::OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data)
{
	ResetTimeout(TimeoutMs);
	if (press == kButtonPressLong)
	{
		Data->ToggleGameMode();
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
	ResetTimeout(TimeoutMs);
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
		std::cout << e.what() << std::endl;
	}
	return true;
}

Poco::JSON::Object CIdleState::GetInterfaceState(const IGameCallback* Callback, const CGameData* Data)
{
	Poco::JSON::Object object = ICommonState::GetInterfaceState(Callback, Data);
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
	return object;
}


CIdleState::CIdleState(const IGameCallback* Callback) 
	:	ITemporaryCommonState("idle", "Login via reader or short press any button to start game as guests. Long press any button to change game mode. Hold to reset.", TimeoutMs)
	,	m_ReaderSet(0)
{
	DisplayRandomStatistics(Callback);
}

void CIdleState::DisplayRandomStatistics(const IGameCallback* Callback)
{
	while (true)
	{
		switch (rand() % 2)
		{
		default:
		case 0:
			{
				if (m_ScoreTableId == "LastMatches")
				{	//	We don't want same statistics one after another
					continue;
				}
				m_ScoreTableId = "LastMatches";
				m_ScoreTableText = "Last matches";
				return DisplayStatistics(Callback->GetLastResults(MaxMatchForStatistics));
			}
		case 1:
			{
				std::pair<std::string, std::string>	Players = Callback->GetRandomPlayers();
				std::string ScoreTableId = Players.first + "x" + Players.second;
				if (m_ScoreTableId == ScoreTableId)
				{	//	Skip same statistics
					continue;
				}
				m_ScoreTableId = ScoreTableId;
				return DisplayStatisticsForPlayers(Callback, Players.first, Players.second);
			}
		};
	}
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

void CIdleState::DisplayStatistics(const std::vector<IGameCallback::MatchResults>& Statistics)
{
	m_ScoreTable = Statistics;
}

/////////////////////////////////////////////////////////////////////////////////

CGameSwitchSideState::CGameSwitchSideState() 
	: ITemporaryCommonState("game", "Switch sides!")
{
}

bool CGameSwitchSideState::Next(IGameCallback* Callback, CGameData* Data)
{
	Data->StartSetContinueMatch();
	Callback->SetCurrentState(new CGameState);
	return true;
}

bool CGameState::Next(IGameCallback* Callback, CGameData* Data)
{
	Callback->SetCurrentState(new CIdleState(Callback));
	return true;
}

CGameState::CGameState() 
	:	ITemporaryCommonState("game", "Short press to add point for specified player. Long press to subtract point.", TimeoutMs)
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
		Callback->StoreCurrentGameResult();
		Callback->SetCurrentState(new CGameSwitchSideState);
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////

const CGameData::CGameMode CGameData::m_Modes[3] =
{
	{	11, 2, "Short set / 11 balls to win", kVictoryStandard },
	{	21, 5, "Long set / 21 balls to win", kVictoryStandard },
	{	11, 2, "Short set / 11 balls to win. Only last set result is counted for victory (Also known as Bara's mode).", kVictoryLast }
};

int CGameData::GetSetsCompleted() const
{
	return m_Players[0].Sets + m_Players[1].Sets;
}

int CGameData::SideToPlayer(SideIndex button) const
{
	return (GetSetsCompleted() & 1) ^ button;
}

void CGameData::Reset()
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
	m_Mode = 0;
	m_ServeSide = kSideOne;
	m_ServeButtonSet = false;
	m_MatchUuid = Poco::UUIDGenerator().createRandom().toString();
	m_SoundSet = "en/computer";
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

int CGameData::IsPlayerVictorious(int player) const
{
	assert(IsSetFinished());
	switch (m_Modes[m_Mode].VictoryCondition)
	{
	case kVictoryStandard:
		if (PlayerSetResult(player) == PlayerSetResult(player ^ 1))
		{
			return 0;
		}
		return PlayerSetResult(player) > PlayerSetResult(player ^ 1);

	case kVictoryLast:
		return m_Players[player].Points > m_Players[player ^ 1].Points;
	};
	return 0;
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
	switch (rand() % 20)
	{
	default:
	case 0:		return "Miyamoto Musashi";
	case 1:		return "John Fitzgerald Kennedy";
	case 2:		return "Leónidás I.";
	case 3:		return "Iejasu Tokugawa";
	case 4:		return "Julie Kapuletova";
	case 5:		return "Antigoné";
	case 6:		return "Sofoklés";
	case 7:		return "????????";
	case 8:		return "Friedrich Nietzsche";
	case 9:		return "Sabina Spielrein";
	case 10:	return "Albert Einstein";
	case 11:	return "John Francis Kovář";
	case 12:	return "sanggjä";
	case 13:	return "Fiona";
	case 14:	return "Carl Gustav Jung";
	case 15:	return "Isaac Newton";
	case 16:	return "Isaac Asimov";
	case 17:	return "Karel Čapek";
	case 18:	return "Dick Francis";
	case 19:	return "Dale Carnegie";
	};
}

void CGameData::SerializeState(Poco::JSON::Object& object) const
{
	object.set("gameMode", m_Modes[m_Mode].ModeText);
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

CGameData::CGameData()
{
	Reset();
}

SideIndex CGameData::PlayerToSide(int player) const
{
	return static_cast<SideIndex>((GetSetsCompleted() & 1) ^ player);
}

std::string CGameData::GetSoundSet() const
{
	return m_SoundSet;
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
		std::cout << e.what() << std::endl;;
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
		std::cout << e.what() << std::endl;;
		ResetToIdleState();
	}
}

void CGameController::ResetToIdleState()
{
	SetCurrentState(new CIdleState(this));
	m_Data->Reset();
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
					ResetSounds();
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
		std::cout << e.what() << std::endl;;
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
		std::cout << e.what() << std::endl;
		return std::string();
	}
}


void CGameController::SetCurrentState(IGameState* newState)
{
	m_State.reset(newState);
	UpdateGui();
}

CGameController::CGameController(SQLite::Database* Database)
	:	m_Database(Database)
	,	m_StateChanged(false)
	,	m_UpdateId(0)
	,	m_ReloadClient(false)
{
	m_Quit = false;
	m_Data.reset(new CGameData);
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
				RenameUser(Params->getValue<std::string>("ID"), Params->getValue<std::string>("Name"));
			}
		}
		catch (std::exception& e)
		{
			result.set("error", e.what());
			std::cout << e.what() << std::endl;
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
			std::cout << e.what() << std::endl;
			ResetToIdleState();
		}
	}
}

void CGameController::Initialize()
{
	if (!m_Database->tableExists("users"))
	{
		m_Database->exec("CREATE TABLE users (uid VARCHAR(33) PRIMARY KEY, name VARCHAR(255), lastLogon INT);");
	}
	if (!m_Database->tableExists("match_results"))
	{
		m_Database->exec("BEGIN;\
						  CREATE TABLE match_results (match VARCHAR(33) PRIMARY KEY, uid1 VARCHAR(33), uid2 VARCHAR(33), result1 INTEGER, result2 INTEGER, win1 INTEGER, win2 INTEGER, mode INTEGER, date INTEGER, \
						  FOREIGN KEY (uid1) REFERENCES users(uid), FOREIGN KEY (uid2) REFERENCES users(uid)); \
						  CREATE INDEX match_results_uid1 ON match_results (uid1); \
						  CREATE INDEX match_results_uid2 ON match_results (uid2); \
						  CREATE INDEX match_results_date ON match_results (date); \
						  COMMIT;");
	}
	if (!m_Database->tableExists("results"))
	{
		m_Database->exec("BEGIN;\
			CREATE TABLE results (match VARCHAR(33), uid1 VARCHAR(33), uid2 VARCHAR(33), result1 INTEGER, result2 INTEGER, won1 INTEGER, won2 INTEGER, date INTEGER, \
			FOREIGN KEY (uid1) REFERENCES users(uid), FOREIGN KEY (uid2) REFERENCES users(uid));\
			CREATE INDEX results_match ON results (match); \
			COMMIT;");
	}
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
		SQLite::Transaction	transaction(*m_Database);

		SQLite::Statement	statement(*m_Database, "INSERT INTO results (match, uid1, uid2, result1, result2, won1, won2, date) VALUES (?,?,?,?,?,?,?,?);");
		statement.bind(1, m_Data->MatchUuid());
		statement.bind(2, m_Data->PlayerId(0));
		statement.bind(3, m_Data->PlayerId(1));
		statement.bind(4, m_Data->PlayerResult(0));
		statement.bind(5, m_Data->PlayerResult(1));
		statement.bind(6, m_Data->PlayerResult(0) > m_Data->PlayerResult(1) ? 1 : 0);
		statement.bind(7, m_Data->PlayerResult(0) < m_Data->PlayerResult(1) ? 1 : 0);
		statement.bind(8, Poco::Timestamp().epochTime());
		statement.exec();

		SQLite::Statement	statement2(*m_Database, "INSERT OR REPLACE INTO match_results (match, uid1, uid2, result1, result2, win1, win2, mode, date) VALUES (?,?,?,?,?,?,?,?,?);");
		statement2.bind(1, m_Data->MatchUuid());
		statement2.bind(2, m_Data->PlayerId(0));
		statement2.bind(3, m_Data->PlayerId(1));
		statement2.bind(4, m_Data->PlayerSetResult(0));
		statement2.bind(5, m_Data->PlayerSetResult(1));
		statement2.bind(6, m_Data->IsPlayerVictorious(0));
		statement2.bind(7, m_Data->IsPlayerVictorious(1));
		statement2.bind(8, m_Data->GameMode());
		statement2.bind(9, Poco::Timestamp().epochTime());
		statement2.exec();

		transaction.commit();
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;;
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
		SQLite::Statement	statement(*m_Database, "SELECT name FROM users WHERE uid = ?;");
		statement.bind(1, Uid);
		if (statement.executeStep())
		{
			return static_cast<std::string>(statement.getColumn(0));
		}
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
	return std::string();
}

std::vector<CGameController::MatchResults> 
CGameController::GetLastResultsForPlayers(const std::string& playerId1, const std::string& playerId2, int max) const
{
	std::vector<MatchResults>	Results;
	try
	{
		if (playerId1.length() && playerId2.length())
		{
			SQLite::Statement	statement(*m_Database, 
				"SELECT results.match, results.uid1, results.uid2, results.result1, results.result2, match_results.result1, match_results.result2, match_results.date, match_results.win1, match_results.win2, users1.name, users2.name \
				FROM results LEFT JOIN match_results ON results.match = match_results.match \
				LEFT JOIN users AS users1 ON match_results.uid1 = users1.uid \
				LEFT JOIN users AS users2 ON match_results.uid2 = users2.uid \
				WHERE match_results.match in  \
				(SELECT match FROM match_results WHERE (uid1=? AND uid2=?) OR (uid1=? AND uid2=?) ORDER BY date DESC LIMIT ?) \
				ORDER BY match_results.DATE desc, results.date");
			statement.bind(1, playerId1);
			statement.bind(2, playerId2);
			statement.bind(3, playerId2);
			statement.bind(4, playerId1);
			statement.bind(5, max);

			GetResultsFromStatement(statement, Results);
		}
		else
		if (playerId1.length() || playerId2.length())
		{
			std::string playerId = playerId1.length() ? playerId1 : playerId2;
			SQLite::Statement	statement(*m_Database, 
				"SELECT results.match, results.uid1, results.uid2, results.result1, results.result2, match_results.result1, match_results.result2, match_results.date, match_results.win1, match_results.win2, users1.name, users2.name \
				FROM results LEFT JOIN match_results ON results.match = match_results.match \
				LEFT JOIN users AS users1 ON match_results.uid1 = users1.uid \
				LEFT JOIN users AS users2 ON match_results.uid2 = users2.uid \
				WHERE match_results.match in  \
				(SELECT match FROM match_results WHERE uid1=? OR uid2=? ORDER BY date DESC LIMIT ?) \
				ORDER BY match_results.DATE desc, results.date");
			statement.bind(1, playerId);
			statement.bind(2, playerId);
			statement.bind(3, max);

			GetResultsFromStatement(statement, Results);
		}

	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
	return Results;
}

std::vector<CGameController::MatchResults> 
CGameController::GetLastResults(int max) const
{
	std::vector<MatchResults>	Results;
	try
	{
		SQLite::Statement	statement(*m_Database, 
			"SELECT results.match, results.uid1, results.uid2, results.result1, results.result2, match_results.result1, match_results.result2, match_results.date, match_results.win1, match_results.win2, users1.name, users2.name \
			FROM results LEFT JOIN match_results ON results.match = match_results.match \
			LEFT JOIN users AS users1 ON match_results.uid1 = users1.uid \
			LEFT JOIN users AS users2 ON match_results.uid2 = users2.uid \
			WHERE match_results.match in  \
			(SELECT match FROM match_results ORDER BY date DESC LIMIT ?) \
			ORDER BY match_results.DATE desc, results.date");
		statement.bind(1, max);
		GetResultsFromStatement(statement, Results);
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
	return Results;
}

void CGameController::GetResultsFromStatement(SQLite::Statement& statement, std::vector<MatchResults>& results) const
{
	std::string match_last;
	while (statement.executeStep())
	{
		std::string match = statement.getColumn(0);
		std::string id[2] = { static_cast<std::string>(statement.getColumn(1)), static_cast<std::string>(statement.getColumn(2)) };
		int			result[2] = { statement.getColumn(3), statement.getColumn(4) },
					match_result[2] = { statement.getColumn(5), statement.getColumn(6) };
		time_t		time = statement.getColumn(7);
		int			win[2] = { statement.getColumn(8), statement.getColumn(9) };
		std::string name[2] = { statement.getColumn(10), statement.getColumn(11) };
		if (match != match_last)
		{
			match_last = match;
			results.push_back(MatchResults());
			for (int idx = 0; idx < 2; ++idx)
			{
				results.back().Result[idx] = match_result[idx];
				results.back().PlayerNames[idx] = name[idx];
				results.back().Win[idx] = win[idx];
			}
			results.back().DateTime = time;
		}
		results.back().SetResults.push_back(Score(result[0], result[1]));
	}
}

std::pair<std::string, std::string> CGameController::GetRandomPlayers() const
{
	std::pair<std::string, std::string>	Result;
	try
	{
		//	TODO possibly very slow (select entire table, generate random)
		//	Think about it 
		SQLite::Statement	statement(*m_Database, 
			"SELECT uid1, uid2 FROM match_results ORDER BY RANDOM() LIMIT 1;");
		if (statement.executeStep())
		{
			Result.first = statement.getColumn(0);
			Result.second = statement.getColumn(1);
		}
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
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
	SQLite::Statement	statement(*m_Database, "SELECT COUNT(*) FROM users WHERE uid = ?;");
	statement.bind(1, Uid);
	if (statement.executeStep())
	{
		if (static_cast<int>(statement.getColumn(0)) == 0)
		{	//	Create player with defaults
			SQLite::Statement	statement2(*m_Database, "INSERT INTO users (uid, name, lastLogon) VALUES (?,?,?);");
			statement2.bind(1, Uid);
			statement2.bind(2, Uid);
			statement2.bind(3, Poco::Timestamp().epochTime());
			statement2.exec();
		}
		else
		{
			SQLite::Statement	statement2(*m_Database, "UPDATE users SET lastLogon = ? WHERE uid = ?;");
			statement2.bind(1, Poco::Timestamp().epochTime());
			statement2.bind(2, Uid);
			statement2.exec();
		}
	}
}

std::string CGameController::GetPlayerIdByName(const std::string& Name)
{
	SQLite::Statement	statement(*m_Database, "SELECT uid FROM users WHERE name = ?;");
	statement.bind(1, Name);
	if (!statement.executeStep())
	{
		throw std::runtime_error("Unknown player name: '" + Name + "'");
	}
	return static_cast<std::string>(statement.getColumn(0));
}

Poco::JSON::Array::Ptr CGameController::GetLastLogons(size_t Count)
{
	SQLite::Statement	statement(*m_Database, "SELECT uid, name, lastLogon FROM users ORDER BY lastLogon DESC LIMIT ?;");
	statement.bind(1, static_cast<int>(Count));
	Poco::JSON::Array::Ptr	Logons(new Poco::JSON::Array);
	while (statement.executeStep())
	{
		Poco::JSON::Object::Ptr	User(new Poco::JSON::Object);
		User->set("ID", static_cast<std::string>(statement.getColumn(0)));
		User->set("Name", static_cast<std::string>(statement.getColumn(1)));
		User->set("LastLogon", static_cast<time_t>(statement.getColumn(2)));
		Logons->add(User);
	}
	return Logons;
}

Poco::JSON::Array::Ptr CGameController::GetUsers()
{
	SQLite::Statement	statement(*m_Database, "SELECT uid, name, lastLogon FROM users ORDER BY uid;");
	Poco::JSON::Array::Ptr	Logons(new Poco::JSON::Array);
	while (statement.executeStep())
	{
		Poco::JSON::Object::Ptr	User(new Poco::JSON::Object);
		User->set("ID", static_cast<std::string>(statement.getColumn(0)));
		User->set("Name", static_cast<std::string>(statement.getColumn(1)));
		User->set("LastLogon", static_cast<time_t>(statement.getColumn(2)));
		Logons->add(User);
	}
	return Logons;
}

void CGameController::RenameUser(const std::string& Id, const std::string& Name)
{
	SQLite::Statement	statement(*m_Database, "UPDATE users SET name = ? WHERE uid = ?;");
	statement.bind(1, Name);
	statement.bind(2, Id);
	statement.exec();
}

void CGameController::ResetSounds()
{
	m_Sounds = new Poco::JSON::Array;
	m_SoundsPlay = m_UpdateId;
}

void CGameController::AddSound(const std::string& sound)
{
	m_Sounds->add(Poco::Dynamic::Var("sounds/" + m_Data->GetSoundSet() + "/" + sound + ".mp3"));
	m_SoundsPlay = m_UpdateId + 1;
}

void CGameController::AddPause(int miliseconds)
{
	m_Sounds->add(Poco::Dynamic::Var(miliseconds));
	m_SoundsPlay = m_UpdateId + 1;
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
	AddSound(Poco::format("%i", m_Data->PlayerResult(Player)));
	AddPause(200);
	AddSound(Poco::format("%i", m_Data->PlayerResult(Player^1)));
}
