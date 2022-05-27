/*
 *@Author:LG-tianyuan
 *@Date:2022-5-19
 *@Last-Update:2022-5-22
*/

#include "kvstore_state_machine.hxx"
#include "in_memory_state_mgr.hxx"
#include "logger_wrapper.hxx"

#include "nuraft.hxx"

#include "test_common.h"

#include <iostream>
#include <sstream>

#include <stdio.h>

using namespace nuraft;

namespace kvstoresystem {

static raft_params::return_method_type CALL_TYPE
    = raft_params::blocking;

static bool ASYNC_SNAPSHOT_CREATION = false;

#include "common.hxx"

kvstore_state_machine* get_sm() {
    return static_cast<kvstore_state_machine*>( stuff.sm_.get() );
}

void print_result(ptr<TestSuite::Timer> timer){
    std::cout << "succeeded, "
              << TestSuite::usToString( timer->getTimeUs() )
              << ", get result: "
              << get_sm()->get_latest_result()
              << ", state machine value: "
              << std::endl;
    get_sm()->get_latest_kvstore();
}

void handle_result(ptr<TestSuite::Timer> timer,
                   raft_result& result,
                   ptr<std::exception>& err)
{
    if (result.get_result_code() != cmd_result_code::OK) {
        // Something went wrong.
        // This means committing this log failed,
        // but the log itself is still in the log store.
        std::cout << "failed: " << result.get_result_code() << ", "
                  << TestSuite::usToString( timer->getTimeUs() )
                  << std::endl;
        return;
    }
    ptr<buffer> buf = result.get();
    uint64_t ret_value = buf->get_ulong();
    std::cout << "succeeded, "
              << TestSuite::usToString( timer->getTimeUs() )
              << ", return value: "
              << ret_value
              << ", state machine value: "
              << get_sm()->get_latest_result()
              << std::endl;
    get_sm()->get_latest_kvstore();
}


void append_log(const std::string& cmd,
                const std::vector<std::string>& tokens)
{
    std::vector<std::string>::const_iterator begin = tokens.begin()+1;
    std::vector<std::string>::const_iterator end = tokens.end();
    std::vector<std::string> operand(begin,end);  
    kvstore_state_machine::op_type op = kvstore_state_machine::SET;
    if(cmd == "SET"){
        op = kvstore_state_machine::SET;
    }else if(cmd == "DELETE"){
        op = kvstore_state_machine::DELETE;
    }

    //DEBUG
    // std::cout << "Get from screen:" << std::endl;
    // std::cout << "operate:" << op << std::endl;
    // std::cout << "key or value:" << std::endl;
    // for(uint64_t i = 0; i < operand.size(); i++){
    //     std::cout<< operand[i] <<std::endl;
    // }

    // To measure the elapsed time.
    ptr<TestSuite::Timer> timer = cs_new<TestSuite::Timer>();

    // Do append.
    if(cmd == "SET" || cmd == "DELETE"){
        // Serialize and generate Raft log to append.
        size_t size = sizeof(int32_t) + sizeof(uint64_t);
        for(uint64_t i = 0; i < operand.size(); i++)
        {
            size += (sizeof(int) + operand[i].size());
        }
        ptr<buffer> new_log = buffer::alloc(size);
        buffer_serializer bs(new_log);
        bs.put_opstruct(op,operand);

        ptr<raft_result> ret = stuff.raft_instance_->append_entries( {new_log} );
        if (!ret->get_accepted()) {
            // Log append rejected, usually because this node is not a leader.
            std::cout << "failed to replicate: "
                << ret->get_result_code() << ", "
                << TestSuite::usToString( timer->getTimeUs() )
                << std::endl;
            return;
        }
        // Log append accepted, but that doesn't mean the log is committed.
        // Commit result can be obtained below.

        if (CALL_TYPE == raft_params::blocking) {
            // Blocking mode:
            //   `append_entries` returns after getting a consensus,
            //   so that `ret` already has the result from state machine.
            ptr<std::exception> err(nullptr);
            handle_result(timer, *ret, err);

        } else if (CALL_TYPE == raft_params::async_handler) {
            // Async mode:
            //   `append_entries` returns immediately.
            //   `handle_result` will be invoked asynchronously,
            //   after getting a consensus.
            ret->when_ready( std::bind( handle_result,
                                        timer,
                                        std::placeholders::_1,
                                        std::placeholders::_2 ) );
        } else {
            assert(0);
        }
    }else{//GET
        if(stuff.server_id_ == stuff.raft_instance_->get_leader())
        {
            get_sm()->op_GET_result(operand[0]);
            print_result(timer);
        }else{
            std::cout << "Failed: this is not the leader!" << std::endl;
        }
    }
}

void print_status(const std::string& cmd,
                  const std::vector<std::string>& tokens)
{
    ptr<log_store> ls = stuff.smgr_->load_log_store();

    std::cout
        << "my server id: " << stuff.server_id_ << std::endl
        << "leader id: " << stuff.raft_instance_->get_leader() << std::endl
        << "Raft log range: ";
    if (ls->start_index() >= ls->next_slot()) {
        // Start index can be the same as next slot when the log store is empty.
        std::cout << "(empty)" << std::endl;
    } else {
        std::cout << ls->start_index()
                  << " - " << (ls->next_slot() - 1) << std::endl;
    }
    std::cout
        << "last committed index: "
            << stuff.raft_instance_->get_committed_log_idx() << std::endl
        << "current term: "
            << stuff.raft_instance_->get_term() << std::endl;
    if(stuff.sm_->last_snapshot()!=nullptr){
        std::cout
            << "last snapshot log index: "
                << stuff.sm_->last_snapshot()->get_last_log_idx() << std::endl
            << "last snapshot log term: "
                << stuff.sm_->last_snapshot()->get_last_log_term() << std::endl;
    }
    std::cout
        << "state machine value: "
            //<< get_sm()->get_latest_result() 
        << std::endl;
    get_sm()->get_latest_kvstore();
}

void help(const std::string& cmd,
          const std::vector<std::string>& tokens)
{
    std::cout
    << "Usage:\n"
    << "@ set value: SET key value\n"
    << "    e.g. SET CS06142 \"Cloud Computing\"\n"
    << "@ get value: GET key\n"
    << "    e.g. GET CS06142\n"
    << "@ delete value: DELETE key1 key2 ...\n"
    << "    e.g. DELETE CS06142\n"
    << "\n"
    << "add server: add <server id> <address>:<port>\n"
    << "    e.g.) add 2 127.0.0.1:20000\n"
    << "\n"
    << "get current server status: st (or stat)\n"
    << "\n"
    << "get the list of members: ls (or list)\n"
    << "\n";
}

// should modify
bool do_cmd(const std::vector<std::string>& tokens) {
    if (!tokens.size()) return true;

    const std::string& cmd = tokens[0];

    if (cmd == "q" || cmd == "exit") {
        stuff.launcher_.shutdown(5);
        stuff.reset();
        return false;

    } else if ( cmd == "SET" ||
                cmd == "GET" ||
                cmd == "DELETE" ) {
        // e.g.) SET CS06142 CloudComputing
        append_log(cmd, tokens);

    } else if ( cmd == "add" ) {
        // e.g.) add 2 localhost:12345
        add_server(cmd, tokens);

    } else if ( cmd == "st" || cmd == "stat" ) {
        print_status(cmd, tokens);

    } else if ( cmd == "ls" || cmd == "list" ) {
        server_list(cmd, tokens);

    } else if ( cmd == "h" || cmd == "help" ) {
        help(cmd, tokens);
    }
    return true;
}

void check_additional_flags(int argc, char** argv) {
    for (int ii = 1; ii < argc; ++ii) {
        if (strcmp(argv[ii], "--async-handler") == 0) {
            CALL_TYPE = raft_params::async_handler;
        } else if (strcmp(argv[ii], "--async-snapshot-creation") == 0) {
            ASYNC_SNAPSHOT_CREATION = true;
        }
    }
}

void kvstore_usage(int argc, char** argv) {
    std::stringstream ss;
    ss << "Usage: \n";
    ss << "    " << argv[0] << " <server id> <IP address and port> [<options>]";
    ss << std::endl << std::endl;
    ss << "    options:" << std::endl;
    ss << "      --async-handler: use async type handler." << std::endl;
    ss << "      --async-snapshot-creation: create snapshots asynchronously."
       << std::endl << std::endl;

    std::cout << ss.str();
    exit(0);
}

}; // namespace kvstoresystem;
using namespace kvstoresystem;

int main(int argc, char** argv) {
    if (argc < 3) kvstore_usage(argc, argv);

    set_server_info(argc, argv);
    check_additional_flags(argc, argv);

    std::cout << "    -- KV Store System with Raft --" << std::endl;
    std::cout << "                         Version 0.1.0" << std::endl;
    std::cout << "    Server ID:    " << stuff.server_id_ << std::endl;
    std::cout << "    Endpoint:     " << stuff.endpoint_ << std::endl;
    if (CALL_TYPE == raft_params::async_handler) {
        std::cout << "    async handler is enabled" << std::endl;
    }
    if (ASYNC_SNAPSHOT_CREATION) {
        std::cout << "    snapshots are created asynchronously" << std::endl;
    }
    init_raft( cs_new<kvstore_state_machine>(ASYNC_SNAPSHOT_CREATION) );
    loop();

    return 0;
}
