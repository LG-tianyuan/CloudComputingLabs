package main

import (
	"bufio"
	"flag"
	"io"
	"os"
	"strconv"
	"strings"

	"Raft/raft"
)

func main() {

	var conf string
	flag.StringVar(&conf, "config_path", "", "./server.conf")
	flag.Parse()
	fi, err := os.Open(conf)
	if err != nil {
		panic(err)
	}

	// 创建 Reader
	r := bufio.NewReader(fi)
	nodePool := make(map[string]raft.NodeInfo)
	cnt := 0
	var ID string
	for {
		//func (b *Reader) ReadString(delim byte) (string, error) {}
		line, err := r.ReadString('\n')
		line = strings.TrimSpace(line)
		if err != nil && err != io.EOF {
			panic(err)
		}
		if err == io.EOF {
			break
		}
		if line[0] == '!' {
			continue
		} else {
			cnt++
			lineSplit := strings.Split(line, " ")
			addr := strings.Split(lineSplit[1], ":")
			port, _ := strconv.Atoi(addr[1])
			port -= 2000
			var node raft.NodeInfo
			node.ID = strconv.Itoa(port)
			node.Port = strconv.Itoa(port)
			node.IP = addr[0]
			if lineSplit[0] == "self_info" {
				ID = node.ID
			}
			nodePool[node.ID] = node
		}
	}
	raft.Start(cnt, ID, nodePool)
}
