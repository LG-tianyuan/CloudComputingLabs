package raft

import (
	"bytes"
	"fmt"
	"math/rand"
	"net"
	"net/rpc"
	"strconv"
	"strings"
)

type RequestArgs struct {
	MsgID int
	Con   string
	SorC  int
	SID   string
	CID   string
	Ok    bool
	Cnt   int
}
type RequestPly struct {
	Cnt    int
	Ok     bool
	Val    string
	AllCou []string
}

func (rf *Raft) httpListen() {
	rfCopy := rf
	rpc.Register(rfCopy)
	//http.HandleFunc("/req", rf.getRequest)
	port, _ := strconv.Atoi(rf.node.Port)
	port += 2000
	listener, err := net.Listen("tcp", rf.node.IP+":"+strconv.Itoa(port))
	// 连接失败处理
	if err != nil {
		fmt.Println("启动服务失败,err:", err)
		return
	}
	// 程序退出时释放端口
	defer listener.Close()

	for {
		// 2. 建立连接
		// conn 是返回的接口
		conn, err := listener.Accept()
		if err != nil {
			//fmt.Println("接收客户端连接失败,err", err)
			continue // 继续阻塞，等待客户端再次连接
		}
		// 3. 启动一个 goroutine 处理客户端连接
		go rf.getRequest(conn)
	}
}
func (rf *Raft) getRequest(writer net.Conn) {
	defer writer.Close()
	var buf [1024]byte
	n, err := writer.Read(buf[:]) // 读取切片数据
	if err != nil {
		//fmt.Println("read err:", err)
		return
	}
	line := string(buf[:n]) // 转换字符串
	//fmt.Println("收到", writer.RemoteAddr(), "发来的数据:", line)
	if len(line) > 0 && rf.currentLeader != "-1" {
		args := strings.Split(line, " ")
		reArgs := new(RequestArgs)
		reArgs.MsgID = getRandom()
		ply := new(RequestPly)
		ply.Ok = false
		if args[0] == "GET" {
			reArgs.Con = "GET"
			if args[1] == "Student" {
				reArgs.SorC = 0
				reArgs.SID = args[2]
			} else {
				reArgs.SorC = 1
				reArgs.CID = args[2]
			}
			node := nodePool[rf.currentLeader]
			conn, err := rpc.Dial("tcp", node.IP+":"+node.Port)
			if err != nil {
				writer.Write([]byte("403\nGET dial ERROR"))
				return
			}
			err = conn.Call("Raft.LeaderGet", reArgs, &ply)
			if err != nil {
				writer.Write([]byte("403\nGET call ERROR"))
				return
			}
			if ply.Ok {
				if reArgs.SorC == 0 {
					var bufs bytes.Buffer
					bufs.WriteString("200\n" + reArgs.SID + " " + KvStu[reArgs.SID].Sname + "\n")
					var tmp Stu
					tmp.Sid = reArgs.SID
					tmp.Sname = KvStu[reArgs.SID].Sname
					for _, scnode := range ply.AllCou {
						tmp.Sc = append(tmp.Sc, scnode)
						bufs.WriteString(scnode + "\n")
					}
					KvStu[reArgs.SID] = tmp
					bufs.WriteTo(writer)
					return
				} else if reArgs.CID == "all" {
					var cnt bytes.Buffer
					var buff bytes.Buffer
					cnt.WriteString(" {\"status\":\"ok\",\"data\":[")
					for _, scnode := range ply.AllCou {
						scSplit := strings.Split(scnode, " ")
						cnt.WriteString("{\"id\":" + scSplit[0] + ",\"name\":" + scSplit[1] + ",\"capacity\":" + scSplit[2])
						cnt.WriteString(",\"selected\":" + scSplit[3] + "},")
					}
					cnt.WriteString("]}")
					//fmt.Printf("%d\n", cnt.Len())
					buff.WriteString("HTTP/1.1 200 OK\r\nContent-Type:application/json\r\n")
					buff.WriteString("Content-Length:" + strconv.Itoa(cnt.Len()) + "\r\n\r\n")
					buff.WriteTo(writer)
					cnt.WriteTo(writer)
					return
				} else {
					writer.Write([]byte("200\n" + ply.Val + "\n"))
					lineSplit := strings.Split(ply.Val, " ")
					var temp Cou
					temp.Cid = lineSplit[0]
					temp.Cname = lineSplit[1]
					temp.Cap, _ = strconv.Atoi(lineSplit[2])
					temp.Seled, _ = strconv.Atoi(lineSplit[3])
					KvCou[lineSplit[0]] = temp
					return
				}
			} else {
				writer.Write([]byte("403\nGET Reply ERROR\n"))
				return
			}
		} else if args[0] == "ADD" || args[0] == "DEL" {
			reArgs.Con = args[0]
			reArgs.SID = args[3]
			reArgs.CID = args[4]
			//fmt.Println("Con:", args[0], "SID:", args[3], "CID:", args[4])
			node := nodePool[rf.currentLeader]
			conn, err := rpc.Dial("tcp", node.IP+":"+node.Port)
			if err != nil {
				writer.Write([]byte("403\n"))
				writer.Write([]byte(args[0] + " Dial ERROR\n"))
				return
			}
			err = conn.Call("Raft.LeaderReceiveMessage", reArgs, &ply)
			if err != nil {
				writer.Write([]byte("403\n"))
				writer.Write([]byte(args[0] + " Call ERROR\n"))
				return
			}
			if ply.Ok {
				writer.Write([]byte("200\n"))
				return
			} else {
				writer.Write([]byte("403\n" + args[0] + " Reply ERROR: " + ply.Val))
				return
			}
		}
	}
}

//返回一个十位数的随机数，作为msg.id
func getRandom() int {
	id := rand.Intn(1000000000) + 1000000000
	return id
}
