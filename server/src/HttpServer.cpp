
#include "stdafx.h"
#include "HttpServer.h"
#include <string> 
#include <algorithm>
#include <Poco/File.h>
#include <sstream>
#include <iostream>

const std::map<std::string, std::string> 
CStaticFileRequestHandler::ContentType = CStaticFileRequestHandler::ContentTypes();

CStaticFileRequestHandler::CStaticFileRequestHandler(const Poco::Path& wwwRoot) 
	:	m_Log(Poco::Logger::get("HTTP.StaticFileRequestHandler"))
	,	m_wwwRoot(wwwRoot)
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
		std::string URI = req.getURI();
		m_Log.debug("URI requested %s", URI);
		if (URI.length() == 0)
		{	//	should not happen ever
			URI = "/";
		}
		if (URI.back() == '/')
		{	//	by default serve directory index.html
			URI += "index.html";
		}
		if (URI[0] != '/') 
		{
			throw std::runtime_error("Unexpected URI format: " + URI);
		}
		URI = URI.substr(1);

		Poco::Path	relative(URI);
		m_Log.information("File requested %s", relative.toString());
		Poco::Path	file(m_wwwRoot, relative);
		if (!file.isFile() ||
			!Poco::File(file).exists() ||
			!Poco::File(file).canRead())
		{
			m_Log.error("File does not exist or is inaccessible: %s", relative.toString());
			resp.setStatusAndReason(resp.HTTP_FORBIDDEN, resp.HTTP_REASON_FORBIDDEN);
			resp.send();
			return;
		}
		resp.sendFile(file.toString(), GetContentType(file.getExtension()));
	}
	catch (std::exception& e)
	{
		resp.setStatusAndReason(resp.HTTP_INTERNAL_SERVER_ERROR, resp.HTTP_REASON_INTERNAL_SERVER_ERROR);
		m_Log.error(e.what());
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
	:	m_Log(Poco::Logger::get("HTTP.RpcRequestHandler"))
	,	m_Controller(controller)
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
		std::string response = m_Controller->RpcCall(params);
		resp.sendBuffer(response.data(), response.size());
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		resp.setStatusAndReason(resp.HTTP_INTERNAL_SERVER_ERROR, resp.HTTP_REASON_INTERNAL_SERVER_ERROR);
		resp.send();
	}
}

const std::string CStateRpcRequestHandler::URI = "/rpc/state";

CStateRpcRequestHandler::CStateRpcRequestHandler(std::shared_ptr<IGameController> controller) 
	:	m_Log(Poco::Logger::get("HTTP.StateRpcRequestHandler"))
	,	m_Controller(controller)
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
			params = std::string(std::istreambuf_iterator<char>(req.stream()), std::istreambuf_iterator<char>());
			m_Log.debug("Request: %s", params);
		}
		std::string response = m_Controller->GetInterfaceState(params);
		m_Log.debug("Response: %s", response);
		resp.sendBuffer(response.data(), response.size());
	}
	catch (std::exception& e)
	{
		m_Log.error(e.what());
		resp.setStatusAndReason(resp.HTTP_INTERNAL_SERVER_ERROR, resp.HTTP_REASON_INTERNAL_SERVER_ERROR);
		resp.send();
	}
}

CHTTPRequestHandlerFactory::CHTTPRequestHandlerFactory(const Poco::Path& wwwRoot, std::shared_ptr<IGameController> game) 
	:	m_Log(Poco::Logger::get("HTTP.RequestFactory"))
	,	m_wwwRoot(wwwRoot)
	,	m_Game(game)
{
}

Poco::Net::HTTPRequestHandler*
CHTTPRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest &req)
{
	std::string Uri = req.getURI();
	m_Log.debug(Uri);
	std::transform(Uri.begin(), Uri.end(), Uri.begin(), ::tolower);
	if (!strncmp(CStateRpcRequestHandler::URI.c_str(), Uri.c_str(), CStateRpcRequestHandler::URI.length()))
	{
		return new CStateRpcRequestHandler(m_Game);
	}
	else
	if (!strncmp(CRpcRequestHandler::URI.c_str(), Uri.c_str(), CRpcRequestHandler::URI.length()))
	{
		return new CRpcRequestHandler(m_Game);
	}
	else
	{
		return new CStaticFileRequestHandler(m_wwwRoot);
	}
	return new CBadRequestHandler;
}

