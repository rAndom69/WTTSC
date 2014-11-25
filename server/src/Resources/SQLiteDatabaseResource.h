
#pragma once

#include "IDatabaseResource.h"
#include "../../ThirdParty/Database.h"
#include <memory>

class CSQLiteDatabaseResource : public IDatabaseResource
{
protected:
	std::unique_ptr<SQLite::Database>	m_Database;

	std::vector<MatchResults> GetMatchResultsFromStatement(SQLite::Statement& statement);
	std::vector<User> GetUserDataFromStatement(SQLite::Statement& statement);
	std::vector<UserResults> GetUserResultsDataFromStatement(SQLite::Statement& statement);

public:
	CSQLiteDatabaseResource(std::unique_ptr<SQLite::Database> Database);

	virtual void Initialize() override;
	virtual void StoreMatchResult(const std::string& MatchUuid, 
		const std::pair<std::string, const std::string>& PlayerId, 
		const std::pair<int, int>& PlayerResult, 
		const std::pair<int, int>& PlayerSetResult, 
		const std::pair<bool, bool>& PlayerVictory,
		int GameMode) override;

	virtual std::vector<MatchResults> GetLatestResultsForUsers(const std::string PlayerId, int Limit) override;
	virtual std::vector<MatchResults> GetLatestResultsForUsers(const std::pair<std::string, std::string> PlayerId, int Limit) override;
	virtual std::vector<MatchResults> GetLatestResults(int Limit) override;
	virtual std::vector<MatchResults> GetLatestResultsFromTime(time_t Limit) override;

	virtual std::pair<std::string, std::string> GetRandomUsers() override;
	virtual std::vector<UserResults> GetUserResults(time_t Limit) override;

	virtual bool UserExists(const std::string& PlayerId) override;
	virtual void CreateUser(const std::string& PlayerId, const std::string& PlayerName) override;
	virtual void UpdateUserLogonTime(const std::string& PlayerId) override;
	virtual std::string GetUserNameById(const std::string& PlayerId) override;
	virtual std::string GetUserIdByName(const std::string& PlayerName) override;
	virtual void SetUserNameById(const std::string& PlayerId, const std::string PlayerName) override;

	virtual std::vector<CSQLiteDatabaseResource::User> GetLatestLogons(int Limit) override;
	virtual std::vector<CSQLiteDatabaseResource::User> GetUsers() override;
};