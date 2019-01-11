#ifndef __SOCKET_H_
#define __SOCKET_H_

#include <arpa/inet.h>
#include <cerrno>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "aesstream.h"
#include "crypto.h"
#include "util.h"
#include <openssl/evp.h>
#include <fstream>

typedef int SOCKET;
#define INVALID_SOCKET (-1)

class CSocket {
public:
  CSocket() {
    m_hSock = INVALID_SOCKET;
    bytesSent = 0;
    bytesReceived = 0;
    key_flag = false;
    iv_prf = NULL;
  }

  ~CSocket(){
    free(iv_prf);
  }

public:
  bool Socket() {
    Close();
    return (m_hSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) != INVALID_SOCKET;
  }

  void Close() {
    if( m_hSock == INVALID_SOCKET ) return;

    shutdown(m_hSock, SHUT_WR);
    close(m_hSock);

    m_hSock = INVALID_SOCKET;
  }

  void AttachFrom(CSocket& s) {
    m_hSock = s.m_hSock;
  }

  void Detach() {
    m_hSock = INVALID_SOCKET;
  }

public:
  std::string GetIP() {
    sockaddr_in addr;
    uint32_t addr_len = sizeof(addr);

    if (getsockname(m_hSock, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) return "";
    return inet_ntoa(addr.sin_addr);
  }


  uint16_t GetPort() {
    sockaddr_in addr;
    uint32_t addr_len = sizeof(addr);

    if (getsockname(m_hSock, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) return 0;
    return ntohs(addr.sin_port);
  }

  bool Bind(uint16_t nPort=0, std::string ip = "") {
    // Bind the socket to its port
    sockaddr_in sockAddr;
    memset(&sockAddr,0,sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;

    if( ip != "" ) {
      int on = 1;
      setsockopt(m_hSock, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof(on));

      sockAddr.sin_addr.s_addr = inet_addr(ip.c_str());

      if (sockAddr.sin_addr.s_addr == INADDR_NONE) {
        hostent* phost;
        phost = gethostbyname(ip.c_str());
        if (phost != NULL)
          sockAddr.sin_addr.s_addr = ((in_addr*)phost->h_addr)->s_addr;
        else
          return false;
      }
    }
    else {
      sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    sockAddr.sin_port = htons(nPort);

    return bind(m_hSock, (const sockaddr *) &sockAddr, (socklen_t)sizeof(sockaddr_in)) >= 0;
  }

  bool Listen(int nQLen = 5) {
    return listen(m_hSock, nQLen) >= 0;
  }

  bool Accept(CSocket& sock) {
    sock.m_hSock = accept(m_hSock, NULL, 0);
    if( sock.m_hSock == INVALID_SOCKET ) return false;

    return true;
  }

  bool Connect(std::string ip, uint16_t port, int32_t lTOSMilisec = -1) {
    sockaddr_in sockAddr;
    memset(&sockAddr,0,sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (sockAddr.sin_addr.s_addr == INADDR_NONE) {
      hostent* lphost;
      lphost = gethostbyname(ip.c_str());
      if (lphost != NULL)
        sockAddr.sin_addr.s_addr = ((in_addr*)lphost->h_addr)->s_addr;
      else
        return false;
    }

    sockAddr.sin_port = htons(port);

    timeval tv;

    if (lTOSMilisec > 0) {
      tv.tv_sec = lTOSMilisec/1000;
      tv.tv_usec = (lTOSMilisec%1000)*1000;

      setsockopt(m_hSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    int ret = connect(m_hSock, (sockaddr*)&sockAddr, sizeof(sockAddr));

    if( ret >= 0 && lTOSMilisec > 0 ) {
      tv.tv_sec = 100000;
      tv.tv_usec = 0;

      setsockopt(m_hSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    return ret >= 0;
  }

  int64_t Receive(void *pBuf, int64_t nLen, int nFlags = 0) {
    bytesReceived += nLen;

    char *p = (char *) pBuf;
    int64_t n = nLen;
    int64_t ret = 0;
    while (n > 0) {
      ret = recv(m_hSock, p, n, 0);
      if (ret < 0) {
        if (errno == EAGAIN) {
          std::cerr << "socket recv eror: EAGAIN" << std::endl;
          usleep(200 << 10);
          continue;
        }
        else {
          std::cerr << "socket recv error: " << errno << std::endl;
          return ret;
        }
      } else if (ret == 0) {
        return ret;
      }

      p += ret;
      n -= ret;
    }

    return nLen;
  }

  int64_t Send(const void *pBuf, int64_t nLen, int nFlags = 0) {
    bytesSent += nLen;
    return send(m_hSock, (char*)pBuf, nLen, nFlags);
  }

  // pBuf is encrypted in-place
  // pBuf should have at least nLen + GCM_AUTH_TAG_LEN bytes
  int64_t SendSecure(unsigned char *pBuf, int64_t nLen, int nFlags = 0) {
    assert(key_flag);
    if (!Encrypt(pBuf, nLen)) {
      std::cerr << "CSocket:;SendSecure: encryption failed" << std::endl;
      return -1;
    } else {
      return Send(pBuf, nLen + GCM_AUTH_TAG_LEN, nFlags);
    }
  }

  // pBuf should have at least nLen + GCM_AUTH_TAG_LEN bytes
  int64_t ReceiveSecure(unsigned char *pBuf, int64_t nLen, int nFlags = 0) {
    assert(key_flag);
    int64_t ret = Receive(pBuf, nLen + GCM_AUTH_TAG_LEN, nFlags);
    if (ret <= 0) {
      return ret;
    } else {
      if (!Decrypt(pBuf, nLen)) {
        std::cerr << "CSocket::ReceiveSecure: decryption failed" << std::endl;
        return -1;
      }
      return nLen;
    }
  }

  uint64_t GetBytesSent() {
    return bytesSent;
  }

  uint64_t GetBytesReceived() {
    return bytesReceived;
  }

  void ResetStats() {
    bytesSent = 0;
    bytesReceived = 0;
  }

  bool SetKey(std::string keyfile) {
    std::ifstream ifs(keyfile.c_str(), std::ios::binary);
    if (!ifs.is_open()) {
      std::cerr << "CSocket::SetKey: failed to open key file " << keyfile << std::endl;
      return false;
    }

    if (!ifs.read((char *)key, PRF_KEY_BYTES)) {
      std::cerr << "CSocket::SetKey: failed to read " << PRF_KEY_BYTES <<
              " bytes from " << keyfile << std::endl;
      return false;
    }

    ifs.close();

    AESStream rs(key);

    unsigned char buf[PRF_KEY_BYTES];
    rs.get(buf, PRF_KEY_BYTES);
    iv_prf = new AESStream(buf);

    rs.get(key, PRF_KEY_BYTES);

    key_flag = true;
    return true;
  }

private:
  SOCKET m_hSock;

  uint64_t bytesSent;
  uint64_t bytesReceived;

  bool key_flag;
  unsigned char key[PRF_KEY_BYTES];
  AESStream *iv_prf;

  unsigned char iv[GCM_IV_LEN];

  // Both parties share the same PRF;
  // they obtain the same IV for each Encrypt/Decrypt pair
  // by invoking this function individually
  void UpdateIV() {
    assert(iv_prf != NULL);
    iv_prf->get(iv, GCM_IV_LEN);
  }

  // in-place encryption; authentication tag is appended to the ciphertext
  // length of the tag is GCM_AUTH_TAG_LEN bytes (defined in crypto.h)
  bool Encrypt(unsigned char *plaintext, int plaintext_len)
  {
    UpdateIV();

    unsigned char *ciphertext = plaintext;

    EVP_CIPHER_CTX *ctx;

    int len;
    int ciphertext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
      return false;
    }

    /* Initialise the encryption operation. */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
      return false;
    }

    /* Initialise key and IV */
    if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
      return false;
    }

    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
      return false;
    }

    ciphertext_len = len;

    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
      return false;
    }

    ciphertext_len += len;
    assert(ciphertext_len == plaintext_len);

    /* Append the authentication tag */
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_AUTH_TAG_LEN,
                                 ciphertext + ciphertext_len)) {
      return false;
    }

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return true;
  }

  // in-place decryption; authentication tag at the end of ciphertext not included in ciphertext_len
  // length of the tag is GCM_AUTH_TAG_LEN bytes (defined in crypto.h)
  bool Decrypt(unsigned char *ciphertext, int ciphertext_len)
  {
    UpdateIV();

    unsigned char *tag = ciphertext + ciphertext_len;
    unsigned char *plaintext = ciphertext;

    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    int ret;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
      return false;
    }

    /* Initialise the decryption operation. */
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
      return false;
    }

    /* Initialise key and IV */
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) {
      return false;
    }

    /* Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) {
      return false;
    }

    plaintext_len = len;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_AUTH_TAG_LEN, tag)) {
      return false;
    }

    /* Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ret > 0;
  }
};


#endif
