
#pragma once

#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Path.h>
#include <map>
#include "IGame.h"

class CBadRequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
	virtual void handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp) override;
};

class CStaticFileRequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
	CStaticFileRequestHandler(const Poco::Path& StaticFilesPath);

	virtual void handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp) override;

	static std::map<std::string, std::string> ContentTypes();
	static const std::string URI;
protected:
	static std::string GetContentType(std::string extension);
	static const std::map<std::string, std::string> ContentType;

	Poco::Path m_wwwRoot;
};

class CRpcRequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
	CRpcRequestHandler(std::shared_ptr<IGameController> controller);

	virtual void handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp) override;
	static const std::string URI;
protected:
	std::shared_ptr<IGameController> m_Controller;
};

class CStateRpcRequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
	CStateRpcRequestHandler(std::shared_ptr<IGameController> controller);

	virtual void handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp) override;
	static const std::string URI;
protected:
	std::shared_ptr<IGameController> m_Controller;
};

class CHTTPRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
protected:
	Poco::Path m_wwwRoot;
	std::shared_ptr<IGameController> m_Game;

public:
	CHTTPRequestHandlerFactory(const Poco::Path& wwwRoot, std::shared_ptr<IGameController> game);

	virtual Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest &req) override;
};