package raft

import (
	"fmt"
	"log"
	"net"
	"net/rpc"
	"time"
)

//注册rpc服务绑定http协议上开启监听
func rpcRegister(raft *Raft) {
	//注册一个RPC服务器
	rpcs := rpc.NewServer()
	if err := rpcs.Register(raft); err != nil {
		log.Panicln("注册RPC失败", err)
	}
	port := raft.node.Port
	ip := raft.node.IP
	//把RPC服务绑定到http协议上
	fmt.Println("rpc监听", raft.node.IP, port, "端口")
	l, e := net.Listen("tcp", ip+":"+port)
	if e != nil {
		log.Fatal("listen error:", e)
	}
	for {
		conn, err := l.Accept()
		if err == nil {
			go rpcs.ServeConn(conn)
		} else {
			break
		}
	}
	l.Close()
}

//广播，调用所有节点的method方法（不广播自己）
func (rf *Raft) broadcast(method string, args interface{}, fun func(Broadply *RequestPly)) {
	for nodeID, node := range nodePool {
		//不广播自己
		if nodeID == rf.me {
			continue
		}
		//连接远程节点的rpc
		ply := new(RequestPly)
		ply.Ok = false
		ply.Cnt = 0
		//fmt.Println("broadcast to port:", node.IP+":"+node.Port)
		conn, err := rpc.Dial("tcp", node.IP+":"+node.Port)
		if err != nil {
			//连接失败，调用回调
			ply.Ok = false
			fun(ply)
			continue
		}
		switch ret := args.(type) {
		case ElectArgs:
			fmt.Printf("Election Broad Term:%d\n", ret.Term)
		}
		err = conn.Call(method, args, &ply)
		if err != nil {
			//调用失败，调用回调
			ply.Ok = false
			fun(ply)
			continue
		}
		//回调
		fun(ply)
	}
}

// HeartBeatResponse 心跳检测回复
func (rf *Raft) HeartBeatResponse(node NodeInfo, b *RequestPly) error {
	//因为发送心跳的一定是leader，之所以写这一句的目的是如果有down的节点恢复了，直接是follower，所以直接告诉它leader是谁即可
	rf.setCurrentLeader(node.ID)
	//最后一次心跳的时间
	rf.lastSendHeartBeatTime = millisecond()
	//fmt.Printf("收到来自leader[%s]节点的心跳检测\n", node.ID)
	b.Ok = true
	return nil
}

// ConfirmationLeader 确认领导者
func (rf *Raft) ConfirmationLeader(node NodeInfo, b *RequestPly) error {
	rf.setCurrentLeader(node.ID)
	b.Ok = true
	fmt.Println("已发现网络中的领导节点，", node.ID, "成为了领导者！")
	rf.reDefault()
	return nil
}

// Vote 投票
func (rf *Raft) Vote(node ElectArgs, ply *RequestPly) error {
	fmt.Printf("Vote Candidate ID:%s\n", node.ID)
	fmt.Printf("Vote Candidate Term:%d\n", node.Term)
	if rf.currentTerm < node.Term && rf.currentLeader == "-1" {
		rf.setVoteFor(node.ID)
		rf.setTerm(node.Term)
		fmt.Printf("Term add from %d to %d\n", rf.currentTerm, node.Term)
		fmt.Printf("投票成功，已投%s节点\n", node.ID)
		ply.Ok = true
	} else if rf.currentTerm == node.Term && rf.votedFor == "-1" && rf.currentLeader == "-1" {
		rf.setVoteFor(node.ID)
		fmt.Printf("Vote at Term:%d\n", rf.currentTerm)
		fmt.Printf("投票成功，已投%s节点\n", node.ID)
		ply.Ok = true
	} else {
		fmt.Printf("False Vote Candidate Term:%d\n", node.Term)
		fmt.Printf("Alreadly Voted at Term:%d\n", rf.currentTerm)
		ply.Ok = false
	}
	return nil
}

// LeaderReceiveMessage 领导者接收到跟随者节点转发过来的消息
func (rf *Raft) LeaderReceiveMessage(reArgs RequestArgs, ply *RequestPly) error {
	fmt.Printf("领导者节点接收到转发过来的消息，id为:%d\n", reArgs.MsgID)
	fmt.Printf("OP:%s Sub:", reArgs.Con)
	for _, key := range reArgs.Sub {
		fmt.Printf("%s ", key)
	}
	fmt.Printf("\n")
	ply.Ok = false
	fmt.Println("准备将消息进行广播...")
	//广播给其他跟随者
	rec := 1
	waitCnt := 0
	switch reArgs.Con {
	case "GET":
		getFlag := 0
		Mu.Lock()
		if KvDataBase[reArgs.Sub[0]] != "" {
			ply.Val = KvDataBase[reArgs.Sub[0]]
			Mu.Unlock()
			ply.Ok = true
			return nil
		} else {
			Mu.Unlock()
			go rf.broadcast("Raft.ReceiveMessage", reArgs, func(Leaderply *RequestPly) {
				if Leaderply.Ok {
					if Leaderply.Val != "" && getFlag == 0 {
						ply.Val = Leaderply.Val
						ply.Ok = true
						getFlag = 1
					}
					rec++
				}
			})
			for {
				if waitCnt >= nodeCount*2 {
					ply.Ok = false
					return nil
				}
				if rec >= nodeCount/2+1 || getFlag == 1 {
					if getFlag == 0 {
						ply.Val = ""
					} else {
						Mu.Lock()
						KvDataBase[reArgs.Sub[0]] = ply.Val
						Mu.Unlock()
					}
					ply.Ok = true
					fmt.Printf("%d个节点接收到消息id:%d\n", rec, reArgs.MsgID)
					//fmt.Printf("raft验证通过,可以打印消息,id为:[%d],消息为:[%s]\n", reArgs.MsgID, reArgs.Con+" "+reArgs.Sub[0])
					rf.lastSendMessageTime = millisecond()
					//fmt.Println("准备将消息提交信息发送至客户端...")
					ShowDatabase()
					return nil
				} else {
					//可能别的节点还没回复，等待一会
					time.Sleep(time.Millisecond * 500)
					waitCnt++
				}
			}
			return nil
		}
	case "DEL":
		delCnt := 0
		go rf.broadcast("Raft.ReceiveMessage", reArgs, func(Leaderply *RequestPly) {
			if Leaderply.Ok {
				if Leaderply.Cnt > delCnt {
					delCnt = Leaderply.Cnt
				}
				rec++
			}
		})
		for {
			if waitCnt >= nodeCount*2 {
				ply.Ok = false
				return nil
			}
			if rec >= nodeCount/2+1 {
				Mu.Lock()
				for _, key := range reArgs.Sub {
					if KvDataBase[key] != "" {
						delete(KvDataBase, key)
					}
				}
				Mu.Unlock()
				ply.Ok = true
				ply.Cnt = delCnt
				fmt.Printf("%d个节点接收到消息id:%d\n", rec, reArgs.MsgID)
				//fmt.Printf("raft验证通过,可以打印消息,id为:[%d],消息为:[%s]\n", reArgs.MsgID, reArgs.Con+" "+reArgs.Sub[0])
				rf.lastSendMessageTime = millisecond()
				//fmt.Println("准备将消息提交信息发送至客户端...")
				ShowDatabase()
				return nil
			} else {
				//可能别的节点还没回复，等待一会
				time.Sleep(time.Millisecond * 500)
				waitCnt++
			}
		}
		return nil
	case "SET":
		go rf.broadcast("Raft.ReceiveMessage", reArgs, func(Leaderply *RequestPly) {
			if Leaderply.Ok {
				rec++
			}
		})
		for {
			if waitCnt >= nodeCount*2 {
				ply.Ok = false
				return nil
			}
			if rec >= nodeCount/2+1 {
				ply.Ok = true
				Mu.Lock()
				KvDataBase[reArgs.Sub[0]] = reArgs.Sub[1]
				Mu.Unlock()
				fmt.Printf("%d个节点接收到消息id:%d\n", rec, reArgs.MsgID)
				//fmt.Printf("raft验证通过,可以打印消息,id为:[%d],消息为:[%s]\n", reArgs.MsgID, reArgs.Con+" "+reArgs.Sub[0])
				rf.lastSendMessageTime = millisecond()
				//fmt.Println("准备将消息提交信息发送至客户端...")
				ShowDatabase()
				return nil
			} else {
				//可能别的节点还没回复，等待一会
				time.Sleep(time.Millisecond * 500)
				waitCnt++
			}
		}
	default:
		ply.Ok = false
		return nil
	}
	return nil
}

// ReceiveMessage 跟随者节点用来接收消息，然后存储到消息池中，待领导者确认后打印
func (rf *Raft) ReceiveMessage(reArgs RequestArgs, ply *RequestPly) error {
	fmt.Printf("接收到领导者节点发来的信息，id:%d\n", reArgs.MsgID)
	fmt.Printf("OP:%s Sub:", reArgs.Con)
	for _, key := range reArgs.Sub {
		fmt.Printf("%s ", key)
	}
	fmt.Printf("\n")
	ply.Ok = false
	ply.Cnt = 0
	switch reArgs.Con {
	case "GET":
		Mu.Lock()
		if KvDataBase[reArgs.Sub[0]] != "" {
			Mu.Unlock()
			ply.Ok = true
			ply.Val = KvDataBase[reArgs.Sub[0]]
			ShowDatabase()
			return nil
		} else {
			Mu.Unlock()
			ply.Ok = true
			ply.Val = ""
			ShowDatabase()
			return nil
		}
	case "SET":
		Mu.Lock()
		KvDataBase[reArgs.Sub[0]] = reArgs.Sub[1]
		Mu.Unlock()
		ply.Ok = true
		ShowDatabase()
		return nil
	case "DEL":
		Mu.Lock()
		for _, key := range reArgs.Sub {
			if KvDataBase[key] != "" {
				delete(KvDataBase, key)
				ply.Cnt++
			}
		}
		Mu.Unlock()
		ply.Ok = true
		ShowDatabase()
		return nil
	}
	fmt.Println("已回复接收到消息，待领导者确认后打印")
	return nil
}
