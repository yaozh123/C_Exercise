#include<iostream>
using namespace std;

void print(int arr[], int n){
    for(int j=0; j<n; j++)
        cout<<arr[j] <<"  ";
    cout<<endl;
}

//void BubbleSort(int arr[], int n){
//    for(int i=0; i<n-1; i++){
//        for(int j=0; j<n-i-1; j++){
//            if(arr[j]>arr[j+1])
//              swap(arr[j], arr[j+1]);
//              }
//    }

//}

void BubbleSort(int R[], int n){
    int i=1;
    int NoSwap=0;
    while(i<n && !NoSwap){
        NoSwap=1;
        for(int j=n-1; j>=i; j--){
            if(R[j-1]>R[j]){
                swap(R[j-1], R[j]);
                NoSwap=0;
            }
        }
    i++;
    }
}

int main(){
    int s[10]={8,1,9,7,2,4,9,6,1,0};
    cout<<"初始序列：";
    print(s,10);
    BubbleSort(s,10);
    cout<<"排序结果：";
    print(s,10);

}

