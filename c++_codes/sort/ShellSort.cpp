#include<iostream>
using namespace std;

void print(int a[], int n){
    for(int j=0; j<n; j++){
        cout<<a[j] <<"  ";
    }
    cout<<endl;
}

void ShellSort(int a[], int n){
    int gap=n/2;
    while(gap){
        for(int i=gap; i<n; i++){ //根据gap分为几组
            int temp=a[i];
            int j=i;
            while(j>=gap && temp<a[j-gap]){
                a[j]=a[j-gap];
                j-=gap;
            }
            a[j]=temp;//插入
        }//一趟希尔排序
        gap=gap/2;
    }
}

int main(){
    int s[10]={9,4,1,3,2,5,8,6,7,0};
    cout<<"初始序列：";
    print(s,10);
    ShellSort(s,10);
    cout<<"排序后序列：";
    print(s,10);
}



