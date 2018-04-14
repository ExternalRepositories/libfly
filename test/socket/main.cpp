#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "fly/fly.h"
#include "fly/concurrency/concurrent_queue.h"
#include "fly/config/config_manager.h"
#include "fly/logger/logger.h"
#include "fly/socket/async_request.h"
#include "fly/socket/socket.h"
#include "fly/socket/socket_manager.h"
#include "fly/string/string.h"

#ifdef FLY_LINUX
    #include "test/mock/mock_system.h"
#endif

namespace
{
    static std::string s_largeMessage;
    static std::string s_smallMessage;
}

//==============================================================================
class SocketTest : public ::testing::Test
{
public:
    SocketTest() :
        m_spConfigManager(std::make_shared<fly::ConfigManager>(
            fly::ConfigManager::CONFIG_TYPE_INI, std::string(), std::string()
        )),

        m_spServerSocketManager(std::make_shared<fly::SocketManagerImpl>(m_spConfigManager)),
        m_spClientSocketManager(std::make_shared<fly::SocketManagerImpl>(m_spConfigManager)),

        m_host("localhost"),
        m_port(12390)
    {
        if (s_largeMessage.empty())
        {
            s_largeMessage = fly::String::GenerateRandomString((128 << 20) - 1);
        }
        if (s_smallMessage.empty())
        {
            s_smallMessage = fly::String::GenerateRandomString((64 << 10) - 1);
        }
    }

    virtual void ServerThread(bool)
    {
        ASSERT_TRUE(false);
    };

    virtual void ClientThread(bool)
    {
        ASSERT_TRUE(false);
    };

protected:
    /**
     * Start the socket managers.
     */
    void SetUp()
    {
        m_spServerSocketManager->Start();
        m_spClientSocketManager->Start();
    }

    /**
     * Stop the socket managers.
     */
    void TearDown()
    {
        m_spClientSocketManager->Stop();
        m_spServerSocketManager->Stop();
    }

    /**
     * Create either a synchronous or an asynchronous socket.
     */
    fly::SocketPtr CreateSocket(
        const fly::SocketManagerPtr &spSocketManager,
        bool doAsync,
        bool isTcp
    )
    {
        fly::SocketPtr spSocket;

        if (doAsync)
        {
            fly::SocketWPtr wpSocket = (isTcp ? spSocketManager->CreateAsyncTcpSocket() : spSocketManager->CreateAsyncUdpSocket());
            spSocket = wpSocket.lock();
        }
        else
        {
            spSocket = (isTcp ? spSocketManager->CreateTcpSocket() : spSocketManager->CreateUdpSocket());
        }

        return spSocket;
    }

    fly::ConfigManagerPtr m_spConfigManager;

    fly::SocketManagerPtr m_spServerSocketManager;
    fly::SocketManagerPtr m_spClientSocketManager;

    fly::ConcurrentQueue<int> m_eventQueue;

    std::string m_host;
    int m_port;
};

#ifdef FLY_LINUX

/**
 * Test handling for when socket creation fails.
 */
TEST_F(SocketTest, MockCreateSocketTest)
{
    {
        fly::MockSystem mock(fly::MockCall::SOCKET);

        ASSERT_FALSE(CreateSocket(m_spServerSocketManager, false, true));
        ASSERT_FALSE(CreateSocket(m_spServerSocketManager, false, false));

        ASSERT_FALSE(CreateSocket(m_spServerSocketManager, true, true));
        ASSERT_FALSE(CreateSocket(m_spServerSocketManager, true, false));
    }
    {
        fly::MockSystem mock(fly::MockCall::FCNTL);

        ASSERT_TRUE(CreateSocket(m_spServerSocketManager, false, true));
        ASSERT_TRUE(CreateSocket(m_spServerSocketManager, false, false));

        ASSERT_FALSE(CreateSocket(m_spServerSocketManager, true, true));
        ASSERT_FALSE(CreateSocket(m_spServerSocketManager, true, false));
    }
}

/**
 * Test handling for when socket binding fails.
 */
TEST_F(SocketTest, MockBindTest)
{
    {
        fly::MockSystem mock(fly::MockCall::BIND);

        fly::SocketPtr spSocket = CreateSocket(m_spServerSocketManager, false, true);

        ASSERT_FALSE(spSocket->BindForReuse(fly::Socket::InAddrAny(), m_port));
        ASSERT_FALSE(spSocket->Bind(fly::Socket::InAddrAny(), m_port));
    }
    {
        fly::MockSystem mock(fly::MockCall::SETSOCKOPT);

        fly::SocketPtr spSocket = CreateSocket(m_spServerSocketManager, false, true);

        ASSERT_FALSE(spSocket->BindForReuse(fly::Socket::InAddrAny(), m_port));
    }
}

/**
 * Test handling for when socket listening fails.
 */
TEST_F(SocketTest, MockListenTest)
{
    fly::MockSystem mock(fly::MockCall::LISTEN);

    fly::SocketPtr spSocket = CreateSocket(m_spServerSocketManager, false, true);

    ASSERT_TRUE(spSocket->BindForReuse(fly::Socket::InAddrAny(), m_port));
    ASSERT_FALSE(spSocket->Listen());
}

/**
 * Test handling for when socket connecting fails.
 */
TEST_F(SocketTest, MockConnectTest)
{
    fly::SocketPtr spServerSocket = CreateSocket(m_spServerSocketManager, true, true);

    ASSERT_TRUE(spServerSocket->BindForReuse(fly::Socket::InAddrAny(), m_port));
    ASSERT_TRUE(spServerSocket->Listen());

    fly::SocketManager::SocketCallback callback([&](fly::SocketPtr spSocket)
    {
        m_eventQueue.Push(spSocket->GetSocketId());
    });

    m_spClientSocketManager->SetClientCallbacks(nullptr, callback);

    {
        fly::MockSystem mock(fly::MockCall::CONNECT);

        fly::SocketPtr spClientSocket = CreateSocket(m_spClientSocketManager, false, true);
        ASSERT_FALSE(spClientSocket->Connect(m_host, m_port));
    }

    {
        fly::MockSystem mock(fly::MockCall::CONNECT);

        fly::SocketPtr spClientSocket = CreateSocket(m_spClientSocketManager, true, true);

        fly::Socket::ConnectedState state = spClientSocket->ConnectAsync(m_host, m_port);
        ASSERT_EQ(state, fly::Socket::ConnectedState::NOT_CONNECTED);
    }

    {
        fly::MockSystem mock(fly::MockCall::GETSOCKOPT);

        fly::SocketPtr spClientSocket = CreateSocket(m_spClientSocketManager, true, true);

        fly::Socket::ConnectedState state = spClientSocket->ConnectAsync(m_host, m_port);
        ASSERT_NE(state, fly::Socket::ConnectedState::NOT_CONNECTED);

        if (state == fly::Socket::ConnectedState::CONNECTING)
        {
            int item = 0;
            std::chrono::seconds waitTime(120);

            while (item != spClientSocket->GetSocketId())
            {
                ASSERT_TRUE(m_eventQueue.Pop(item, waitTime));
            }
        }

        ASSERT_FALSE(spClientSocket->IsConnected());
        ASSERT_FALSE(spClientSocket->IsValid());
    }
}

/**
 * Test handling for when socket accepting fails.
 */
TEST_F(SocketTest, MockAcceptTest)
{
    fly::MockSystem mock(fly::MockCall::ACCEPT);

    fly::SocketPtr spSocket = CreateSocket(m_spServerSocketManager, false, true);

    ASSERT_TRUE(spSocket->BindForReuse(fly::Socket::InAddrAny(), m_port));
    ASSERT_TRUE(spSocket->Listen());
    ASSERT_FALSE(spSocket->Accept());
}

#endif

//==============================================================================
class TcpSocketTest : public SocketTest
{
public:
    TcpSocketTest() : SocketTest()
    {
    }

    /**
     * Thread to run server functions do handle accepting a client socket and
     * receiving data from it.
     */
    void ServerThread(bool doAsync)
    {
        fly::SocketPtr spAcceptSocket = CreateSocket(m_spServerSocketManager, doAsync, true);

        ASSERT_TRUE(spAcceptSocket && spAcceptSocket->IsValid());
        ASSERT_EQ(spAcceptSocket->IsAsync(), doAsync);
        ASSERT_GE(spAcceptSocket->GetSocketId(), 0);
        ASSERT_TRUE(spAcceptSocket->IsTcp());
        ASSERT_FALSE(spAcceptSocket->IsUdp());

        ASSERT_TRUE(spAcceptSocket->BindForReuse(fly::Socket::InAddrAny(), m_port));
        ASSERT_TRUE(spAcceptSocket->Listen());
        m_eventQueue.Push(1);

        if (doAsync)
        {
            fly::AsyncRequest request;
            std::chrono::seconds waitTime(120);

            ASSERT_TRUE(m_spServerSocketManager->WaitForCompletedReceive(request, waitTime));
            ASSERT_EQ(s_largeMessage.length(), request.GetRequest().length());
            ASSERT_EQ(s_largeMessage, request.GetRequest());

            ASSERT_GE(request.GetSocketId(), 0);
        }
        else
        {
            fly::SocketPtr spRecvSocket = spAcceptSocket->Accept();
            ASSERT_EQ(spRecvSocket->Recv(), s_largeMessage);

            ASSERT_GT(spRecvSocket->GetClientIp(), 0);
            ASSERT_GT(spRecvSocket->GetClientPort(), 0);
            ASSERT_GE(spRecvSocket->GetSocketId(), 0);
            ASSERT_TRUE(spRecvSocket->IsTcp());
            ASSERT_FALSE(spRecvSocket->IsUdp());
        }
    }

    /**
     * Thread to run client functions to connect to the server socket and send
     * data to it.
     */
    void ClientThread(bool doAsync)
    {
        fly::SocketPtr spSendSocket = CreateSocket(m_spClientSocketManager, doAsync, true);

        ASSERT_TRUE(spSendSocket && spSendSocket->IsValid());
        ASSERT_EQ(spSendSocket->IsAsync(), doAsync);
        ASSERT_GE(spSendSocket->GetSocketId(), 0);
        ASSERT_TRUE(spSendSocket->IsTcp());
        ASSERT_FALSE(spSendSocket->IsUdp());

        int item = 0;
        std::chrono::seconds waitTime(120);
        ASSERT_TRUE(m_eventQueue.Pop(item, waitTime));

        fly::SocketManager::SocketCallback callback([&](...) { m_eventQueue.Push(1); } );
        m_spClientSocketManager->SetClientCallbacks(callback, nullptr);

        if (doAsync)
        {
            fly::Socket::ConnectedState state = spSendSocket->ConnectAsync(m_host, m_port);
            ASSERT_NE(state, fly::Socket::ConnectedState::NOT_CONNECTED);

            if (state == fly::Socket::ConnectedState::CONNECTING)
            {
                ASSERT_TRUE(m_eventQueue.Pop(item, waitTime));
                ASSERT_TRUE(spSendSocket->IsConnected());
            }

            ASSERT_TRUE(spSendSocket->SendAsync(s_largeMessage));

            fly::AsyncRequest request;
            std::chrono::seconds waitTime(120);

            ASSERT_TRUE(m_spClientSocketManager->WaitForCompletedSend(request, waitTime));
            ASSERT_EQ(s_largeMessage.length(), request.GetRequest().length());
            ASSERT_EQ(s_largeMessage, request.GetRequest());

            ASSERT_EQ(request.GetSocketId(), spSendSocket->GetSocketId());
        }
        else
        {
            ASSERT_TRUE(spSendSocket->Connect(m_host, m_port));
            ASSERT_EQ(spSendSocket->Send(s_largeMessage), s_largeMessage.length());
        }

        m_spClientSocketManager->ClearClientCallbacks();
    }
};

//==============================================================================
class UdpSocketTest : public SocketTest
{
public:
    UdpSocketTest() : SocketTest()
    {
    }

    /**
     * Thread to run server functions do handle accepting a client socket and
     * receiving data from it.
     */
    void ServerThread(bool doAsync)
    {
        fly::SocketPtr spRecvSocket = CreateSocket(m_spServerSocketManager, doAsync, false);

        ASSERT_TRUE(spRecvSocket && spRecvSocket->IsValid());
        ASSERT_EQ(spRecvSocket->IsAsync(), doAsync);
        ASSERT_GE(spRecvSocket->GetSocketId(), 0);
        ASSERT_FALSE(spRecvSocket->IsTcp());
        ASSERT_TRUE(spRecvSocket->IsUdp());

        ASSERT_TRUE(spRecvSocket->BindForReuse(fly::Socket::InAddrAny(), m_port));
        m_eventQueue.Push(1);

        if (doAsync)
        {
            fly::AsyncRequest request;
            std::chrono::seconds waitTime(120);

            ASSERT_TRUE(m_spServerSocketManager->WaitForCompletedReceive(request, waitTime));
            ASSERT_EQ(s_smallMessage, request.GetRequest());

            ASSERT_EQ(request.GetSocketId(), spRecvSocket->GetSocketId());
        }
        else
        {
            ASSERT_EQ(spRecvSocket->RecvFrom(), s_smallMessage);
        }
    }

    /**
     * Thread to run client functions to connect to the server socket and send
     * data to it.
     */
    void ClientThread(bool doAsync)
    {
        fly::SocketPtr spSendSocket = CreateSocket(m_spClientSocketManager, doAsync, false);

        ASSERT_TRUE(spSendSocket && spSendSocket->IsValid());
        ASSERT_EQ(spSendSocket->IsAsync(), doAsync);
        ASSERT_GE(spSendSocket->GetSocketId(), 0);
        ASSERT_FALSE(spSendSocket->IsTcp());
        ASSERT_TRUE(spSendSocket->IsUdp());

        int item = 0;
        std::chrono::seconds waitTime(120);
        m_eventQueue.Pop(item, waitTime);

        if (doAsync)
        {
            ASSERT_TRUE(spSendSocket->SendToAsync(s_smallMessage, m_host, m_port));

            fly::AsyncRequest request;
            std::chrono::seconds waitTime(120);

            ASSERT_TRUE(m_spClientSocketManager->WaitForCompletedSend(request, waitTime));
            ASSERT_EQ(s_smallMessage, request.GetRequest());

            ASSERT_EQ(request.GetSocketId(), spSendSocket->GetSocketId());
        }
        else
        {
            ASSERT_EQ(spSendSocket->SendTo(s_smallMessage, m_host, m_port), s_smallMessage.length());
        }
    }
};

/**
 * Test a synchronous server with a synchronous client.
 */
TEST_F(TcpSocketTest, SyncServer_SyncClient_Test)
{
    auto server = std::async(std::launch::async, &TcpSocketTest::ServerThread, this, false);
    auto client = std::async(std::launch::async, &TcpSocketTest::ClientThread, this, false);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}

/**
 * Test an asynchronous server with a synchronous client.
 */
TEST_F(TcpSocketTest, AsyncServer_SyncClient_Test)
{
    auto server = std::async(std::launch::async, &TcpSocketTest::ServerThread, this, true);
    auto client = std::async(std::launch::async, &TcpSocketTest::ClientThread, this, false);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}

/**
 * Test a synchronous server with an asynchronous client.
 */
TEST_F(TcpSocketTest, SyncServer_AsyncClient_Test)
{
    auto server = std::async(std::launch::async, &TcpSocketTest::ServerThread, this, false);
    auto client = std::async(std::launch::async, &TcpSocketTest::ClientThread, this, true);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}

/**
 * Test an asynchronous server with an asynchronous client.
 */
TEST_F(TcpSocketTest, AsyncServer_AsyncClient_Test)
{
    auto server = std::async(std::launch::async, &TcpSocketTest::ServerThread, this, true);
    auto client = std::async(std::launch::async, &TcpSocketTest::ClientThread, this, true);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}

/**
 * Test a synchronous server with a synchronous client.
 */
TEST_F(UdpSocketTest, SyncServer_SyncClient_Test)
{
    auto server = std::async(std::launch::async, &UdpSocketTest::ServerThread, this, false);
    auto client = std::async(std::launch::async, &UdpSocketTest::ClientThread, this, false);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}

/**
 * Test an asynchronous server with a synchronous client.
 */
TEST_F(UdpSocketTest, AsyncServer_SyncClient_Test)
{
    auto server = std::async(std::launch::async, &UdpSocketTest::ServerThread, this, true);
    auto client = std::async(std::launch::async, &UdpSocketTest::ClientThread, this, false);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}

/**
 * Test a synchronous server with an asynchronous client.
 */
TEST_F(UdpSocketTest, SyncServer_AsyncClient_Test)
{
    auto server = std::async(std::launch::async, &UdpSocketTest::ServerThread, this, false);
    auto client = std::async(std::launch::async, &UdpSocketTest::ClientThread, this, true);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}

/**
 * Test an asynchronous server with an asynchronous client.
 */
TEST_F(UdpSocketTest, AsyncServer_AsyncClient_Test)
{
    auto server = std::async(std::launch::async, &UdpSocketTest::ServerThread, this, true);
    auto client = std::async(std::launch::async, &UdpSocketTest::ClientThread, this, true);

    ASSERT_TRUE(server.valid() && client.valid());
    client.get(); server.get();
}
