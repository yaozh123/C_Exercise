#include<iostream>
using namespace std;

void print(int a[], int n){
    for(int i=0; i<n; i++)
        cout<<a[i] <<" ";
    cout<<endl;
}

void HeapAdjust(int a[], int parent, int length){
    int temp=a[parent];//保存当前父节点
    int child=2*parent+1;//先获得左孩子

    while(child<length){
        if(child+1<length && a[child]<a[child+1]){
            child++;
        }
        if(temp>=a[child])//若父节点的值大于孩子节点的值，则直接结束
            break;
        a[parent]=a[child];
        parent=child;
        child=2*child+1;
    }
    a[parent]=temp;
}

void HeapSort(int a[], int n){
    for(int i=n/2; i>=0; i--)
        HeapAdjust(a, i, n);//初始堆
    for(int i=n-1; i>0; i--){
        swap(a[0], a[i]);
        HeapAdjust(a, 0, i);
    }
}

int main(){
    int s[10]={9,3,0,4,6,5,2,1,8,7};
    cout<<"初始序列：";
    print(s,10);
    HeapSort(s,10);
    cout<<"排序后序列：";
    print(s,10);
}
