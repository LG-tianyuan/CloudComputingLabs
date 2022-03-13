#include <iostream>
#include <vector>
#include <stdio.h>
using namespace std;

char **buf;
int fill_index=0;
void put(char* value)
{
	/*for(int i=0; i<81; i++)
    {
      buf[fill][i] = value[i];
    }*/
    buf[fill_index] = value;
    fill_index++;
}

int main()
{
	buf = (char **)malloc(2 * sizeof(char *));
    for (int i = 0; i < 2; i++)
        buf[i] = (char *)malloc(5);
	vector<char*> V;
	char* a;
	a = (char *)malloc(5);
	for(int i=0;i<5;i++)
	{
		a[i]=i+48;
	}
	V.push_back(a);
	V.push_back(a);
	for(int i=0;i<2;i++)
	{
		put(V[i]);
	}
	for(int i=0;i<2;i++)
	{
		for(int j=0;j<5;j++)
		{
			cout<<buf[i][j]<<" ";
		}
		cout<<endl;
	}

}