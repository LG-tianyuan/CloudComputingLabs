package raft

import (
	"fmt"
	"log"
	"net"
	"net/rpc"
	"strconv"
	"strings"
	"time"
)

type BroadArgs struct {
	SID string
	CID string
	SC  []string
	SEL int
}

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
		ply.Ok = false
	}
	return nil
}
func (rf *Raft) LeaderGet(reArgs RequestArgs, ply *RequestPly) error {
	if reArgs.SorC == 0 {
		if stmp, bol := KvStu[reArgs.SID]; bol {
			for _, scnode := range stmp.Sc {
				ply.AllCou = append(ply.AllCou, scnode)
			}
			//fmt.Println("GET Stu ply:", ply.AllCou)
			ply.Ok = true
			return nil
		}
	} else if reArgs.CID == "all" {
		for _, Cnode := range KvCou {
			var tmp string
			tmp = Cnode.Cid + " " + Cnode.Cname + " " + strconv.Itoa(Cnode.Cap) + " " + strconv.Itoa(Cnode.Seled)
			ply.AllCou = append(ply.AllCou, tmp)
		}
		ply.Ok = true
		return nil
	} else if ctmp, bol := KvCou[reArgs.CID]; bol {
		ply.Val = reArgs.CID + " " + ctmp.Cname + " " +
			strconv.Itoa(ctmp.Cap) + " " + strconv.Itoa(ctmp.Seled)
		//fmt.Println("GET Cou ply:", ply.Val)
		ply.Ok = true
		return nil
	}
	return nil
}

// LeaderReceiveMessage 领导者接收到跟随者节点转发过来的消息
func (rf *Raft) LeaderReceiveMessage(reArgs RequestArgs, ply *RequestPly) error {
	//fmt.Printf("领导者节点接收到转发过来的消息，id为:%d\n", reArgs.MsgID)
	//fmt.Printf("OP:%s Sub:", reArgs.Con)
	//fmt.Printf("\n")
	ply.Ok = false
	//fmt.Println("准备将消息进行广播...")
	//广播给其他跟随者
	rec := 1
	waitCnt := 0
	bArgs := new(BroadArgs)
	switch reArgs.Con {
	case "DEL":
		ply.Ok = false
		if ctmp, cbol := KvCou[reArgs.CID]; cbol {
			if stmp, sbol := KvStu[reArgs.SID]; sbol {
				selcnt := 0
				for _, scnode := range stmp.Sc {
					lineSplit := strings.Split(scnode, " ")
					if lineSplit[0] == reArgs.CID {
						selcnt++
						continue
					} else {
						bArgs.SC = append(bArgs.SC, scnode)
					}
				}
				bArgs.SEL = ctmp.Seled - selcnt
				bArgs.CID = reArgs.CID
				bArgs.SID = reArgs.SID
				go rf.broadcast("Raft.ReceiveMessage", bArgs, func(Leaderply *RequestPly) {
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
						var tmp Stu
						tmp.Sid = reArgs.SID
						tmp.Sname = stmp.Sname
						for _, scnode := range bArgs.SC {
							tmp.Sc = append(tmp.Sc, scnode)
						}
						Mu.Lock()
						KvStu[reArgs.SID] = tmp
						var temp Cou
						temp = ctmp
						temp.Seled = bArgs.SEL
						KvCou[reArgs.CID] = temp
						Mu.Unlock()
						ply.Ok = true
						//fmt.Printf("%d个节点接收到消息id:%d\n", rec, reArgs.MsgID)
						//fmt.Printf("raft验证通过,可以打印消息,id为:[%d],消息为:[%s]\n", reArgs.MsgID, reArgs.Con+" "+reArgs.Sub[0])
						//fmt.Println("Stu:", tmp)
						//fmt.Println("Cou:", temp)
						rf.lastSendMessageTime = millisecond()
						//fmt.Println("准备将消息提交信息发送至客户端...")
						return nil
					} else {
						//可能别的节点还没回复，等待一会
						time.Sleep(time.Millisecond * 500)
						waitCnt++
					}
				}
			}
			return nil
		}
		return nil
	case "ADD":
		ply.Ok = false
		if ctmp, cbol := KvCou[reArgs.CID]; cbol {
			if stmp, sbol := KvStu[reArgs.SID]; sbol {
				selcnt := 1
				for _, scnode := range stmp.Sc {
					lineSplit := strings.Split(scnode, " ")
					if lineSplit[0] == reArgs.CID {
						selcnt = 0
						break
					} else {
						bArgs.SC = append(bArgs.SC, scnode)
					}
				}
				if selcnt == 1 && ctmp.Seled+1 <= ctmp.Cap {
					bArgs.CID = reArgs.CID
					bArgs.SID = reArgs.SID
					bArgs.SEL = ctmp.Seled + 1
					bArgs.SC = append(bArgs.SC, reArgs.CID+" "+ctmp.Cname)
					go rf.broadcast("Raft.ReceiveMessage", bArgs, func(Leaderply *RequestPly) {
						if Leaderply.Ok {
							rec++
						}
					})
					for {
						if waitCnt >= nodeCount*2 {
							ply.Ok = false
							ply.Val = "Node reply only:" + strconv.Itoa(rec)
							return nil
						}
						if rec >= nodeCount/2+1 {
							Mu.Lock()
							var tmp Stu
							tmp.Sid = reArgs.SID
							tmp.Sname = stmp.Sname
							for _, scnode := range bArgs.SC {
								tmp.Sc = append(tmp.Sc, scnode)
							}
							KvStu[reArgs.SID] = tmp
							var temp Cou
							temp = ctmp
							temp.Seled = bArgs.SEL
							KvCou[reArgs.CID] = temp
							Mu.Unlock()
							ply.Ok = true
							//fmt.Printf("%d个节点接收到消息id:%d\n", rec, reArgs.MsgID)
							//fmt.Printf("raft验证通过,可以打印消息,id为:[%d],消息为:[%s]\n", reArgs.MsgID, reArgs.Con+" "+reArgs.Sub[0])
							//fmt.Println("Stu:", tmp)
							//fmt.Println("Cou:", temp)
							rf.lastSendMessageTime = millisecond()
							ply.Cnt = rec
							//fmt.Println("准备将消息提交信息发送至客户端...")
							return nil
						} else {
							//可能别的节点还没回复，等待一会
							time.Sleep(time.Millisecond * 500)
							waitCnt++
						}
					}
				} else if selcnt == 0 {
					ply.Ok = true
					//ply.Val = "Repeat Course Selection!!"
					return nil
				} else if ctmp.Seled+1 <= ctmp.Cap {
					ply.Ok = true
					//ply.Val = "Course Full!!"
					return nil
				}
			} else {
				ply.Ok = false
				ply.Val = "No Student:" + reArgs.SID
				return nil
			}
		} else {
			ply.Ok = false
			ply.Val = "No Course:" + reArgs.CID
			return nil
		}
		return nil
	default:
		ply.Ok = false
		return nil
	}
}

// ReceiveMessage 跟随者节点用来接收消息，然后存储到消息池中，待领导者确认后打印
func (rf *Raft) ReceiveMessage(reArgs BroadArgs, ply *RequestPly) error {
	//fmt.Printf("接收到领导者节点发来的信息，sid:%s cid:%s\n", reArgs.SID, reArgs.CID)
	//fmt.Printf("\n")
	ply.Ok = false
	ply.Cnt = 0
	Mu.Lock()
	var tmp Stu
	tmp.Sid = reArgs.SID
	tmp.Sname = KvStu[reArgs.SID].Sname
	for _, scnode := range reArgs.SC {
		tmp.Sc = append(tmp.Sc, scnode)
	}
	//fmt.Println("Stu:", tmp.Sc)
	KvStu[reArgs.SID] = tmp
	var temp Cou
	temp = KvCou[reArgs.CID]
	temp.Seled = reArgs.SEL
	KvCou[reArgs.CID] = temp
	//fmt.Println("Cou:", temp)
	Mu.Unlock()
	ply.Ok = true
	//fmt.Println("已回复接收到消息，待领导者确认后打印")
	return nil
}
