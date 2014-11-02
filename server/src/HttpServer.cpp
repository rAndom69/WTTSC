
#include "stdafx.h"
#include "HttpServer.h"
#include <string> 
#include <algorithm>
#include <Poco/File.h>
#include <sstream>
#include <iostream>

const std::string
CStaticFileRequestHandler::URI = "/www/";

const std::map<std::string, std::string> 
CStaticFileRequestHandler::ContentType = CStaticFileRequestHandler::ContentTypes();

CStaticFileRequestHandler::CStaticFileRequestHandler(const Poco::Path& wwwRoot) 
	:	m_wwwRoot(wwwRoot)
{
}

std::map<std::string, std::string> 
CStaticFileRequestHandler::ContentTypes()
{
	std::map<std::string, std::string>	contentTypes;
	contentTypes["html"] = "text/html";
	contentTypes["js"]   = "application/javascript";
	contentTypes["css"]  = "text/css";
	contentTypes["gif"]	 = "image/gif";
	contentTypes["png"]  = "image/png";
	contentTypes["mp3"]  = "audio/mpeg";
	return contentTypes;
}

std::string 
CStaticFileRequestHandler::GetContentType(std::string extension)
{
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	auto ct = ContentType.find(extension);
	if (ct != ContentType.end())
	{
		return ct->second;
	}
	return "application/octet-stream";
}

void 
CStaticFileRequestHandler::handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp)
{
	try
	{
		Poco::Path	relative(req.getURI().substr(URI.length()));
		std::cout << relative.toString() << std::endl;
		Poco::Path	file(m_wwwRoot, relative);
		if (!file.isFile() ||
			!Poco::File(file).exists() ||
			!Poco::File(file).canRead())
		{
			resp.setStatusAndReason(resp.HTTP_FORBIDDEN, resp.HTTP_REASON_FORBIDDEN);
			resp.send();
			return;
		}
		resp.sendFile(file.toString(), GetContentType(file.getExtension()));
	}
	catch (std::exception& e)
	{
		resp.setStatusAndReason(resp.HTTP_INTERNAL_SERVER_ERROR, resp.HTTP_REASON_INTERNAL_SERVER_ERROR);
		resp.send() << e.what() << std::endl;
	}
}

void 
CBadRequestHandler::handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp)
{
	resp.setStatusAndReason(resp.HTTP_BAD_REQUEST, resp.HTTP_REASON_BAD_REQUEST);
	resp.send();
}

const std::string CRpcRequestHandler::URI = "/rpc/command";

CRpcRequestHandler::CRpcRequestHandler(std::shared_ptr<IGameController> controller) 
	:	m_Controller(controller)
{
}

void CRpcRequestHandler::handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp)
{
	try
	{
		resp.setContentType("application/json");
		std::string params;
		if (req.getMethod() == "PUT")
		{
			params = std::string(std::istreambuf_iterator<char>(req.stream()), std::istreambuf_iterator<char>());
		}
		std::string Buffer = m_Controller->RpcCall(params);
		resp.sendBuffer(Buffer.data(), Buffer.size());
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
		resp.setStatusAndReason(resp.HTTP_INTERNAL_SERVER_ERROR, resp.HTTP_REASON_INTERNAL_SERVER_ERROR);
		resp.send();
	}
}

const std::string CStateRpcRequestHandler::URI = "/rpc/state";

CStateRpcRequestHandler::CStateRpcRequestHandler(std::shared_ptr<IGameController> controller) 
	:	m_Controller(controller)
{
}

void CStateRpcRequestHandler::handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp)
{
	try
	{
		resp.setContentType("application/json");
		std::string params;
		if (req.getMethod() == "PUT")
		{
			req.stream() >> params;
		}
		resp.send() << m_Controller->GetInterfaceState(params);
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
		resp.setStatusAndReason(resp.HTTP_INTERNAL_SERVER_ERROR, resp.HTTP_REASON_INTERNAL_SERVER_ERROR);
		resp.send();
	}
}

CHTTPRequestHandlerFactory::CHTTPRequestHandlerFactory(const Poco::Path& wwwRoot, std::shared_ptr<IGameController> game) 
	:	m_wwwRoot(wwwRoot)
	,	m_Game(game)
{
}

Poco::Net::HTTPRequestHandler*
CHTTPRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest &req)
{
	std::string Uri = req.getURI();
	std::transform(Uri.begin(), Uri.end(), Uri.begin(), ::tolower);
	if (!strncmp(CStaticFileRequestHandler::URI.c_str(), Uri.c_str(), CStaticFileRequestHandler::URI.length()))
	{
		return new CStaticFileRequestHandler(m_wwwRoot);
	}
	else
	if (!strncmp(CStateRpcRequestHandler::URI.c_str(), Uri.c_str(), CRpcRequestHandler::URI.length()))
	{
		return new CStateRpcRequestHandler(m_Game);
	}
	else
	if (!strncmp(CRpcRequestHandler::URI.c_str(), Uri.c_str(), CRpcRequestHandler::URI.length()))
	{
		return new CRpcRequestHandler(m_Game);
	}
	return new CBadRequestHandler;
}

