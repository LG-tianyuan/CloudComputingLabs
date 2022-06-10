/*
 *@Last-update:2022-05-30
 */

package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"net/rpc"
	"os"
	"strings"

	"Client/resp"
)

func main() {
	var conf string
	flag.StringVar(&conf, "config_path", "", "./config/server001.conf")
	flag.Parse()
	fi, err := os.Open(conf)
	if err != nil {
		panic(err)
	}
	r := bufio.NewReader(fi)
	var nodePool []string
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
			lineSplit := strings.Split(line, " ")
			nodePool = append(nodePool, lineSplit[1])
		}
	}
	for {
		inputReader := bufio.NewReader(os.Stdin)
		st, err := inputReader.ReadString('\n')
		if err != nil {
			log.Fatal("Input Error!")
		}
		st = strings.Replace(st, "\n", "", -1)
		st = strings.Replace(st, "\r", "", -1)
		st = strings.Replace(st, "\t", "", -1)
		//fmt.Printf("original:%s\n", st)
		stArr := strings.Split(st, " ")
		buf := make([][]byte, len(stArr))
		for i, v := range stArr {
			//fmt.Printf("%d:%s\n", i, v)
			v = strings.Join(strings.Fields(v), "")
			if i == 0 {
				if v != "DEL" && v != "GET" && v != "SET" {
					log.Fatal("Instruct Error!")
				} else {
					encodestr := resp.EncodeBulkString(v)
					buf[i] = encodestr
				}
			} else {
				encodestr := resp.EncodeBulkString(v)
				buf[i] = encodestr
			}
		}
		str := string(resp.EncodeArray(buf))
		i := 0
		cnt := 0
		for {
			if cnt >= 4 {
				fmt.Printf("KV servers Break Down!!\n")
				return
			}
			cl, err := rpc.Dial("tcp", nodePool[i])
			if err != nil {
				i = (i + 1) % 4
				cnt++
				continue
			}
			var rep string
			err = cl.Call("Raft.DealRequest", str, &rep)
			if err != nil {
				i = (i + 1) % 4
				cnt++
				continue
			}
			cnt = 0
			reader := bufio.NewReader(strings.NewReader(rep))
			res, _ := resp.Decode(reader)
			fmt.Println("Reply:", res)
			// fmt.Printf("Reply: %s\n", res)
			break
		}
	}
}
