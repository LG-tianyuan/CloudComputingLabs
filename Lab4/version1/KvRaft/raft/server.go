package raft

import (
	"bufio"
	"fmt"
	"io"
	"log"
	"net/rpc"
	"os"
	"strings"
	"sync"
)

type Cou struct {
	Cid   string
	Cname string
	Cap   int
	Seled int
}
type Stu struct {
	Sid   string
	Sname string
	Sc    []string
}

//定义节点数量
var nodeCount = 4

//节点池
var nodePool map[string]NodeInfo

//选举超时时间（秒）
var electionTimeout = 4

//心跳检测超时时间
var heartBeatTimeout = 3

//心跳检测频率（秒）
var heartBeatRate = 1

var KvStu = make(map[string]Stu)

var KvCou = make(map[string]Cou)

var Mu sync.Mutex

func InitKv(st string, cou string) {
	stfi, err := os.Open(st)
	if err != nil {
		panic(err)
	}
	// 创建 Reader
	r := bufio.NewReader(stfi)
	go func() {
		//stunum := 0
		for {
			line, err := r.ReadString('\n')
			if err != nil && err != io.EOF {
				panic(err)
			}
			if err == io.EOF {
				break
			}
			line = strings.TrimSpace(line)
			if line == "" {
				break
			}
			lineSplit := strings.Split(line, " ")
			var temp Stu
			temp.Sid = lineSplit[0]
			temp.Sname = lineSplit[1]
			KvStu[lineSplit[0]] = temp
			//stunum++
		}
		///println(stunum)
	}()
	Coufi, err := os.Open(cou)
	if err != nil {
		panic(err)
	}
	c := bufio.NewReader(Coufi)
	go func() {
		//cnt := 0
		for {
			line, err := c.ReadString('\n')
			line = strings.TrimSpace(line)
			//println(line == "")
			if line == "" {
				break
			}
			lineSplit := strings.Split(line, " ")
			//fmt.Println("lineSplit:", lineSplit[1])
			if err != nil && err != io.EOF {
				panic(err)
			}
			if err == io.EOF {
				break
			}
			var temp Cou
			temp.Cid = lineSplit[0]
			temp.Cname = lineSplit[1]
			temp.Cap = 999
			temp.Seled = 0
			KvCou[lineSplit[0]] = temp
			//cnt++
		}
		//fmt.Println(KvCou["GE12006"])
		//fmt.Println("course num:", cnt)
	}()
}

func Start(nodeNum int, ID string, nodeTable map[string]NodeInfo) {
	nodeCount = nodeNum
	nodePool = nodeTable

	if len(os.Args) < 1 {
		log.Panicln("缺少程序运行的参数")
	}

	//即 A、B、C其中一个
	id := ID

	//传入节点编号，端口号，创建raft实例
	ip := nodePool[id].IP
	port := nodePool[id].Port

	raft := NewRaft(id, ip, port)
	//注册rpc服务绑定http协议上开启监听
	go rpcRegister(raft)
	nodeState := make(map[string]int)
	for nodeID, _ := range nodePool {
		nodeState[nodeID] = 0
	}
	for {
		if raft.serverOn >= nodeCount/2+1 {
			fmt.Printf("%d servers On\n", raft.serverOn)
			break
		}
		for nodeID, node := range nodePool {
			//不广播自己
			if nodeID == raft.me || nodeState[nodeID] == 1 {
				continue
			}
			_, err := rpc.Dial("tcp", node.IP+":"+node.Port)
			if err == nil && nodeState[nodeID] == 0 {

				fmt.Printf("node off %d, server %s on\n", nodeState[nodeID], node.IP+":"+node.Port)
				nodeState[nodeID] = 1
				raft.serverOn++
			}
		}
	}
	//发送心跳包
	go raft.sendHeartPacket()

	//开启一个Http监听client发来的信息
	go raft.httpListen()

	//尝试成为候选人并选举
	go raft.tryToBeCandidateWithElection()

	//进行心跳超时检测
	go raft.heartTimeoutDetection()

	//TODO
	select {}
}
