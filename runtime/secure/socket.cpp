#include <cstdlib>
#include <string>
#include "lib.h"
#include "socket.h"

static seq_str_t string_conv(const std::string& s)
{
	auto len = s.length();
	auto *buf = (char *)seq_alloc_atomic(len);
	std::memcpy(buf, s.data(), len);
	return {(seq_int_t)len, buf};
}

SEQ_FUNC void seq_socket_del(void *sock, void *unused)
{
	((CSocket *)sock)->~CSocket();
}

SEQ_FUNC void *seq_socket_new()
{
	void *sock = seq_alloc(sizeof(CSocket));
	seq_register_finalizer(sock, seq_socket_del);
	return (void *)(new (sock) CSocket());
}

SEQ_FUNC bool seq_socket_socket(void *sock)
{
	return ((CSocket *)sock)->Socket();
}

SEQ_FUNC void seq_socket_close(void *sock)
{
	((CSocket *)sock)->Close();
}

SEQ_FUNC void seq_socket_attach_from(void *sock, void *other)
{
	((CSocket *)sock)->AttachFrom(*(CSocket *)other);
}

SEQ_FUNC void seq_socket_detach(void *sock)
{
	((CSocket *)sock)->Detach();
}

SEQ_FUNC seq_str_t seq_socket_get_ip(void *sock)
{
	return string_conv(((CSocket *)sock)->GetIP());
}

SEQ_FUNC seq_int_t seq_socket_get_port(void *sock)
{
	return ((CSocket *)sock)->GetPort();
}

SEQ_FUNC seq_int_t seq_socket_bind(void *sock, seq_str_t ip, seq_int_t port)
{
	return ((CSocket *)sock)->Bind((uint16_t)port, std::string(ip.str, (size_t)ip.len));
}

SEQ_FUNC bool seq_socket_listen(void *sock, seq_int_t q)
{
	return ((CSocket *)sock)->Listen((int)q);
}

SEQ_FUNC bool seq_socket_accept(void *sock, void *other)
{
	return ((CSocket *)sock)->Accept(*(CSocket *)other);
}

SEQ_FUNC bool seq_socket_connect(void *sock, seq_str_t ip, seq_int_t port, seq_int_t timeout)
{
	return ((CSocket *)sock)->Connect(std::string(ip.str, (size_t)ip.len), (uint16_t)port, (int32_t)timeout);
}

SEQ_FUNC seq_int_t seq_socket_receive(void *sock, char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->Receive(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_send(void *sock, char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->Send(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_receive_secure(void *sock, unsigned char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->ReceiveSecure(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_send_secure(void *sock, unsigned char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->SendSecure(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_get_bytes_received(void *sock)
{
	return ((CSocket *)sock)->GetBytesReceived();
}

SEQ_FUNC seq_int_t seq_socket_get_bytes_sent(void *sock)
{
	return ((CSocket *)sock)->GetBytesSent();
}

SEQ_FUNC void seq_socket_reset_stats(void *sock)
{
	((CSocket *)sock)->ResetStats();
}

SEQ_FUNC bool seq_socket_set_key(void *sock, seq_str_t keyfile)
{
	return ((CSocket *)sock)->SetKey(std::string(keyfile.str, (size_t)keyfile.len));
}

SEQ_FUNC void seq_usleep(seq_int_t time)
{
	usleep((useconds_t)time);
}
