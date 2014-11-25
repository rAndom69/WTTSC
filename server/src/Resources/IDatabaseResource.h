
#pragma once

#include <tuple>
#include <string>
#include <vector>

class IDatabaseResource
{
public:
	virtual ~IDatabaseResource()
	{}
	virtual void Initialize() abstract;
	virtual void StoreMatchResult(const std::string& MatchUuid, 
								  const std::pair<std::string, const std::string>& PlayerId, 
								  const std::pair<int, int>& PlayerResult, 
								  const std::pair<int, int>& PlayerSetResult, 
								  const std::pair<bool, bool>& PlayerVictory,
								  int GameMode) abstract;

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
		std::string				PlayerUids[2];
		time_t					DateTime;
		int						Win[2];
	};

	virtual std::vector<MatchResults> GetLatestResultsForUsers(const std::string PlayerId, int Limit) abstract;
	virtual std::vector<MatchResults> GetLatestResultsForUsers(const std::pair<std::string, std::string> PlayerId, int Limit) abstract;
	virtual std::vector<MatchResults> GetLatestResults(int Limit) abstract;
	virtual std::vector<MatchResults> GetLatestResultsFromTime(time_t Limit) abstract;

	struct UserResults
	{
		std::string				Uid;
		std::string				Name;
		int						SetsWon;
		int						SetsTotal;
		int						MatchesWon;
		int						MatchesTotal;
	};
	virtual std::vector<UserResults> GetUserResults(time_t Limit) abstract;


	virtual std::pair<std::string, std::string> GetRandomUsers() abstract;


	virtual bool UserExists(const std::string& PlayerId) abstract;
	virtual void CreateUser(const std::string& PlayerId, const std::string& PlayerName) abstract;
	virtual void UpdateUserLogonTime(const std::string& PlayerId) abstract;
	
	virtual std::string GetUserNameById(const std::string& PlayerId) abstract;
	virtual std::string GetUserIdByName(const std::string& PlayerName) abstract;
	virtual void SetUserNameById(const std::string& PlayerId, const std::string PlayerName) abstract;

	struct User
	{
		std::string Name;
		std::string Id;
		time_t      LastLogon;
	};

	virtual std::vector<User> GetLatestLogons(int Limit) abstract;
	virtual std::vector<User> GetUsers() abstract;
};