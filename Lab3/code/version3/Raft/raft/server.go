package raft

import (
	"fmt"
	"log"
	"net/rpc"
	"os"
	"sync"
)

//定义节点数量
var nodeCount = 4

//节点池
var nodePool map[string]NodeInfo

//选举超时时间（秒）
var electionTimeout = 4

//心跳检测超时时间
var heartBeatTimeout = 2

//心跳检测频率（秒）
var heartBeatRate = 1

var KvDataBase = make(map[string]string)
var Mu sync.Mutex

func ShowDatabase() {
	fmt.Println("*******************************")
	for key, val := range KvDataBase {
		fmt.Printf("Key:%s\tVal:%s\n", key, val)
	}
	fmt.Println("*******************************")
}
func Showply(ply *RequestPly) {
	fmt.Println("``````````````````````")
	fmt.Println("ply.Ok:", ply.Ok)
	fmt.Println("ply.Cnt:", ply.Cnt)
	fmt.Println("ply.Val", ply.Val)
	fmt.Println("``````````````````````")
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
	for nodeID := range nodePool {
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
