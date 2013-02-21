#ifndef MAVSERVER_H
#define MAVSERVER_H

#include <winsock2.h> 

#include <boost/thread.hpp>
#include <algorithm>
#include <iostream>
#include <queue>
#include <vector>
#include <thread>

#include <mavlink.h>
#include <json/json.h>

class Client {
public:
	SOCKET sock;
	sockaddr_in address;
	bool connected;

	bool operator== (const Client &cP2) {
		return ((cP2.sock == this->sock) &&
			    (cP2.address.sin_addr.S_un.S_addr == this->address.sin_addr.S_un.S_addr) &&
				(cP2.address.sin_family == this->address.sin_family) &&
				(cP2.address.sin_port == this->address.sin_port) &&
				(cP2.connected == this->connected));
	}
};

class MAVServer {
public:
	MAVServer()
		: m_clients() {
#if defined _WIN32 || _WIN64
		initWinsock();												// WinSock initialisieren, falls n�tig
#endif
		//
		m_myAdr.sin_addr.s_addr = INADDR_ANY;						// Auf jeder IP dieses Rechners lauschend...
		m_myAdr.sin_family = AF_INET;								// mit der Adressfamilie "Internet"...
		m_myAdr.sin_port = ::htons(58147);							// auf Port 58147....
		//
		m_sock = ::socket(AF_INET, SOCK_STREAM, 0);					// erstellen wir einen TCP-Socket
		::bind(m_sock, (sockaddr *)&m_myAdr, sizeof(m_myAdr));		// und binden ihn an die Adresse
	}

	/**
	 *	Startet den Listen-Thread. WICHTIG!!! BLOCKIEREND!!!
	 */
	void run() {
		::listen(m_sock, 8);
		m_listenThread = boost::thread(std::bind(&MAVServer::listenMethod, this));
		m_listenThread.join();
	}

	/**
	 *	Startet den Listen-Thread
	 */
	void async_run() {
		::listen(m_sock, 8);
		m_listenThread = boost::thread(std::bind(&MAVServer::listenMethod, this));

	}

	void sendMessage(mavlink_message_t msg) {
		Json::Value root;
		Json::FastWriter writer;
		std::string str;
		//
		root["msgId"] = getMavlinkMessageName(msg);
		root["content"] = getMavlinkParams(msg);
		//
		str = writer.write(root);
		//
		std::for_each(m_clients.begin(), m_clients.end(), [&] (Client &c) {
			int bytesSent = send(c.sock, str.c_str(), str.size(), 0);
			if (bytesSent > 0) {
				std::cout << "\tSent " << bytesSent << " to " << inet_ntoa(c.address.sin_addr) << "!" << std::endl;
			} else {
				std::cout << "Client " << inet_ntoa(c.address.sin_addr) << " disconnected!" << std::endl;
				//
				c.connected = false;
				m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), c));
			}
		});
	}
	
private:
	#if defined _WIN32 || _WIN64
		/**
		 *	Initialisiere WinSock unter Windows
		 */
		void initWinsock() {
			WSADATA wsaData;
			WSAStartup(MAKEWORD(2, 2), &wsaData);
		}
	#endif

	void listenMethod() {
		while (1) {
			int addrlen;
			sockaddr_in address;
			SOCKET new_socket;

			addrlen = sizeof(address);
			//
			new_socket = accept(m_sock, (sockaddr *)&address, &addrlen);

			if (new_socket != SOCKET_ERROR) {
				Client cl;
				cl.sock = new_socket;
				cl.address = address;
				//
				m_clients.push_back(cl);
				//
				std::thread clientThread(std::bind(&MAVServer::clientListenThread, this, std::ref(m_clients.back())));
				clientThread.detach();
				//
				std::cout << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port)
						  << " connected to server with ID"
						  << m_clients.size() << "!" << std::endl;
			} else {
				throw new std::exception("can't accept()!");
			}
		}
	}

	void clientListenThread(Client &cl) {
		while (cl.connected) {
			std::string inStr;
			char inBuf[1];
			int retval = 0;
			//
			while ((retval = recv(cl.sock, inBuf, sizeof(inBuf) * sizeof(char), 0)) != -1) {
				inStr += inBuf[0];
			}
			//
			std::cout << "Client " << inet_ntoa(cl.address.sin_addr) << " sendet: " << std::endl;
			std::cout << inStr << std::endl;
		}
	}

	SOCKET m_sock;
	sockaddr_in m_myAdr;
	boost::thread m_listenThread;

	std::vector< Client > m_clients;

	/*
	 *	Autogenerated!!!
	 *	Do not change!
	 */
	std::string getMavlinkMessageName(mavlink_message_t msg);

	/*
	 *	Autogenerated!!!
	 *	Do not change!
	 */
	Json::Value getMavlinkParams(mavlink_message_t msg);
};

#endif