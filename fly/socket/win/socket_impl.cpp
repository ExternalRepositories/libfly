#include "fly/socket/win/socket_impl.h"

#include "fly/logger/logger.h"
#include "fly/socket/socket_config.h"
#include "fly/system/system.h"

#include <WinSock.h>
#include <socketapi.h>

namespace fly {

namespace {

    struct sockaddr_in
    CreateSocketAddress(address_type address, port_type port) noexcept
    {
        struct sockaddr_in socketAddress;
        memset(&socketAddress, 0, sizeof(socketAddress));

        socketAddress.sin_family = AF_INET;
        socketAddress.sin_addr.s_addr = htonl(address);
        socketAddress.sin_port = htons(port);

        return socketAddress;
    }

} // namespace

//==============================================================================
SocketImpl::SocketImpl(
    Protocol protocol,
    const std::shared_ptr<SocketConfig> &spConfig) noexcept :
    Socket(protocol, spConfig)
{
    switch (m_protocol)
    {
        case Protocol::TCP:
            m_socketHandle = ::socket(AF_INET, SOCK_STREAM, 0);
            break;

        case Protocol::UDP:
            m_socketHandle = ::socket(AF_INET, SOCK_DGRAM, 0);
            break;
    }
}

//==============================================================================
SocketImpl::~SocketImpl()
{
    Close();
}

//==============================================================================
bool SocketImpl::HostnameToAddress(
    const std::string &hostname,
    address_type &address) noexcept
{
    struct hostent *ipAddress = ::gethostbyname(hostname.c_str());

    if (ipAddress == NULL)
    {
        LOGS("Error resolving %s", hostname);
        return false;
    }

    memcpy((char *)&address, ipAddress->h_addr, ipAddress->h_length);
    address = ntohl(address);

    LOGD("Converted hostname %s to %d", hostname, address);
    return true;
}

//==============================================================================
address_type SocketImpl::InAddrAny() noexcept
{
    return INADDR_ANY;
}

//==============================================================================
socket_type SocketImpl::InvalidSocket() noexcept
{
    return INVALID_SOCKET;
}

//==============================================================================
void SocketImpl::Close() noexcept
{
    if (IsValid())
    {
        ::closesocket(m_socketHandle);
        m_socketHandle = InvalidSocket();
    }
}

//==============================================================================
bool SocketImpl::IsErrorFree() noexcept
{
    int opt = 0;
    int len = sizeof(opt);

    int ret =
        ::getsockopt(m_socketHandle, SOL_SOCKET, SO_ERROR, (char *)&opt, &len);

    if (ret == SOCKET_ERROR)
    {
        SLOGS(m_socketHandle, "Error getting error flag");
    }

    return opt == 0;
}

//==============================================================================
bool SocketImpl::SetAsync() noexcept
{
    unsigned long nonZero = 1;

    if (::ioctlsocket(m_socketHandle, FIONBIO, &nonZero) == SOCKET_ERROR)
    {
        SLOGS(m_socketHandle, "Error setting async flag");
        return false;
    }

    m_isAsync = true;
    return m_isAsync;
}

//==============================================================================
bool SocketImpl::Bind(address_type address, port_type port, BindOption option)
    const noexcept
{
    static const char bindForReuseOption = 1;
    static const int bindForReuseOptionLength =
        static_cast<int>(sizeof(bindForReuseOption));

    struct sockaddr_in socketAddress = CreateSocketAddress(address, port);
    auto *pSocketAddress = reinterpret_cast<sockaddr *>(&socketAddress);

    switch (option)
    {
        case BindOption::SingleUse:
            break;

        case BindOption::AllowReuse:
            if (::setsockopt(
                    m_socketHandle,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &bindForReuseOption,
                    bindForReuseOptionLength) == SOCKET_ERROR)
            {
                SLOGS(m_socketHandle, "Error setting reuse flag");
                return false;
            }

            break;
    }

    int ret = ::bind(m_socketHandle, pSocketAddress, sizeof(socketAddress));

    if (ret == SOCKET_ERROR)
    {
        SLOGS(m_socketHandle, "Error binding to %d", port);
        return false;
    }

    return true;
}

//==============================================================================
bool SocketImpl::Listen() noexcept
{
    if (::listen(m_socketHandle, 100) == SOCKET_ERROR)
    {
        SLOGS(m_socketHandle, "Error listening");
        return false;
    }

    m_isListening = true;
    return m_isListening;
}

//==============================================================================
bool SocketImpl::Connect(address_type address, port_type port) noexcept
{
    struct sockaddr_in socketAddress = CreateSocketAddress(address, port);
    auto *pSocketAddress = reinterpret_cast<sockaddr *>(&socketAddress);

    int ret = ::connect(m_socketHandle, pSocketAddress, sizeof(socketAddress));

    if (ret == SOCKET_ERROR)
    {
        SLOGS(m_socketHandle, "Error connecting");
        int error = System::GetErrorCode();

        if ((error == WSAEWOULDBLOCK) || (error == WSAEINPROGRESS))
        {
            m_aConnectedState.store(ConnectedState::Connecting);
        }

        return false;
    }

    m_aConnectedState.store(ConnectedState::Connected);
    return true;
}

//==============================================================================
std::shared_ptr<Socket> SocketImpl::Accept() const noexcept
{
    auto ret = std::make_shared<SocketImpl>(m_protocol, m_spConfig);

    struct sockaddr_in socketAddress;
    auto *pSocketAddress = reinterpret_cast<sockaddr *>(&socketAddress);
    int socketAddressLength = static_cast<int>(sizeof(socketAddress));

    socket_type skt =
        ::accept(m_socketHandle, pSocketAddress, &socketAddressLength);

    if (skt == InvalidSocket())
    {
        SLOGS(m_socketHandle, "Error accepting");
        ret.reset();
    }
    else
    {
        SLOGD(
            m_socketHandle,
            "Accepted new socket: %d (%d)",
            ret->GetSocketId(),
            skt);

        ret->m_socketHandle = skt;
        ret->m_clientIp = ntohl(socketAddress.sin_addr.s_addr);
        ret->m_clientPort = ntohs(socketAddress.sin_port);
        ret->m_aConnectedState.store(ConnectedState::Connected);
    }

    return ret;
}

//==============================================================================
size_t SocketImpl::Send(const std::string &message, bool &wouldBlock) const
    noexcept
{
    static const std::string eom(1, m_socketEoM);
    std::string toSend = message + eom;

    bool keepSending = !toSend.empty();
    size_t bytesSent = 0;
    wouldBlock = false;

    while (keepSending)
    {
        // Window's ::send() takes string size as an integer, but std::string's
        // length is size_t - send at most MAX_INT bytes at a time
        static size_t intMax = std::numeric_limits<int>::max();
        int toSendSize = static_cast<int>(std::min(toSend.size(), intMax));

        int currSent = ::send(m_socketHandle, toSend.c_str(), toSendSize, 0);

        if (currSent > 0)
        {
            if (toSend[currSent - 1] == m_socketEoM)
            {
                bytesSent += currSent - 1;
            }
            else
            {
                bytesSent += currSent;
            }

            toSend = toSend.substr(currSent, std::string::npos);
            keepSending = !toSend.empty();
        }
        else
        {
            keepSending = false;

            if (currSent == SOCKET_ERROR)
            {
                wouldBlock = (System::GetErrorCode() == WSAEWOULDBLOCK);
                SLOGS(m_socketHandle, "Error sending");
            }
        }
    }

    return bytesSent;
}

//==============================================================================
size_t SocketImpl::SendTo(
    const std::string &message,
    address_type address,
    port_type port,
    bool &wouldBlock) const noexcept
{
    static const std::string eom(1, m_socketEoM);
    std::string toSend = message + eom;

    bool keepSending = !toSend.empty();
    size_t bytesSent = 0;
    wouldBlock = false;

    struct sockaddr_in socketAddress = CreateSocketAddress(address, port);
    auto *pSocketAddress = reinterpret_cast<sockaddr *>(&socketAddress);

    while (keepSending)
    {
        int currSent = ::sendto(
            m_socketHandle,
            toSend.c_str(),
            static_cast<int>(std::min(m_packetSize, toSend.size())),
            0,
            pSocketAddress,
            sizeof(socketAddress));

        if (currSent > 0)
        {
            if (toSend[currSent - 1] == m_socketEoM)
            {
                bytesSent += currSent - 1;
            }
            else
            {
                bytesSent += currSent;
            }

            toSend = toSend.substr(currSent, std::string::npos);
            keepSending = !toSend.empty();
        }
        else
        {
            keepSending = false;

            if (currSent == SOCKET_ERROR)
            {
                wouldBlock = (System::GetErrorCode() == WSAEWOULDBLOCK);
                SLOGS(m_socketHandle, "Error sending");
            }
        }
    }

    return bytesSent;
}

//==============================================================================
std::string SocketImpl::Recv(bool &wouldBlock, bool &isComplete) const noexcept
{
    std::string ret;

    bool keepReading = true;
    wouldBlock = false;
    isComplete = false;

    const int packetSize = static_cast<int>(m_packetSize);

    while (keepReading)
    {
        char *buff = (char *)calloc(1, m_packetSize * sizeof(char));
        int bytesRead = ::recv(m_socketHandle, buff, packetSize, 0);

        if (bytesRead > 0)
        {
            if (buff[bytesRead - 1] == m_socketEoM)
            {
                keepReading = false;
                isComplete = true;
                --bytesRead;
            }

            ret.append(buff, bytesRead);
        }
        else
        {
            keepReading = false;

            if (bytesRead == SOCKET_ERROR)
            {
                wouldBlock = (System::GetErrorCode() == WSAEWOULDBLOCK);
                SLOGS(m_socketHandle, "Error receiving");
            }
        }

        free(buff);
    }

    return ret;
}

//==============================================================================
std::string SocketImpl::RecvFrom(bool &wouldBlock, bool &isComplete) const
    noexcept
{
    std::string ret;

    bool keepReading = true;
    wouldBlock = false;
    isComplete = false;

    struct sockaddr_in socketAddress;
    auto *pSocketAddress = reinterpret_cast<sockaddr *>(&socketAddress);
    int socketAddressLength = static_cast<int>(sizeof(socketAddress));

    const int packetSize = static_cast<int>(m_packetSize);

    while (keepReading)
    {
        char *buff = (char *)calloc(1, m_packetSize * sizeof(char));

        int bytesRead = ::recvfrom(
            m_socketHandle,
            buff,
            packetSize,
            0,
            pSocketAddress,
            &socketAddressLength);

        if (bytesRead > 0)
        {
            if (buff[bytesRead - 1] == m_socketEoM)
            {
                keepReading = false;
                isComplete = true;
                --bytesRead;
            }

            ret.append(buff, bytesRead);
        }
        else
        {
            keepReading = false;

            if (bytesRead == SOCKET_ERROR)
            {
                wouldBlock = (System::GetErrorCode() == WSAEWOULDBLOCK);
                SLOGS(m_socketHandle, "Error receiving");
            }
        }

        free(buff);
    }

    return ret;
}

} // namespace fly
