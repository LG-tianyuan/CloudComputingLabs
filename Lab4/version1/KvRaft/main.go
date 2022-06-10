package main

import (
	"KvRaft/raft"
	"bufio"
	"flag"
	"io"
	"os"
	"strconv"
	"strings"
)

func main() {

	var conf string
	flag.StringVar(&conf, "config_path", "", "eg:./src1/server001.conf")
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
	var st_file string
	var cou_file string
	for {
		//func (b *Reader) ReadString(delim byte) (string, error) {}
		line, err := r.ReadString('\n')
		line = strings.TrimSpace(line)
		lineSplit := strings.Split(line, " ")
		if err != nil && err != io.EOF {
			panic(err)
		}
		if err == io.EOF {
			break
		}
		if line[0] == '!' {
			continue
		} else if lineSplit[0] == "student" {
			st_file = lineSplit[1]
		} else if lineSplit[0] == "course" {
			cou_file = lineSplit[1]
		} else {
			cnt++
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
	go raft.InitKv(st_file, cou_file)
	raft.Start(cnt, ID, nodePool)
}
