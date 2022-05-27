#include <string.h>
#include <vector>
#include <stdio.h>
using namespace std;
const char typeArrays = '*';
const char typeBulkStrings = '$';
const char typeSuccessMsgs = '+';
const char typeErrorMsgs = '-';
const char typeIntegers = ':';
const std::string CRLF = "\r\n";
const long long BSMaxLength = 512*1024*1024;

std::string EncodeSuccessMsg(std::string s){
	if(s.find(CRLF)!=std::string::npos){
		printf("String can not contain a CR or LF character!\n");
		exit(0);
	}
	std::string str(1,typeSuccessMsgs);
	return str+s+CRLF;
}

std::string EncodeErrorMsg(std::string s){
	if(s.find(CRLF)!=std::string::npos){
		printf("String can not contain a CR or LF character!\n");
		exit(0);
	}
	std::string str(1,typeErrorMsgs);
	return str+s+CRLF;
}

std::string EncodeInteger(int s){
	std::string str(1,typeIntegers);
	return  str+to_string(s)+CRLF;
}

std::string EncodeBulkString(std::string s){
	if(sizeof(s)>BSMaxLength){
		printf("The bulk string is over 512MB!\n");
		exit(0);
	}
	std::string str(1,typeBulkStrings);
	return str+to_string(s.length())+CRLF+s+CRLF;
}

std::string EncodeArray(std::string s){
	std::string str(1,typeArrays);
	int len = s.length();
	size_t index1=0,index2 = s.find(" ");
	std::vector<std::string> v;
	while(index2!=std::string::npos){
		std::string temp = s.substr(index1,index2-index1);
		size_t i = temp.find("\"");
		if(i!=std::string::npos){
			if(i==0){
				temp = temp.substr(1,temp.length()-1);
			}else{
				temp = temp.substr(0,temp.length()-1);
			}
		}
		v.push_back(temp);
		index2 += 1;
		index1 = index2;
		index2 = s.find(" ",index2);
	}
	if(s[len-1]=='\"'){
		v.push_back(s.substr(index1,len-index1-1));
	}else{
		v.push_back(s.substr(index1,len-index1));
	}
	str += to_string(v.size())+CRLF;
	for(unsigned long i = 0 ; i < v.size() ; i++){
		str += EncodeBulkString(v[i]);
	}
	return str;
}

std::string DecodeSuccessMsg(std::string s){
	std::string res;
	int index = s.find(CRLF);
	res = s.substr(1,index-1);
	return res;
}

std::string DecodeErrorMsg(std::string s){
	std::string res;
	int index = s.find(CRLF);
	res = s.substr(1,index-1);
	return res;
}

int DecodeInteger(std::string s){
	std::string res;
	int index = s.find(CRLF);
	res = s.substr(1,index-1);
	return stoi(res);
}

std::string DecodeBulkString(std::string s){
	std::string res;
	int index = s.find(CRLF);
	res = s.substr(1,index-1);
	int len = stoi(res);
	res = s.substr(index+2,len);
	return res;
}

std::string DecodeArrays(std::string s){
	std::string res="";
	std::string temp;
	size_t index = s.find(CRLF);
	temp = s.substr(1,index-1);
	int len = stoi(temp);
	index += 2;
	for(int i=0; i<len; i++){
		size_t index_ = s.find("$",index+1);
		if(index_==std::string::npos){
            index_=s.length();
		}
		temp = s.substr(index,index_-index);
		temp = DecodeBulkString(temp);
		res += temp;
		index = index_;
		if(i!=len-1) res+=" ";
	}
	return res;
}