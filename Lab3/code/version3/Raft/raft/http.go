package raft

import (
	"bufio"
	"bytes"
	"fmt"
	"log"
	"math/rand"
	"net"
	"net/rpc"
	"os"
	"strconv"
	"strings"
)

type RequestArgs struct {
	MsgID int
	Con   string
	Sub   []string
	Ok    bool
	Cnt   int
}
type RequestPly struct {
	Cnt int
	Ok  bool
	Val string
}

func (rf *Raft) httpListen() {
	//创建getRequest()回调方法
	rfCopy := rf
	rpc.Register(rfCopy)
	//http.HandleFunc("/req", rf.getRequest)
	port, _ := strconv.Atoi(rf.node.Port)
	port += 2000
	fmt.Println("http监听", rf.node.IP, strconv.Itoa(port), "端口")
	tcpAddr, err := net.ResolveTCPAddr("tcp", rf.node.IP+":"+strconv.Itoa(port))
	if err != nil {
		fmt.Println("Fatal error:", err)
		os.Exit(1)
	}
	l, e := net.ListenTCP("tcp", tcpAddr)
	if e != nil {
		log.Fatal("listen error:", e)
	}
	for {
		conn, err := l.Accept()
		if err != nil {
			continue
		}
		fmt.Println(conn.RemoteAddr().String())
		go rpc.ServeConn(conn)
	}
	l.Close()
}

func (rf *Raft) DealRequest(args string, reply *string) error {
	reArgs := new(RequestArgs)
	reArgs.MsgID = getRandom()
	ply := new(RequestPly)
	ply.Ok = false
	reader := bufio.NewReader(strings.NewReader(args))
	result, _ := Decode(reader)
	reTemp := result.([]interface{})
	var bt bytes.Buffer
	Operate := reTemp[0].(string)
	length := len(reTemp)
	fmt.Println(length)
	switch Operate {
	case "DEL":
		if length < 2 {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
		reArgs.Con = "DEL"
		reArgs.Sub = make([]string, length-1)
		for i := 1; i < length; i++ {
			if reTemp[i] == "" {
				continue
			}
			reArgs.Sub[i-1] = reTemp[i].(string)
		}
		node := nodePool[rf.currentLeader]
		conn, err := rpc.Dial("tcp", node.IP+":"+node.Port)
		if err != nil {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
		err = conn.Call("Raft.LeaderReceiveMessage", reArgs, &ply)
		Showply(ply)
		if err != nil {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
		if ply.Ok {
			*reply = string(EncodeInteger(int64(ply.Cnt)))
			//*reply = strconv.Itoa(ply.Cnt)
			return nil
		} else {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
	case "SET":
		if length < 3 {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
		reArgs.Con = "SET"
		reArgs.Sub = make([]string, 2)
		for i := 1; i < length; i++ {
			if reTemp[i] == "" {
				continue
			} else if i == 1 {
				reArgs.Sub[i-1] = reTemp[i].(string)
			} else {
				bt.WriteString(reTemp[i].(string) + " ")
			}
		}
		reArgs.Sub[1] = bt.String()
		node := nodePool[rf.currentLeader]
		conn, err := rpc.Dial("tcp", node.IP+":"+node.Port)
		if err != nil {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
		err = conn.Call("Raft.LeaderReceiveMessage", reArgs, &ply)
		Showply(ply)
		if err != nil {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
		fmt.Printf("OP:%s \n", Operate)
		fmt.Println(len(reArgs.Sub))
		fmt.Println(reArgs.Sub)
		if ply.Ok {
			*reply = string(EncodeSuccessMsg("Ok"))
			return nil
		} else {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
	case "GET":
		var Getkey string
		var Getvalue string
		if length == 2 {
			Getkey = reTemp[1].(string)
			fmt.Printf("OP:%s key:%s", Operate, Getkey)
			Getvalue = KvDataBase[Getkey]
			if Getvalue != "" {
				st := Getvalue
				st = strings.Replace(st, "\n", "", -1)
				st = strings.Replace(st, "\r", "", -1)
				st = strings.Replace(st, "\t", "", -1)
				stArr := strings.Split(st, " ")
				buf := make([][]byte, len(stArr))
				for i, v := range stArr {
					//fmt.Printf("%d:%s\n", i, v)
					v = strings.Join(strings.Fields(v), "")
					encodestr := EncodeBulkString(v)
					buf[i] = encodestr
				}
				str := string(EncodeArray(buf))
				*reply = str
				return nil
			} else {
				reArgs.Con = "GET"
				reArgs.Sub = make([]string, 1)
				reArgs.Sub[0] = Getkey
				node := nodePool[rf.currentLeader]
				conn, err := rpc.Dial("tcp", node.IP+":"+node.Port)
				if err != nil {
					*reply = string(EncodeErrorMsg("ERROR"))
					return nil
				}
				ply.Ok = false
				err = conn.Call("Raft.LeaderReceiveMessage", reArgs, &ply)
				Showply(ply)
				if err != nil {
					*reply = string(EncodeErrorMsg("ERROR"))
					return nil
				}
				if ply.Ok {
					if ply.Val != "" {
						KvDataBase[Getkey] = ply.Val
						st := ply.Val
						st = strings.Replace(st, "\n", "", -1)
						st = strings.Replace(st, "\r", "", -1)
						st = strings.Replace(st, "\t", "", -1)
						stArr := strings.Split(st, " ")
						buf := make([][]byte, len(stArr))
						for i, v := range stArr {
							//fmt.Printf("%d:%s\n", i, v)
							v = strings.Join(strings.Fields(v), "")
							encodestr := EncodeBulkString(v)
							buf[i] = encodestr
						}
						str := string(EncodeArray(buf))
						*reply = str
						return nil
					} else {
						*reply = string(EncodeBulkString("nil"))
						return nil
					}
				} else {
					*reply = string(EncodeErrorMsg("ERROR"))
					return nil
				}
			}
		} else {
			*reply = string(EncodeErrorMsg("ERROR"))
			return nil
		}
	default:
		*reply = string(EncodeErrorMsg("ERROR"))
		return nil
	}

	*reply = string(EncodeErrorMsg("ERROR"))

	return nil

}

//返回一个十位数的随机数，作为msg.id
func getRandom() int {
	id := rand.Intn(1000000000) + 1000000000
	return id
}
