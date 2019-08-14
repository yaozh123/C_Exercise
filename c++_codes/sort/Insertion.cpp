#include<iostream>
using namespace std;

void print(int a[], int n){
    for(int j=0; j<n; j++){
        cout<<a[j] <<"  ";
    }
    cout<<endl;
}

void InsertionSort(int a[], int n){
    int i,j;
    for(i=1; i<n; i++){
        int temp=a[i];
        for(j=i-1; j>=0; j--){
            if(temp<a[j]){
                a[j+1]=a[j];
            }
            else
                break;
        }
        a[j+1]=temp;
    }
}

int main(){
    int s[10]={2,9,3,4,1,0,7,6,8,5};
    cout<<"初始序列：";
    print(s,10);
    InsertionSort(s,10);
    cout<<"排序后序列：";
    print(s,10);
}
