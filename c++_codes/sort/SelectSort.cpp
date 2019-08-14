#include<iostream>
using namespace std;

void print(int a[], int n){
    for(int i=0; i<n; i++){
        cout<<a[i] <<"  ";
    }
    cout<<endl;
}

void SelectSort(int a[], int n){
    for(int i=0; i<n-1; i++){
        int min=i;
        int j=i+1;
        for(j; j<n; j++){
            if(a[j]<a[min])
                min=j;
        }
        if(min!=i)  //对换到第i个位置
            swap(a[i], a[min]);
    }
}

int main(){
    int s[10]={3,5,4,1,0,9,6,8,5,2};
    cout<<"初始序列：";
    print(s,10);
    SelectSort(s,10);
    cout<<"排序后序列：";
    print(s,10);
}
