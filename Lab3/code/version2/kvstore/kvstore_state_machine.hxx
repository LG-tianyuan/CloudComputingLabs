/*
 *@Author:LG-tianyuan
 *@Date:2022-5-19
 *@Last-Update:2022-5-26
*/

#pragma once

#include "nuraft.hxx"

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <map>
#include <vector>
#include <string.h>

using namespace nuraft;

typedef std::map<std::string,std::string> StrStrMap;
typedef std::pair<std::string,std::string> StrStrPair;

namespace kvstoresystem {

class kvstore_state_machine : public state_machine {
public:
    //构造函数
    kvstore_state_machine(bool async_snapshot = false)
        : cur_value_(1)
        , result_("ERROR")
        , last_committed_idx_(0)
        , async_snapshot_(async_snapshot)
        {}

    //析构函数
    ~kvstore_state_machine() {}

    enum op_type : int32_t {
        SET = 0x0,
        DELETE = 0x1
    };

    struct op_payload {
        op_type type_;
        std::vector<std::string> oprnd_;
    };

    ptr<buffer> pre_commit(const ulong log_idx, buffer& data) {
        // Nothing to do with pre-commit in this example.
        return nullptr;
    }

    ptr<buffer> commit(const ulong log_idx, buffer& data) {
        op_payload payload;
        buffer_serializer b_s(data);
        int32_t op;
        b_s.get_opstruct(op,payload.oprnd_);
        switch(op){
            case 0x0: payload.type_ = SET; break;
            case 0x1: payload.type_ = DELETE; break;
        }

        //DEBUG
        // std::cout << "Commit:" << std::endl;
        // std::cout << "operate:" << op << std::endl;
        // std::cout << "key or value:" << std::endl;
        // for(uint64_t i = 0; i < payload.oprnd_.size(); i++){
        //     std::cout<< payload.oprnd_[i] <<std::endl;
        // }

        int cnt = 0;
        result_="ERROR";
        std::string val = "";
        switch (payload.type_) {
        case SET:
            for(unsigned long i = 1; i < payload.oprnd_.size(); i++){
                val += payload.oprnd_[i];
                if(i!=payload.oprnd_.size()-1){
                    val += " ";
                }
            }
            if(kvstore.find(payload.oprnd_[0])==kvstore.end()){
                kvstore.insert(StrStrPair(payload.oprnd_[0],val));
            }else{
                kvstore[payload.oprnd_[0]]=val;
            }
            result_ = "OK";
            break;
        case DELETE:
            for(unsigned long i = 0; i < payload.oprnd_.size(); i++)
            {
                if(kvstore.find(payload.oprnd_[i])!=kvstore.end()){
                    kvstore.erase(payload.oprnd_[i]);
                    cnt++;
                }
            }
            result_ = std::to_string(cnt);
            break;
        }

        cur_value_ += 1;

        last_committed_idx_ = log_idx;

        // Return Raft log number as a return result.
        ptr<buffer> ret = buffer::alloc( sizeof(log_idx) );
        buffer_serializer bs(ret);
        bs.put_u64(log_idx);
        return ret;
    }

    void op_GET_result(std::string str){
        if(kvstore.find(str)==kvstore.end()){
            result_ = "nil";
        }else{
            result_ = kvstore[str];
        }
    }

    void commit_config(const ulong log_idx, ptr<cluster_config>& new_conf) {
        // Nothing to do with configuration change. Just update committed index.
        last_committed_idx_ = log_idx;
    }

    void rollback(const ulong log_idx, buffer& data) {
        // Nothing to do with rollback,
        // as this example doesn't do anything on pre-commit.
    }

    int read_logical_snp_obj(snapshot& s,
                             void*& user_snp_ctx,
                             ulong obj_id,
                             ptr<buffer>& data_out,
                             bool& is_last_obj)
    {
        ptr<snapshot_ctx> ctx = nullptr;
        {   std::lock_guard<std::mutex> ll(snapshots_lock_);
            auto entry = snapshots_.find(s.get_last_log_idx());
            if (entry == snapshots_.end()) {
                // Snapshot doesn't exist.
                data_out = nullptr;
                is_last_obj = true;
                return 0;
            }
            ctx = entry->second;
        }

        if (obj_id == 0) {
            // Object ID == 0: first object, put dummy data.
            data_out = buffer::alloc( sizeof(int32) );
            buffer_serializer bs(data_out);
            bs.put_i32(0);

            is_last_obj = false;
        } else {
            // Object ID > 0: second object, put actual value.
            data_out = buffer::alloc( sizeof(ulong) );
            buffer_serializer bs(data_out);
            bs.put_u64( ctx->value_ );
            is_last_obj = true;
        }
        return 0;
    }

    void save_logical_snp_obj(snapshot& s,
                              ulong& obj_id,
                              buffer& data,
                              bool is_first_obj,
                              bool is_last_obj)
    {
        if (obj_id == 0) {
            // Object ID == 0: it contains dummy value, create snapshot context.
            ptr<buffer> snp_buf = s.serialize();
            ptr<snapshot> ss = snapshot::deserialize(*snp_buf);
            create_snapshot_internal(ss);

        } else {
            // Object ID > 0: actual snapshot value.
            buffer_serializer bs(data);
            int64_t local_value = (int64_t)bs.get_u64();

            std::lock_guard<std::mutex> ll(snapshots_lock_);
            auto entry = snapshots_.find(s.get_last_log_idx());
            assert(entry != snapshots_.end());
            entry->second->value_ = local_value;
        }
        // Request next object.
        obj_id++;
    }

    bool apply_snapshot(snapshot& s) {
        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = snapshots_.find(s.get_last_log_idx());
        if (entry == snapshots_.end()) return false;

        ptr<snapshot_ctx> ctx = entry->second;
        cur_value_ = ctx->value_;
        //kvstore.insert(ctx->value_.begin(),ctx->value_.end());
        return true;
    }

    void free_user_snp_ctx(void*& user_snp_ctx) {
        // In this example, `read_logical_snp_obj` doesn't create
        // `user_snp_ctx`. Nothing to do in this function.
    }

    ptr<snapshot> last_snapshot() {
        // Just return the latest snapshot.
        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = snapshots_.rbegin();
        if (entry == snapshots_.rend()) return nullptr;
        
        ptr<snapshot_ctx> ctx = entry->second;
        return ctx->snapshot_;
    }

    ulong last_commit_index() {
        return last_committed_idx_;
    }

    void create_snapshot(snapshot& s,
                         async_result<bool>::handler_type& when_done)
    {
        if (!async_snapshot_) {
            // Create a snapshot in a synchronous way (blocking the thread).
            create_snapshot_sync(s, when_done);
        } else {
            // Create a snapshot in an asynchronous way (in a different thread).
            create_snapshot_async(s, when_done);
        }
    }

    std::string get_latest_result() const { return result_; }
    void get_latest_kvstore(){
        StrStrMap::iterator p;
        int i = 1;
        for(p = kvstore.begin();p!=kvstore.end();p++){
            std::string str = p->first;
            std::cout<<"["<<i<<"]"<<"key:"<<str.data()<<",value:"<<p->second<<std::endl;
            i++;
        }
    }

private:
    struct snapshot_ctx {
        snapshot_ctx( ptr<snapshot>& s, int64_t v )
            : snapshot_(s), value_(v) {}
        ptr<snapshot> snapshot_;
        int64_t value_;
    };

    void create_snapshot_internal(ptr<snapshot> ss) {
        std::lock_guard<std::mutex> ll(snapshots_lock_);

        // Put into snapshot map.
        ptr<snapshot_ctx> ctx = cs_new<snapshot_ctx>(ss, cur_value_);
        snapshots_[ss->get_last_log_idx()] = ctx;

        // Maintain last 3 snapshots only.
        const int MAX_SNAPSHOTS = 3;
        int num = snapshots_.size();
        auto entry = snapshots_.begin();
        for (int ii = 0; ii < num - MAX_SNAPSHOTS; ++ii) {
            if (entry == snapshots_.end()) break;
            entry = snapshots_.erase(entry);
        }
    }

    void create_snapshot_sync(snapshot& s,
                              async_result<bool>::handler_type& when_done)
    {
        // Clone snapshot from `s`.
        ptr<buffer> snp_buf = s.serialize();
        ptr<snapshot> ss = snapshot::deserialize(*snp_buf);
        create_snapshot_internal(ss);

        ptr<std::exception> except(nullptr);
        bool ret = true;
        when_done(ret, except);

        std::cout << "snapshot (" << ss->get_last_log_term() << ", "
                  << ss->get_last_log_idx() << ") has been created synchronously"
                  << std::endl;
    }

    void create_snapshot_async(snapshot& s,
                               async_result<bool>::handler_type& when_done)
    {
        // Clone snapshot from `s`.
        ptr<buffer> snp_buf = s.serialize();
        ptr<snapshot> ss = snapshot::deserialize(*snp_buf);

        // Note that this is a very naive and inefficient example
        // that creates a new thread for each snapshot creation.
        std::thread t_hdl([this, ss, when_done]{
            create_snapshot_internal(ss);

            ptr<std::exception> except(nullptr);
            bool ret = true;
            when_done(ret, except);

            std::cout << "snapshot (" << ss->get_last_log_term() << ", "
                      << ss->get_last_log_idx() << ") has been created asynchronously"
                      << std::endl;
        });
        t_hdl.detach();
    }

    std::atomic<int64_t> cur_value_;

    // State machine's latest op result
    std::string result_;

    // State machine's kvstore.
    //std::atomic<StrStrMap> kvstore;
    StrStrMap kvstore;

    // Last committed Raft log number.
    std::atomic<uint64_t> last_committed_idx_;

    // Keeps the last 3 snapshots, by their Raft log numbers.
    std::map< uint64_t, ptr<snapshot_ctx> > snapshots_;

    // Mutex for `snapshots_`.
    std::mutex snapshots_lock_;

    // If `true`, snapshot will be created asynchronously.
    bool async_snapshot_;
};
};