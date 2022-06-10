/*
 *@References:github.com/teambition/respgo
 *@Author:LG.tianyuan
 *@Date:2022-05-11
 */

package resp

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"io"
	"strconv"
	"strings"
)

//定义常量,5种类型的resp报文标识
const (
	typeArrays      = "*"
	typeBulkStrings = "$"
	typeSuccessMsgs = "+"
	typeErrorMsgs   = "-"
	typeIntegers    = ":"
	CRLF            = "\r\n"
	BSMaxLength     = 512 * 1024 * 1024
)

//Encode
func EncodeSuccessMsg(s string) []byte {
	if strings.ContainsAny(s, "\r\n") {
		panic("String can not contain a CR or LF character")
	}
	return []byte(typeSuccessMsgs + s + CRLF)
}

func EncodeErrorMsg(s string) []byte {
	if strings.ContainsAny(s, "\r\n") {
		panic("String can not contain a CR or LF character")
	}
	return []byte(typeErrorMsgs + s + CRLF)
}

func EncodeInteger(s int64) []byte {
	return []byte(typeIntegers + strconv.FormatInt(s, 10) + CRLF)
}

func EncodeBulkString(s string) []byte {
	if len(s) > BSMaxLength {
		panic("The bulk string is over 512MB!")
	}
	return []byte(typeBulkStrings + strconv.Itoa(len(s)) + CRLF + s + CRLF)
}

func EncodeArray(s [][]byte) []byte {
	var buf bytes.Buffer
	buf.WriteString(typeArrays)
	buf.WriteString(strconv.Itoa(len(s)))
	buf.WriteString(CRLF)
	for _, val := range s {
		buf.Write(val)
	}
	return buf.Bytes()
}

//decode from reader
func Decode(reader *bufio.Reader) (result interface{}, err error) {
	//无io错误时err为nil
	line, err := reader.ReadString('\n')
	if err != nil {
		return
	}
	lineLen := len(line)
	if lineLen < 3 {
		err = fmt.Errorf("line is too short: %#v", line)
		return
	}
	if line[lineLen-2] != '\r' || line[lineLen-1] != '\n' {
		err = fmt.Errorf("invalid CRLF: %#v", line)
		return
	}
	//获取报文标识和报文
	MsgType, line := string(line[0]), line[1:lineLen-2]
	switch MsgType {
	case typeSuccessMsgs:
		result = line
	case typeErrorMsgs:
		result = errors.New(line)
	case typeIntegers:
		result, err = strconv.ParseInt(line, 10, 64) //十进制64位
	case typeBulkStrings:
		var length int
		//Atoi：形成整数的字符后面还有字符则自动忽略，无影响
		//获取bulk string的长度
		length, err = strconv.Atoi(line)
		if err != nil {
			return
		}
		if length == -1 {
			return
		}
		if length > BSMaxLength || length < -1 {
			err = fmt.Errorf("invalid bulk strings length: %#v", length)
			return
		}
		buff := make([]byte, length+2)
		_, err = io.ReadFull(reader, buff)
		if err != nil {
			return
		}
		if buff[length] != '\r' || buff[length+1] != '\n' {
			err = fmt.Errorf("invalid CRLF: %#v", string(buff))
			return
		}
		result = string(buff[:length])
	case typeArrays:
		var length int
		//获取array的长度
		length, err = strconv.Atoi(line)
		if length == -1 {
			return
		}
		array := make([]interface{}, length)
		for i := 0; i < length; i++ {
			array[i], err = Decode(reader)
			if err != nil {
				return
			}
		}
		result = array
	default:
		err = fmt.Errorf("invalid RESP type: %#v", MsgType)
	}
	return
}
