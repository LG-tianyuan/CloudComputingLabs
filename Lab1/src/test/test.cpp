// #include <iostream>
// #include <vector>
// #include <queue>
// #include <stdio.h>
// using namespace std;

// char **buf;
// int fill_index=0;
// void put(char* value)
// {
// 	/*for(int i=0; i<81; i++)
//     {
//       buf[fill][i] = value[i];
//     }*/
//     buf[fill_index] = value;
//     fill_index++;
// }

// int main()
// {
// 	buf = (char **)malloc(2 * sizeof(char *));
//     for (int i = 0; i < 2; i++)
//         buf[i] = (char *)malloc(5);
// 	vector<char*> V;
// 	char* a;
// 	a = (char *)malloc(5);
// 	for(int i=0;i<5;i++)
// 	{
// 		a[i]=i+48;
// 	}
// 	V.push_back(a);
// 	V.push_back(a);
// 	for(int i=0;i<2;i++)
// 	{
// 		put(V[i]);
// 	}
// 	for(int i=0;i<2;i++)
// 	{
// 		for(int j=0;j<5;j++)
// 		{
// 			cout<<buf[i][j]<<" ";
// 		}
// 		cout<<endl;
// 	}

// }
// queue<char*> Q;
// void myfunc()
// {
// 	char *a = (char*)malloc(5);
// 	for(int i=0;i<5;i++)
// 	{
// 		a[i]=i;
// 	}
// 	Q.push(a);
// }
// int main()
// {
// 	myfunc();
// 	char *b;
// 	b = Q.front();
// 	Q.pop();
// 	free(b);
// 	cout<<"Great!"<<endl;
// 	return 0;
// }

#include "unistd.h"
#include <stdio.h>
int main()
{
	printf("system cpu num is %ld\n", sysconf( _SC_NPROCESSORS_CONF));
	printf("system enable cpu num is %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
	return 0;
}
	