#include<iostream>
using namespace std;

void print(int R[], int n){
    for(int i=0; i<n; i++)
        cout<<R[i]<<"  ";
    cout<<endl;
}

int Partition(int R[], int low, int high){
    int x=R[high];//设立枢轴数字
    int i=low-1;//设立i为第一个存储的数字下标
    for(int j=low; j<high; j++){
        if(R[j]<x){
            i++;
            swap(R[i],R[j]);
        }
    }
    R[high]=R[i+1];
    R[i+1]=x;
    return i+1;
}

void QuickSort(int R[], int low, int high){
    if(low<high){
         int q=Partition(R, low, high);
         QuickSort(R, low, q-1);
         QuickSort(R, q+1, high);
    }
}


int main(){
    int s[10]={9,4,2,6,8,0,1,3,5,7};
    cout<<"初始序列：";
    print(s,10);
    QuickSort(s,0,9);
    cout<<"排序后序列：";
    print(s,10);

}
