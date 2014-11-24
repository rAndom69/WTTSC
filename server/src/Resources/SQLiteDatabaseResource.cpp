
#include "stdafx.h"
#include "SQLiteDatabaseResource.h"
#include <Poco/Timestamp.h>
#include "../../ThirdParty/Statement.h"
#include "../../ThirdParty/Transaction.h"
#include <algorithm>

void CSQLiteDatabaseResource::Initialize()
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

void CSQLiteDatabaseResource::StoreMatchResult(const std::string& MatchUuid, const std::pair<std::string, const std::string>& PlayerId, const std::pair<int, int>& PlayerResult, const std::pair<int, int>& PlayerSetResult, const std::pair<bool, bool>& PlayerVictory, int GameMode)
{
	SQLite::Transaction	transaction(*m_Database);

	SQLite::Statement	statement(*m_Database, "INSERT INTO results (match, uid1, uid2, result1, result2, won1, won2, date) VALUES (?,?,?,?,?,?,?,?);");
	statement.bind(1, MatchUuid);
	statement.bind(2, PlayerId.first);
	statement.bind(3, PlayerId.second);
	statement.bind(4, PlayerResult.first);
	statement.bind(5, PlayerResult.second);
	statement.bind(6, PlayerResult.first > PlayerResult.second ? 1 : 0);
	statement.bind(7, PlayerResult.first < PlayerResult.second ? 1 : 0);
	statement.bind(8, Poco::Timestamp().epochTime());
	statement.exec();

	SQLite::Statement	statement2(*m_Database, "INSERT OR REPLACE INTO match_results (match, uid1, uid2, result1, result2, win1, win2, mode, date) VALUES (?,?,?,?,?,?,?,?,?);");
	statement2.bind(1, MatchUuid);
	statement2.bind(2, PlayerId.first);
	statement2.bind(3, PlayerId.second);
	statement2.bind(4, PlayerSetResult.first);
	statement2.bind(5, PlayerSetResult.second);
	statement2.bind(6, PlayerVictory.first);
	statement2.bind(7, PlayerVictory.second);
	statement2.bind(8, GameMode);
	statement2.bind(9, Poco::Timestamp().epochTime());
	statement2.exec();

	transaction.commit();
}

std::string CSQLiteDatabaseResource::GetUserNameById(const std::string& PlayerId)
{
	SQLite::Statement	statement(*m_Database, "SELECT name FROM users WHERE uid = ?;");
	statement.bind(1, PlayerId);
	if (statement.executeStep())
	{
		return static_cast<std::string>(statement.getColumn(0));
	}
	throw std::runtime_error("Unknown player id.");
}

std::vector<CSQLiteDatabaseResource::MatchResults> CSQLiteDatabaseResource::GetMatchResultsFromStatement(SQLite::Statement& statement)
{
	std::vector<CSQLiteDatabaseResource::MatchResults> Results;
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
			Results.push_back(MatchResults());
			for (int idx = 0; idx < 2; ++idx)
			{
				Results.back().Result[idx] = match_result[idx];
				Results.back().PlayerNames[idx] = name[idx];
				Results.back().Win[idx] = win[idx];
			}
			Results.back().DateTime = time;
		}
		Results.back().SetResults.push_back(Score(result[0], result[1]));
	}
	return Results;
}

std::vector<CSQLiteDatabaseResource::MatchResults> CSQLiteDatabaseResource::GetLatestResultsForUsers(const std::string PlayerId, int Limit)
{
	SQLite::Statement	statement(*m_Database, 
		"SELECT results.match, results.uid1, results.uid2, results.result1, results.result2, match_results.result1, match_results.result2, match_results.date, match_results.win1, match_results.win2, users1.name, users2.name \
		FROM results LEFT JOIN match_results ON results.match = match_results.match \
		LEFT JOIN users AS users1 ON match_results.uid1 = users1.uid \
		LEFT JOIN users AS users2 ON match_results.uid2 = users2.uid \
		WHERE match_results.match in  \
		(SELECT match FROM match_results WHERE uid1=? OR uid2=? ORDER BY date DESC LIMIT ?) \
		ORDER BY match_results.DATE desc, results.date");
	statement.bind(1, PlayerId);
	statement.bind(2, PlayerId);
	statement.bind(3, Limit);

	return GetMatchResultsFromStatement(statement);
}

std::vector<CSQLiteDatabaseResource::MatchResults> CSQLiteDatabaseResource::GetLatestResultsForUsers(const std::pair<std::string, std::string> PlayerId, int Limit)
{
	SQLite::Statement	statement(*m_Database, 
		"SELECT results.match, results.uid1, results.uid2, results.result1, results.result2, match_results.result1, match_results.result2, match_results.date, match_results.win1, match_results.win2, users1.name, users2.name \
		FROM results LEFT JOIN match_results ON results.match = match_results.match \
		LEFT JOIN users AS users1 ON match_results.uid1 = users1.uid \
		LEFT JOIN users AS users2 ON match_results.uid2 = users2.uid \
		WHERE match_results.match in  \
		(SELECT match FROM match_results WHERE (uid1=? AND uid2=?) OR (uid1=? AND uid2=?) ORDER BY date DESC LIMIT ?) \
		ORDER BY match_results.DATE desc, results.date");
	statement.bind(1, PlayerId.first);
	statement.bind(2, PlayerId.second);
	statement.bind(3, PlayerId.second);
	statement.bind(4, PlayerId.first);
	statement.bind(5, Limit);

	return GetMatchResultsFromStatement(statement);
}

std::vector<CSQLiteDatabaseResource::MatchResults> CSQLiteDatabaseResource::GetLatestResults(int Limit)
{
	SQLite::Statement	statement(*m_Database, 
		"SELECT results.match, results.uid1, results.uid2, results.result1, results.result2, match_results.result1, match_results.result2, match_results.date, match_results.win1, match_results.win2, users1.name, users2.name \
		FROM results LEFT JOIN match_results ON results.match = match_results.match \
		LEFT JOIN users AS users1 ON match_results.uid1 = users1.uid \
		LEFT JOIN users AS users2 ON match_results.uid2 = users2.uid \
		WHERE match_results.match in  \
		(SELECT match FROM match_results ORDER BY date DESC LIMIT ?) \
		ORDER BY match_results.DATE desc, results.date");
	statement.bind(1, Limit);
	return GetMatchResultsFromStatement(statement);
}

std::pair<std::string, std::string> CSQLiteDatabaseResource::GetRandomUsers()
{
	//	TODO possibly very slow (select entire table, generate random)
	//	Think about it 
	SQLite::Statement	statement(*m_Database, 
		"SELECT uid1, uid2 FROM match_results ORDER BY RANDOM() LIMIT 1;");
	if (statement.executeStep())
	{
		return std::make_pair(static_cast<std::string>(statement.getColumn(0)), static_cast<std::string>(statement.getColumn(1)));
	}
	throw std::runtime_error("No players.");
}

bool CSQLiteDatabaseResource::UserExists(const std::string& PlayerId)
{
	SQLite::Statement	statement(*m_Database, "SELECT COUNT(*) FROM users WHERE uid = ?;");
	statement.bind(1, PlayerId);
	if (statement.executeStep())
	{
		return (static_cast<int>(statement.getColumn(0)) != 0);
	}
	return false;
}

void CSQLiteDatabaseResource::CreateUser(const std::string& PlayerId, const std::string& PlayerName)
{
	SQLite::Statement	statement(*m_Database, "INSERT INTO users (uid, name, lastLogon) VALUES (?,?,?);");
	statement.bind(1, PlayerId);
	statement.bind(2, PlayerName);
	statement.bind(3, Poco::Timestamp().epochTime());
	statement.exec();
}

void CSQLiteDatabaseResource::UpdateUserLogonTime(const std::string& PlayerId)
{
	SQLite::Statement	statement(*m_Database, "UPDATE users SET lastLogon = ? WHERE uid = ?;");
	statement.bind(1, Poco::Timestamp().epochTime());
	statement.bind(2, PlayerId);
	statement.exec();
}

std::string CSQLiteDatabaseResource::GetUserIdByName(const std::string& PlayerName)
{
	SQLite::Statement	statement(*m_Database, "SELECT uid FROM users WHERE name = ?;");
	statement.bind(1, PlayerName);
	if (!statement.executeStep())
	{
		throw std::runtime_error("Unknown player name: '" + PlayerName + "'");
	}
	return static_cast<std::string>(statement.getColumn(0));
}

std::vector<CSQLiteDatabaseResource::User> CSQLiteDatabaseResource::GetUserDataFromStatement(SQLite::Statement& statement)
{
	std::vector<CSQLiteDatabaseResource::User>	Results;
	while (statement.executeStep())
	{
		User	player;
		player.Id = statement.getColumn(0);
		player.Name = statement.getColumn(1);
		player.LastLogon = statement.getColumn(2);
		Results.emplace_back(player);
	}
	return Results;
}

std::vector<CSQLiteDatabaseResource::User> CSQLiteDatabaseResource::GetLatestLogons(int Limit)
{
	SQLite::Statement	statement(*m_Database, "SELECT uid, name, lastLogon FROM users ORDER BY lastLogon DESC LIMIT ?;");
	statement.bind(1, Limit);
	return GetUserDataFromStatement(statement);
}

std::vector<CSQLiteDatabaseResource::User> CSQLiteDatabaseResource::GetUsers()
{
	SQLite::Statement	statement(*m_Database, "SELECT uid, name, lastLogon FROM users ORDER BY uid;");
	return GetUserDataFromStatement(statement);
}

void CSQLiteDatabaseResource::SetUserNameById(const std::string& PlayerId, const std::string PlayerName)
{
	SQLite::Statement	statement(*m_Database, "UPDATE users SET name = ? WHERE uid = ?;");
	statement.bind(1, PlayerName);
	statement.bind(2, PlayerId);
	statement.exec();
}

CSQLiteDatabaseResource::CSQLiteDatabaseResource(std::unique_ptr<SQLite::Database> Database) 
	:	m_Database(std::move(Database))
{

}

std::vector<CSQLiteDatabaseResource::UserResults> CSQLiteDatabaseResource::GetUserResultsDataFromStatement(SQLite::Statement& statement)
{
	std::vector<UserResults>	Results;
	while (statement.executeStep())
	{
		UserResults		UserResult;
		UserResult.Uid = statement.getColumn(0);
		UserResult.Name = statement.getColumn(1);
		UserResult.SetsWon = statement.getColumn(2);
		UserResult.SetsTotal = static_cast<int>(statement.getColumn(3)) + UserResult.SetsWon;
		UserResult.MatchesWon = statement.getColumn(4);
		UserResult.MatchesTotal = statement.getColumn(5);
		Results.emplace_back(std::move(UserResult));
	}
	return Results;
}

std::vector<CSQLiteDatabaseResource::UserResults> CSQLiteDatabaseResource::GetUserResults(time_t Limit)
{
	//	we'll do ordering in code. it would require another select and as I expect it will change...
	SQLite::Statement	statement(*m_Database, "SELECT results.uid, users.name, SUM(sets_won), SUM(sets_lost), SUM(match_won), SUM(match_total) FROM \
		(SELECT uid1 AS uid, result1 AS sets_won, result2 AS sets_lost, win1 AS match_won, 1 AS match_total FROM match_results WHERE date >= ? \
		UNION ALL \
		SELECT uid2 AS uid, result2 AS sets_won, result1 AS sets_lost, win2 AS match_won, 1 AS match_total FROM match_results WHERE date >= ?) AS results \
		LEFT JOIN users ON results.uid=users.uid \
		GROUP BY results.uid");
	statement.bind(1, Limit);
	statement.bind(2, Limit);
	auto Result = GetUserResultsDataFromStatement(statement);
	std::sort(Result.begin(), Result.end(), [](const UserResults& i, const UserResults& j) -> bool {
		auto ScoreFunction = [](const UserResults& result) -> int
		{
			//	(SetsWon - SetsLost) +			(the real score)
			return (result.SetsWon * 2 - result.SetsTotal) +
			//	SetsWon							(bonus for more sets or longer matches)
				result.SetsWon +
			//	MatchesWon						(activity and winning bonus)
				result.MatchesWon +
			//	MatchesTotal					(activity bonus)
				result.MatchesTotal;
		};
		return ScoreFunction(i) > ScoreFunction(j);
	});
	return Result;
}
