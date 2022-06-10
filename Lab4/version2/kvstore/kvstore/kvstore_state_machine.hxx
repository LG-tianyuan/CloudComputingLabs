/*
 *@Author:LG-tianyuan
 *@Date:2022-5-19
 *@Last-Update:2022-6-04
*/

#pragma once

#include "nuraft.hxx"
#include <stdlib.h>
#include <stdio.h>
#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <map>
#include <vector>
#include <string.h>
#include <sys/socket.h>

using namespace nuraft;

typedef struct {
 std::string id;
 std::string name;
 int capacity;
 int selected;
}Course;

typedef struct {
    std::string id;
    std::string name;
    std::vector<std::string> sc;
}Student;


typedef std::map<std::string,Course> CourseMap;
typedef std::pair<std::string,Course> CoursePair;
typedef std::map<std::string,Student> StudentMap;
typedef std::pair<std::string,Student> StudentPair;

namespace kvstoresystem {

class kvstore_state_machine : public state_machine {
public:
    //构造函数
    kvstore_state_machine(bool async_snapshot = false)
        : cur_value_(1)
        , result_("ERROR")
        , last_committed_idx_(0)
        , async_snapshot_(async_snapshot)
        {
            course = new std::map<std::string,Course>();
            student = new std::map<std::string,Student>();
        }

    //析构函数
    ~kvstore_state_machine() {}

    enum op_type : int32_t {
        ADD = 0x0,
        DEL = 0x1
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
            case 0x0: payload.type_ = ADD; break;
            case 0x1: payload.type_ = DEL; break;
        }

        //DEBUG
        // std::cout << "Commit:" << std::endl;
        // std::cout << "operate:" << op << std::endl;
        // std::cout << "key or value:" << std::endl;
        // for(uint64_t i = 0; i < payload.oprnd_.size(); i++){
        //     std::cout<< payload.oprnd_[i] <<std::endl;
        // }

        // int cnt = 0;
        result_="-ERROR";
        std::string val = "";
        std::map<std::string,Course>::iterator it2;
        std::map<std::string,Student>::iterator it1;
        switch (payload.type_) {
        case ADD:
            it1 = student->find(payload.oprnd_[0]);
            it2 = course->find(payload.oprnd_[1]);
            if(it1==student->end()){
                result_ = "The student does not exist!";
            }else if(it2==course->end()){
                result_ = "The course does not exist!";
            }else{
                Student temp1 = it1->second;
                Course temp2 = it2->second;
                if(temp2.capacity > temp2.selected){//课程人数未满
                    bool flag = true;
                    for(unsigned int i = 0; i < temp1.sc.size(); i++){//判断是否已经选过该课程
                        if(temp1.sc[i] == payload.oprnd_[1]){
                            flag = false;
                            break;
                        }
                    }
                    if(flag){
                        temp1.sc.push_back(payload.oprnd_[1]);
                        it1->second = temp1;
                        temp2.selected += 1;
                        it2->second = temp2;//更新选课人数
                        result_ = "+OK";
                    }else{
                        result_ = "You have already taken this course and can not choose it again.";
                    }
                }else{//课程已满
                    result_ = "Course is full.";
                }
            }
            break;
        case DEL:
            it1 = student->find(payload.oprnd_[0]);
            it2 = course->find(payload.oprnd_[1]);
            if(it1==student->end()){
                result_ = "The student does not exist!";
            }else if(it2==course->end()){
                result_ = "The course does not exist!";
            }else{
                Student temp1 = it1->second;
                Course temp2 = it2->second;
                if(temp2.capacity > temp2.selected){//课程人数未满
                    bool flag = false;
                    for(unsigned int i = 0; i < temp1.sc.size(); i++){//判断是否选过该课程
                        if(temp1.sc[i] == payload.oprnd_[1]){
                            temp1.sc.erase(temp1.sc.begin()+i); //退课
                            flag = true;
                            break;
                        }
                    }
                    if(flag){
                        it1->second = temp1;
                        temp2.selected -= 1;
                        it2->second = temp2;//更新选课人数
                        result_ = "+OK";
                    }else{
                        result_ = "You haven't token this course.";
                    }
                }else{//课程已满
                    result_ = "Course is full.";
                }
            }
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

    Student get_student(std::string str){
        std::map<std::string,Student>::iterator it;
        it = student->find(str);
        Student temp;
        if(it==student->end()){
            result_ = "The student does not exist!";
        }else{
            temp = it->second;
        }
        return temp;
    }

    Course get_course(std::string str){
        std::map<std::string,Course>::iterator it;
        it = course->find(str);
        Course temp;
        if(it==course->end()){
            result_ = "The course does not exist!";
        }else{
            temp = it->second;
        }
        return temp;
    }

    void send_response_head(int cfd, int len)
    {
      char buf[1024] = {'\0'};
      sprintf(buf, "HTTP/1.1 200 OK\r\n");
      sprintf(buf+strlen(buf), "Content-Type:application/json\r\n");
      sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len); 
      send(cfd, buf, strlen(buf), 0);
      send(cfd, "\r\n", 2, 0);
    }

    void send_course_all(int cfd){
        char* buff = (char *)malloc(25600*sizeof(char));
        char* buff2 = (char *)malloc(25600*sizeof(char));
        int len = 0, i = 0;
        if(course->empty()){
            sprintf(buff,"{\"status\":\"error\", \"message\":\"No course data!\"}\n");
        }else{
            len = course->size();
            sprintf(buff,"{\"status\":\"ok\",\"data\":\"[");
            std::map<std::string,Course>::iterator it;
            for(it = course->begin(); it != course->end(); it++,i++){
                Course temp = it->second;
                sprintf(buff+strlen(buff),"{\"id\":%s,\"name\":%s,\"capacity\":%d,\"selected\":%d}",temp.id.c_str(),temp.name.c_str(),temp.capacity,temp.selected);
                if(i != len-1){
                    sprintf(buff+strlen(buff),",");
                }
                int length = strlen(buff);
                if(length>25000){
                    break;
                }
                // sprintf(buff+strlen(buff),"%s %s %d %d\n",temp.id.c_str(),temp.name.c_str(),temp.capacity,temp.selected);
            }
            for(; it != course->end(); it++,i++){
                Course temp = it->second;
                sprintf(buff2+strlen(buff2),"{\"id\":%s,\"name\":%s,\"capacity\":%d,\"selected\":%d}",temp.id.c_str(),temp.name.c_str(),temp.capacity,temp.selected);
                if(i != len-1){
                    sprintf(buff2+strlen(buff2),",");
                }
            }
            len = strlen(buff2);
            if(len == 0){
                sprintf(buff+strlen(buff),"]}\r\n");
            }else{
                sprintf(buff2+strlen(buff2),"]}\r\n");
            }
        }
        int total = strlen(buff)+strlen(buff2);
        send_response_head(cfd,total);
        send(cfd,buff,strlen(buff),0); 
        if(len!=0){
            send(cfd,buff2,strlen(buff2),0);  
        }
        free(buff);
        free(buff2);
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
    CourseMap* get_coursemap(){
        return course;
    }
    StudentMap* get_studentmap(){
        return student;
    }
    void init_kvstore(char* course_file, char* student_file){
        FILE *fp = fopen(course_file,"r");
        if(!fp){
            printf("can not open file\n");
            exit(0);
        }
        char line[1000]={'\0'};
        char id[50]={'\0'},name[100]={'\0'},capacity[20]={'\0'},selected[20]={'\0'};
        int cnt = 0;
        while(!feof(fp)){
            fgets(line,1000,fp);
            char* ptr = strchr(line,' ');
            strncpy(id,line,ptr-line);
            id[ptr-line]='\0';
            char* ptr2 = strchr(ptr+1,' ');
            strncpy(name,ptr+1,ptr2-ptr-1);
            name[ptr2-ptr-1]='\0';
            ptr = strchr(ptr2+1,' ');
            strncpy(capacity,ptr2+1,ptr-ptr2-1);
            capacity[ptr-ptr2-1]='\0';
            strcpy(selected,ptr+1);
            int len = strlen(selected);
            if(selected[len-1] == '\n'){
                selected[len-1] = '\0';
            }
            Course temp;
            temp.id = std::string(id);
            temp.name = std::string(name);
            temp.capacity = atoi(capacity);
            temp.selected = atoi(selected);
            course->insert(CoursePair(temp.id,temp));
            cnt++;
        }
        printf("1.course total number:%d\n",cnt-1);
        cnt=0;
        fclose(fp);
        fp = fopen(student_file,"r");
        if(!fp){
            printf("can not open file\n");
            exit(0);
        }
        while(!feof(fp)){
            fgets(line,1000,fp);
            char* ptr = strchr(line,' ');
            strncpy(id,line,ptr-line);
            strcpy(name,ptr+1);
            int len = strlen(name);
            if(name[len-1] == '\n'){
                name[len-1] = '\0';
            } 
            Student temp;
            temp.id = std::string(id);
            temp.name = std::string(name);
            student->insert(StudentPair(temp.id,temp));
            cnt++;
        }
        printf("2.student total number:%d\n",cnt-1);
        fclose(fp);
        printf("init kvstore successfully!\n");
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
    StudentMap *student;
    CourseMap *course;

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