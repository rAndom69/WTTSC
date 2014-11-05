
#pragma once

#include "IGame.h"
#include <Poco/Timestamp.h>
#include <Poco/Mutex.h>
#include <memory>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Object.h>
#include <Poco/Thread.h>
#include <Poco/Event.h>
#include <Poco/Logger.h>
#include "Resources/SoundResourceCollection.h"
#include "../ThirdParty/Database.h"

enum ButtonPressLength
{
	kButtonPressShort,
	kButtonPressLong
};

class IGameCallback;

class CGameData;

class IGameState
{
public:
	virtual ~IGameState()
	{}
	//	Control inputs
	virtual bool OnInputId(SideIndex button, const std::string& Id, IGameCallback* Callback, CGameData* Data) abstract;
	virtual bool OnInputPress(SideIndex button, ButtonPressLength press, IGameCallback* Callback, CGameData* Data) abstract;
	//	Timeout input (tick is called every approximately 200 miliseconds)
	virtual bool OnTick(IGameCallback* Callback, CGameData* Data) abstract;
	//	Interface state
	virtual Poco::JSON::Object GetInterfaceState(const IGameCallback* Callback, const CGameData* Data) abstract;
};

class IGameCallback
{
public:
	virtual void SetCurrentState(IGameState* newState) abstract;

//	Database related
	struct Score
	{
		int	score[2];
		Score(int s0 = 0, int s1 = 0)
		{ score[0] = s0; score[1] = s1; }
		int& operator [] (size_t index)
		{ return score[index]; }
		int operator [] (size_t index) const
		{ return score[index]; }
	};

	struct MatchResults
	{
		std::vector<Score>		SetResults;
		Score					Result;
		std::string				PlayerNames[2];
		time_t					DateTime;
		int						Win[2];
	};

	virtual void StoreCurrentGameResult() abstract;
	virtual std::string GetPlayerNameById(const std::string& Uid) const abstract;
	virtual void LoginPlayer(const std::string& Uid) abstract;
	virtual std::vector<MatchResults> GetLastResultsForPlayers(const std::string& playerId1, const std::string& playerId2, int max) const abstract;
	virtual std::vector<MatchResults> GetLastResults(int max) const abstract;
	virtual std::pair<std::string, std::string> GetRandomPlayers() const abstract;

	virtual void PlaySoundPointResult() abstract;
};

class CGameController : public IGameController, public IGameCallback, Poco::Runnable
{
protected:
	Poco::Logger&						m_Log;
	std::unique_ptr<IGameState>			m_State;
	std::unique_ptr<CGameData>			m_Data;
	Poco::Mutex							m_Mutex;
	Poco::Thread						m_Timer;
	volatile bool						m_Quit;
	std::unique_ptr<SQLite::Database>	m_Database;

	Poco::Event							m_StateChanged;				//!< Event notification of game state change
	volatile unsigned int				m_UpdateId;					//!< Last update id

	bool								m_ReloadClient;				//!< Force reload of one client
	Poco::JSON::Array::Ptr				m_Sounds;					//!< Sounds to be played by client
	unsigned int						m_SoundsPlay;				//!< Keep sounds over next GUI update

	CSoundResourceCollection			m_SoundResourceCollection;	//!< Sound resource collection


	//	Reset to beginning state
	void ResetToIdleState();
	//	Set current interface
	virtual void SetCurrentState(IGameState* newState) override;
	//	Handle commands from test-interface
	Poco::JSON::Object HandleRpcCalls(Poco::JSON::Object& object);

	enum 
	{
		kShortPressTimeMilisecMin		= 0,
		kLongPressTimeMilisecMin		= 1000,
		kVeryLongPressTimeMilisecMin	= 5000,

		kConnectionTimeout				= 10 * 1000,
	};

	//	Threaded loop for ticking
	virtual void run() override;
	//	Store current game result
	virtual void StoreCurrentGameResult() override;
	//	Return player name by it's id
	virtual std::string GetPlayerNameById(const std::string& Uid) const override;
	//	Return last N matches between specified players. One of player Ids may be empty to return matches played by other player
	virtual std::vector<MatchResults> GetLastResultsForPlayers(const std::string& playerId1, const std::string& playerId2, int max) const override;
	//	Return last N matches
	virtual std::vector<MatchResults> GetLastResults(int max) const override;
	//	Return random players who played at least one set together
	virtual std::pair<std::string, std::string> GetRandomPlayers() const override;
	//	Login player. If player exists his last logon time is updated, If player is logged on for first time his data are generated
	virtual void LoginPlayer(const std::string& Uid) override;

	//	Load results from active statement
	void GetResultsFromStatement(SQLite::Statement& statement, std::vector<MatchResults>& results) const;

	//	Trigger GUI update
	void UpdateGui();
	
	//	Wait for gui update or timeout
	void WaitForUpdate(unsigned int LastClientUpdateId);

	std::string GetPlayerIdByName(const std::string& Name);

	Poco::JSON::Array::Ptr GetLastLogons(size_t Count);
	Poco::JSON::Array::Ptr GetUsers();
	void RenameUser(const std::string& Id, const std::string& Name);


//	Sounds
//	NOTE: Sounds are played ONLY at next frame (aka with UpdateId set equally m_SoundsPlay)
	void ResetSounds();
	virtual void PlaySoundPointResult();

public:
	CGameController(SQLite::Database* Database, Poco::Path FSClientRoot);
   ~CGameController();
	virtual void OnInputId(SideIndex button, const std::string& Id) override;
	virtual void OnInputPress(SideIndex button, int miliseconds) override;
	virtual std::string GetInterfaceState(const std::string& message) override;

	virtual std::string RpcCall(const std::string& command) override;

	void Initialize();
};

