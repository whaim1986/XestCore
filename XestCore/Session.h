#ifndef _SESSION_H_
#define _SESSION_H_

#include"stdxafx.h"

typedef std::shared_ptr<tcp::socket> sptr_Socket;

class Session {
private:
	sptr_Socket m_spSocket;                //Socket

	boost::asio::streambuf wsbuf;       //д����Ϣ������//x
	boost::asio::streambuf m_ReadBuffer;       //��ȡ��Ϣ������
	string m_WriteBuffer;                     //д����Ϣ������
public:
	~Session();
	Session(sptr_Socket ps);
	bool isConnected();
	void do_read() throw();
	void do_write(string) throw();
	string getAddress();
	int getPort();
	void start();                       //��ʼ��Ϣѭ��
};

#endif // !_SESSION_HPP_