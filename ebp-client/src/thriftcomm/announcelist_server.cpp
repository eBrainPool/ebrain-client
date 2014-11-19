#include "gen-cpp/announcelist.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include <iostream>
#include <stdexcept>
#include <sstream>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

using namespace boost;

using namespace std;

class announcelistHandler : virtual public announcelistIf {
 public:
  announcelistHandler() {
    // Your initialization goes here
  }

  bool sendlist(const applist& apps) {
    // Your implementation goes here
        this->name = apps.name;
        this->command = apps.command;
        //list.push_back(this->name);
	cout << name << " := " << command << endl;
  }
  
 private:
   string name;
   string command;
   
   vector<announcelistHandler> list;

};

extern "C" int ebpserver(int argc, char **argv) {
  int port = 9090;
  shared_ptr<announcelistHandler> handler(new announcelistHandler());
  shared_ptr<TProcessor> processor(new announcelistProcessor(handler));
  shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
  shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
  server.serve();

  return 0;

}

