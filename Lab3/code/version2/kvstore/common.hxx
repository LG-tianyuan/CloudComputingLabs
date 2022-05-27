/*
 *@Modified by:LG-tianyuan
 *@Date:2022-5-24
*/

#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include "resp.hxx"
using namespace nuraft;

using raft_result = cmd_result< ptr<buffer> >;

struct server_stuff {
    server_stuff()
        : server_id_(1)
        , addr_("localhost")
        , port_(25000)
        , raft_logger_(nullptr)
        , sm_(nullptr)
        , smgr_(nullptr)
        , raft_instance_(nullptr)
        {}

    void reset() {
        raft_logger_.reset();
        sm_.reset();
        smgr_.reset();
        raft_instance_.reset();
    }

    // Server ID.
    int server_id_;

    // Server address.
    std::string addr_;

    // Server port.
    int port_;

    // Endpoint: `<addr>:<port>`.
    std::string endpoint_;

    // Logger.
    ptr<logger> raft_logger_;

    // State machine.
    ptr<state_machine> sm_;

    // State manager.
    ptr<state_mgr> smgr_;

    // Raft launcher.
    raft_launcher launcher_;

    // Raft server instance.
    ptr<raft_server> raft_instance_;
};
static server_stuff stuff;


void add_server(const std::string& cmd,
                const std::vector<std::string>& tokens)
{
    if (tokens.size() < 3) {
        std::cout << "too few arguments" << std::endl;
        return;
    }

    int server_id_to_add = atoi(tokens[1].c_str());
    if ( !server_id_to_add || server_id_to_add == stuff.server_id_ ) {
        std::cout << "wrong server id: " << server_id_to_add << std::endl;
        return;
    }

    std::string endpoint_to_add = tokens[2];
    srv_config srv_conf_to_add( server_id_to_add, endpoint_to_add );
    ptr<raft_result> ret = stuff.raft_instance_->add_srv(srv_conf_to_add);
    if (!ret->get_accepted()) {
        std::cout << "failed to add server: "
                  << ret->get_result_code() << std::endl;
        return;
    }
    std::cout << "async request is in progress (check with `list` command)"
              << std::endl;
}

void server_list()
{
    std::vector< ptr<srv_config> > configs;
    stuff.raft_instance_->get_srv_config_all(configs);

    int leader_id = stuff.raft_instance_->get_leader();

    for (auto& entry: configs) {
        ptr<srv_config>& srv = entry;
        std::cout
            << "server id " << srv->get_id()
            << ": " << srv->get_endpoint();
        if (srv->get_id() == leader_id) {
            std::cout << " (LEADER)";
        }
        std::cout << std::endl;
    }
}

std::string do_cmd(const std::vector<std::string>& tokens);

//按空格拆分命令，tokenization
std::vector<std::string> tokenize(const char* str, char c = ' ') {
    std::vector<std::string> tokens;
    do {
        const char *begin = str;
        while(*str != c && *str) str++;
        if (begin != str) tokens.push_back( std::string(begin, str) );
    } while (0 != *str++);

    return tokens;
}

void loop(int server_sockfd) {
    while(true){
        struct sockaddr_in client_addr;
        unsigned int sin_size=sizeof(struct sockaddr_in);
        int cfd;
        if((cfd=accept(server_sockfd,(struct sockaddr *)&client_addr,&sin_size))<0)
        {
            perror("Error: accept");
            return;
        }
        while(true){
            char buf[1024]={'\0'};
            //设置60秒，超过60秒断开连接
            struct timeval timeout = {60,0};
            setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            int len = 0;
            len = recv(cfd,buf,1024,0);
            if(len>0){
                if(strcmp(buf,"exit")==0){
                    break;
                }
                if(stuff.server_id_ == stuff.raft_instance_->get_leader()){
                    // printf("Receive from client: %s\n",buf);
                    std::string cmd = DecodeArrays(std::string(buf));
                    // std::cout<<cmd<<std::endl;
                    std::vector<std::string> tokens = tokenize(cmd.c_str());
                    std::string result = do_cmd(tokens);
                    if(tokens[0]=="SET"){
                        if(result=="OK"){
                            result = EncodeSuccessMsg(result);
                        }else{
                            result = EncodeErrorMsg(result);
                        }
                    }else if(tokens[0]=="GET"){
                        result = EncodeArray(result);
                    }else if(tokens[0]=="DELETE"){
                        result = EncodeInteger(atoi(result.c_str()));
                    }else{
                        result = EncodeErrorMsg("ERROR");
                    }
                    send(cfd,result.c_str(),result.length(),0);
                    // printf("Leader:response %s\n",result.c_str());
                }else{
                    std::string result = "-1";
                    send(cfd,result.c_str(),result.length(),0);
                    // printf("Not Leader:response -1.\n");
                }
            }else if(len==-1){
                printf("Time out!\n");
                break;
            }   
        }
    }
}

void init_raft(ptr<state_machine> sm_instance) {
    // Logger.
    std::string log_file_name = "./srv" +
                                std::to_string( stuff.server_id_ ) +
                                ".log";
    ptr<logger_wrapper> log_wrap = cs_new<logger_wrapper>( log_file_name, 4 );
    stuff.raft_logger_ = log_wrap;

    // State machine.
    stuff.smgr_ = cs_new<inmem_state_mgr>( stuff.server_id_,
                                           stuff.endpoint_ );
    // State manager.
    stuff.sm_ = sm_instance;

    // ASIO options.
    asio_service::options asio_opt;
    asio_opt.thread_pool_size_ = 4;

    // Raft parameters.
    raft_params params;
#if defined(WIN32) || defined(_WIN32)
    // heartbeat: 1 sec, election timeout: 2 - 4 sec.
    params.heart_beat_interval_ = 1000;
    params.election_timeout_lower_bound_ = 2000;
    params.election_timeout_upper_bound_ = 4000;
#else
    // heartbeat: 100 ms, election timeout: 200 - 400 ms.
    params.heart_beat_interval_ = 100;
    params.election_timeout_lower_bound_ = 200;
    params.election_timeout_upper_bound_ = 400;
#endif
    // Upto 1000 logs will be preserved ahead the last snapshot.
    params.reserved_log_items_ = 1000;
    // Snapshot will be created for every 5 log appends.
    params.snapshot_distance_ = 10;
    // Client timeout: 3000 ms.
    params.client_req_timeout_ = 3000;
    // According to this method, `append_log` function
    // should be handled differently.
    params.return_method_ = CALL_TYPE;

    // Initialize Raft server.
    stuff.raft_instance_ = stuff.launcher_.init(stuff.sm_,
                                                stuff.smgr_,
                                                stuff.raft_logger_,
                                                stuff.port_,
                                                asio_opt,
                                                params);
    if (!stuff.raft_instance_) {
        std::cerr << "Failed to initialize launcher (see the message "
                     "in the log file)." << std::endl;
        log_wrap.reset();
        exit(-1);
    }

    // Wait until Raft server is ready (upto 5 seconds).
    const size_t MAX_TRY = 20;
    std::cout << "init Raft instance ";
    for (size_t ii=0; ii<MAX_TRY; ++ii) {
        if (stuff.raft_instance_->is_initialized()) {
            std::cout << " done" << std::endl;
            return;
        }
        std::cout << ".";
        fflush(stdout);
        TestSuite::sleep_ms(250);
    }
    std::cout << " FAILED" << std::endl;
    log_wrap.reset();
    exit(-1);
}

void usage(int argc, char** argv) {
    std::stringstream ss;
    ss << "Usage: \n";
    ss << "    " << argv[0] << " <server id> <IP address and port>";
    ss << std::endl;

    std::cout << ss.str();
    exit(0);
}

int set_server_info(int argc, char** argv) {
    // Get server ID.
    stuff.server_id_ = atoi(argv[1]);
    if (stuff.server_id_ < 1) {
        std::cerr << "wrong server id (should be >= 1): "
                  << stuff.server_id_ << std::endl;
        usage(argc, argv);
    }

    // Get server address and port.
    std::string str = argv[2];
    size_t pos = str.rfind(":");
    if (pos == std::string::npos) {
        std::cerr << "wrong endpoint: " << str << std::endl;
        usage(argc, argv);
    }

    stuff.port_ = atoi(str.substr(pos + 1).c_str());
    if (stuff.port_ < 1000) {
        std::cerr << "wrong port (should be >= 1000): "
                  << stuff.port_ << std::endl;
        usage(argc, argv);
    }

    stuff.addr_ = str.substr(0, pos);
    stuff.endpoint_ = stuff.addr_ + ":" + std::to_string(stuff.port_);

    // create socket
    int port=9000 + stuff.server_id_;
    
    //创建套接字
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");   //s_addr按照网络字节顺序存储IP地址                        
    server_addr.sin_port = htons(port);
    int server_sockfd;
    if((server_sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
    {  
        perror("socket");
        return 1;
    }
    int on = 1;
    if((setsockopt(server_sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)  
    {  
        perror("setsockopt failed");  
        return 1;  
    }   
    if (bind(server_sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr))<0) //绑定
    {
        perror("bind");
        return 1;
    }
    listen(server_sockfd, 1000);
    return server_sockfd;
}

