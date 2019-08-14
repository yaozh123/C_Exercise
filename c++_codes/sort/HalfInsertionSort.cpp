#include<iostream>
using namespace std;

void print(int a[], int n){
    for(int j=0; j<n; j++){
        cout<<a[j] <<"  ";
    }
    cout<<endl;
}

void HalfInsertionSort(int a[], int n){
    int left,right;
    for(int i=1; i<n; i++){ //插入n-1次
        left=0, right=i-1;
        int temp=a[i];//确定待插入元素
        while(left<=right){
            int middle=(left+right)/2;
            if(temp<a[middle])
                right=middle-1;
            else
                left=middle+1;
        }//折半查找插入位置
        for(int j=i-1; j>=left; j--){
            a[j+1]=a[j];
        }
        a[left]=temp; 
    }
}

int main(){
    int s[10]={9,3,8,2,5,1,6,4,7,0};
    cout<<"初始序列：";
    print(s,10);
    HalfInsertionSort(s,10);
    cout<<"排序后序列：";
    print(s,10);
}
