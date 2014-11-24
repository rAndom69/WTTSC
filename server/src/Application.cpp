
#include "stdafx.h"
#include "Application.h"
#include "HttpServer.h"
#include "Game.h"
#include <Poco/FormattingChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FileChannel.h>
#include "Resources/SQLiteDatabaseResource.h"

class NoDelayServerSocket : public Poco::Net::ServerSocket
{
public:
	NoDelayServerSocket(int Port)
		:	Poco::Net::ServerSocket(Port)
	{
	}
	virtual Poco::Net::StreamSocket acceptConnection()
	{
		Poco::Net::StreamSocket socket = Poco::Net::ServerSocket::acceptConnection();
		socket.setNoDelay(true);
		return socket;
	}
	virtual Poco::Net::StreamSocket acceptConnection(Poco::Net::SocketAddress & clientAddr)
	{
		Poco::Net::StreamSocket socket = Poco::Net::ServerSocket::acceptConnection(clientAddr);
		socket.setNoDelay(true);
		return socket;

	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	Application	App;
	return App.run(argc, argv);
}

void Application::initialize(Poco::Util::Application& self)
{
	std::string executableDir = config().getString("application.dir");
	Poco::FormattingChannel *LogFile = new Poco::FormattingChannel(new Poco::PatternFormatter("%Y%m%d %H%M%S.%i %q:%s:%t"));
	Poco::AutoPtr<Poco::FileChannel> fileChannel(new Poco::FileChannel(Poco::Path(executableDir, "wttsc-server.log").toString()));
	fileChannel->setProperty("rotation", "10 M");
	fileChannel->setProperty("rotateOnOpen", "false");
	fileChannel->setProperty("purgeCount", "1");
	fileChannel->setProperty("archive", "number");
	fileChannel->setProperty("compress", "true");

	LogFile->setChannel(fileChannel);
	LogFile->open();
	Poco::Logger& Log = Poco::Logger::root();
	Log.setChannel(LogFile);
	Log.setLevel("debug");

	return _super::initialize(self);
}

int Application::main(const std::vector < std::string > & args)
{
	Poco::Logger& Log = Poco::Logger::get("Application");
	try
	{
		Log.information("Initiazation");

	//	Database. serialized access should help with deadlocks (aka none should happen at performance cost)
		Log.debug("Database initialization");
		std::string executableDir = config().getString("application.dir");
		sqlite3_config(SQLITE_CONFIG_SERIALIZED, 1);
		std::unique_ptr<SQLite::Database> Database;
		Database.reset(new SQLite::Database(Poco::Path(executableDir, "db.sqlite").toString(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
	//	Game controller
		Log.debug("Game initialization");
		std::shared_ptr<CGameController> Game(new CGameController(
			std::unique_ptr<CSQLiteDatabaseResource>(new CSQLiteDatabaseResource(std::move(Database))), 
			Poco::Path(executableDir).pushDirectory("www")));
		Game->Initialize();
	//	HTTP server (serve static files and rpc)
		Log.debug("Server initialization");
		Poco::AutoPtr<Poco::Net::HTTPServerParams> params(new Poco::Net::HTTPServerParams);
		params->setKeepAlive(true);
		params->setKeepAliveTimeout(Poco::Timespan(10 * 60, 0));
		params->setMaxKeepAliveRequests(1000);
		Poco::Net::HTTPServer Server(
			new CHTTPRequestHandlerFactory(Poco::Path(executableDir).pushDirectory("www"), Game), 
			NoDelayServerSocket(9090),
			params);
	//	Run
		Server.start();
		Log.information("Server started");

		waitForTerminationRequest();  // wait for CTRL-C or kill

		Log.information("Server shutdown");
		Server.stop();
	}
	catch (std::exception& e)
	{
		Log.error(e.what());
		return -1;
	}
	Log.information("Shutdown complete");
	return 0;
}
