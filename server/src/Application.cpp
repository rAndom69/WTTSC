
#include "stdafx.h"
#include "Application.h"
#include "HttpServer.h"
#include "Game.h"
#include "../ThirdParty/Database.h"

int _tmain(int argc, _TCHAR* argv[])
{
	Application	App;
	return App.run(argc, argv);
}

void Application::initialize(Poco::Util::Application& self)
{
	return _super::initialize(self);
}

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

int Application::main(const std::vector < std::string > & args)
{
	std::string executableDir = config().getString("application.dir");
//	Database. serialized access should help with deadlocks (aka none should happen at performance cost)
	sqlite3_config(SQLITE_CONFIG_SERIALIZED, 1);
	std::unique_ptr<SQLite::Database> Database;
	Database.reset(new SQLite::Database(Poco::Path(executableDir, "db.sqlite").toString(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
//	Game controller
	std::shared_ptr<CGameController> Game(new CGameController(Database.release()));
	Game->Initialize();
//	HTTP server (serve static files and rpc)
	Poco::AutoPtr<Poco::Net::HTTPServerParams> params(new Poco::Net::HTTPServerParams);
	params->setKeepAlive(true);
	params->setKeepAliveTimeout(Poco::Timespan(10 * 60, 0));
	params->setMaxKeepAliveRequests(1000);
	Poco::Net::HTTPServer Server(
		new CHTTPRequestHandlerFactory(Poco::Path(executableDir).pushDirectory("www"), Game), 
		//Poco::Net::ServerSocket(9090), 
		NoDelayServerSocket(9090),
		params);
//	Run
	Server.start();
    std::cout << std::endl << "Server started" << std::endl;

    waitForTerminationRequest();  // wait for CTRL-C or kill

    std::cout << std::endl << "Shutting down..." << std::endl;
    Server.stop();


	return 0;
}
